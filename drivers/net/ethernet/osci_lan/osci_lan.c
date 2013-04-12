/******************************************************************************

  Network driver for OSCI Ethernet based on earlier work by EZchip

  originally by Tal Zilcer <talz@ezchip.com>

  modified to use device tree and use non-BE OSCI implementation by
  Mischa Jonker <mjonker@synopsys.com>

  EZchip Network Linux driver
  Copyright(c) 2012 EZchip Technologies.

  This program is free software; you can redistribute it and/or modify it
  under the terms and conditions of the GNU General Public License,
  version 2, as published by the Free Software Foundation.

  This program is distributed in the hope it will be useful, but WITHOUT
  ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
  FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
  more details.

  The full GNU General Public License is included in this distribution in
  the file called "COPYING".

*******************************************************************************/

#include <linux/platform_device.h>
#include <linux/etherdevice.h>
#include <linux/netdevice.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/module.h>
#include <linux/of.h>
#include "osci_lan.h"

static struct sockaddr mac_addr = { 0, {0x84, 0x66, 0x46, 0x88, 0x63, 0x33} };

static int osci_lan_set_mac_address(struct net_device *netdev, void *p)
{
	struct osci_lan_priv *npriv = netdev_priv(netdev);
	struct sockaddr *x = (struct sockaddr *)p;
	char *uc_addr = (char *)x->sa_data;
	int res;

	netif_info(npriv, hw, netdev,
			"new MAC addr %02x:%02x:%02x:%02x:%02x:%02x\n",
			uc_addr[0]&0x0FF, uc_addr[1]&0x0FF, uc_addr[2]&0x0FF,
			uc_addr[3]&0x0FF, uc_addr[4]&0x0FF, uc_addr[5]&0x0FF);

	res = eth_mac_addr(netdev, p);
	if (!res)
		memcpy(npriv->address, uc_addr, ETH_ALEN);

	return res;
}

static void osci_lan_hw_enable_control(struct net_device *netdev)
{
	struct osci_lan_priv *npriv = netdev_priv(netdev);
	iowrite32(ioread32(&npriv->regs->tx_ctl) & ~TX_CTL_RESET,
		  &npriv->regs->tx_ctl);
	iowrite32(ioread32(&npriv->regs->rx_ctl) & ~RX_CTL_RESET,
		  &npriv->regs->rx_ctl);
}

static void osci_lan_hw_disable_control(struct net_device *netdev)
{
	struct osci_lan_priv *npriv = netdev_priv(netdev);
	iowrite32(ioread32(&npriv->regs->tx_ctl) | TX_CTL_RESET,
		  &npriv->regs->tx_ctl);
	iowrite32(ioread32(&npriv->regs->rx_ctl) | RX_CTL_RESET,
		  &npriv->regs->rx_ctl);
}

static void osci_lan_send_buf(struct net_device *netdev,
				struct sk_buff *skb, short length)
{
	struct osci_lan_priv *npriv = netdev_priv(netdev);
	unsigned int i, ctrl;
	unsigned char *src_data = (unsigned char *)virt_to_phys(skb->data);

	for (i = 0; i < length; i++)
		iowrite32(src_data[i], &npriv->regs->txbuf_data);

	/* Write the length of the Frame */
	ctrl = ioread32(&npriv->regs->tx_ctl);
	ctrl = (ctrl & ~(TX_CTL_LENGTH_MASK)) | length;

	/* Send Frame */
	ctrl |= TX_CTL_BUSY | TX_CTL_INT_ENA;
	iowrite32(ctrl, &npriv->regs->tx_ctl);
}

static int osci_lan_open(struct net_device *netdev)
{
	struct osci_lan_priv *npriv = netdev_priv(netdev);

	if (npriv->status != OSCI_LAN_DEV_OFF)
		return -EAGAIN;

	osci_lan_hw_enable_control(netdev);

	npriv->status = OSCI_LAN_DEV_ON;

	netif_start_queue(netdev);

	return 0;
}

static int osci_lan_stop(struct net_device *netdev)
{
	struct osci_lan_priv *npriv = netdev_priv(netdev);

	netif_stop_queue(netdev);

	osci_lan_hw_disable_control(netdev);

	if (npriv->status != OSCI_LAN_DEV_OFF) {
		npriv->status = OSCI_LAN_DEV_OFF;
		npriv->down_event++;
	}
	return 0;
}

static netdev_tx_t osci_lan_start_xmit(struct sk_buff *skb,
					struct net_device *netdev)
{
	struct osci_lan_priv *npriv = netdev_priv(netdev);
	short length = skb->len;

	if (skb->len < ETH_ZLEN) {
		if (unlikely(skb_padto(skb, ETH_ZLEN) != 0))
			return NETDEV_TX_OK;
		length = ETH_ZLEN;
	}

	npriv->packet_len = length;
	osci_lan_send_buf(netdev, skb, length);

	dev_kfree_skb(skb);

	/* This driver handles one frame at a time  */
	netif_stop_queue(netdev);

	return NETDEV_TX_OK;
}

static struct net_device_stats *osci_lan_get_stats(struct net_device *netdev)
{
	return &netdev->stats;
}

static int osci_lan_change_mtu(struct net_device *netdev, int new_mtu)
{
	struct osci_lan_priv *npriv = netdev_priv(netdev);

	netif_printk(npriv, hw, KERN_INFO, netdev ,
		   "mtu change from %d to %d\n",
		   (int)netdev->mtu, new_mtu);

	if ((new_mtu < ETH_ZLEN) || (new_mtu > OSCI_LAN_ETH_PKT_SZ))
		return -EINVAL;

	netdev->mtu = new_mtu;

	return 0;
}

static void osci_lan_set_rx_mode(struct net_device *netdev)
{
	struct osci_lan_priv *npriv = netdev_priv(netdev);

	if (netdev->flags & IFF_PROMISC)
		npriv->rx_mode = OSCI_LAN_PROMISC_MODE;
	else
		npriv->rx_mode = OSCI_LAN_BROADCAST_MODE;

	return;
}

static int osci_lan_do_ioctl(struct net_device *netdev,
				 struct ifreq *ifr, int cmd)
{
	switch (cmd) {
	case SIOCSIFTXQLEN:
		if (ifr->ifr_qlen < 0)
			return -EINVAL;

		netdev->tx_queue_len = ifr->ifr_qlen;
		break;

	case SIOCGIFTXQLEN:
		ifr->ifr_qlen = netdev->tx_queue_len;
		break;

	case SIOCSIFFLAGS:      /* Set interface flags */
		return dev_change_flags(netdev, ifr->ifr_flags);

	case SIOCGIFFLAGS:      /* Get interface flags */
		ifr->ifr_flags = (short) dev_get_flags(netdev);
		break;

	default:
		return -EOPNOTSUPP;
	}
	return 0;
}

static void osci_lan_tx_timeout(struct net_device *netdev)
{
	struct osci_lan_priv *npriv = netdev_priv(netdev);

	netif_wake_queue(netdev);

	netif_printk(npriv, tx_err, KERN_DEBUG, netdev,
		     "transmission timed out\n");
}

static const struct net_device_ops nps_netdev_ops = {
	.ndo_open		= osci_lan_open,
	.ndo_stop		= osci_lan_stop,
	.ndo_start_xmit		= osci_lan_start_xmit,
	.ndo_get_stats		= osci_lan_get_stats,
	.ndo_set_mac_address	= osci_lan_set_mac_address,
	.ndo_set_rx_mode	= osci_lan_set_rx_mode,
	.ndo_change_mtu		= osci_lan_change_mtu,
	.ndo_do_ioctl		= osci_lan_do_ioctl,
	.ndo_tx_timeout		= osci_lan_tx_timeout,

};

static void clean_rx_fifo(struct net_device *netdev, int frame_len)
{
	struct osci_lan_priv *npriv = netdev_priv(netdev);
	int i, dummy;

	for (i = 0; i < frame_len; i++)
		dummy = ioread32(&npriv->regs->rxbuf_data);
}

static void read_rx_fifo(struct net_device *netdev, unsigned char *dest,
			 int length)
{
	struct osci_lan_priv *npriv = netdev_priv(netdev);
	int i;

	for (i = 0; i < length; i++)
		dest[i] = (char) ioread32(&npriv->regs->rxbuf_data);
}

static bool is_valid_enter_frame(struct net_device *netdev, char *buf_recv)
{
	char mac_broadcast[ETH_ALEN] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
	struct osci_lan_priv *npriv = netdev_priv(netdev);

	/* Check destination MAC address */
	if (memcmp(buf_recv, npriv->address, ETH_ALEN) == 0)
		return true;

	/* Value 0xFF,0xFF,0xFF,0xFF,0xFF,0xFF for broadcast */
	if (memcmp(buf_recv, mac_broadcast, ETH_ALEN) == 0)
		return true;

	/* Promisc mode */
	if (npriv->rx_mode == OSCI_LAN_PROMISC_MODE)
		return true;

	return false;
}


static irqreturn_t osci_lan_rx_irq(int irq, void *dev_id)
{
	struct net_device *netdev;
	struct osci_lan_priv *npriv;
	struct sk_buff *skb;
	int cur_rx_ctl;
	int frame_len;

	netdev = dev_id;
	if (unlikely(!netdev))
		return IRQ_NONE;

	npriv = netdev_priv(netdev);
	cur_rx_ctl = ioread32(&npriv->regs->rx_ctl);
	frame_len = cur_rx_ctl & RX_CTL_LENGTH_MASK;

	/* Check that the hardware finished updating the ctrl
	 * in slow hardware this will prevent race conditions on the ctrl
	 */
	if (!(cur_rx_ctl & RX_CTL_BUSY))
		goto rx_irq_finish;

	/* Check RX error */
	if (cur_rx_ctl & RX_CTL_ERROR) {
		netdev->stats.rx_errors++;
		goto rx_irq_error;
	}

	/* Check RX crc error */
	if (cur_rx_ctl & RX_CTL_CRC) {
		netdev->stats.rx_crc_errors++;
		netdev->stats.rx_dropped++;
		goto rx_irq_error;
	}

	/* Check Frame length (MAX 1.5k Min 64b) */
	if (unlikely(frame_len > OSCI_LAN_LAN_BUFFER_SIZE
			|| frame_len < ETH_ZLEN)) {
		netdev->stats.rx_dropped++;
		goto rx_irq_error;
	}

	skb = netdev_alloc_skb_ip_align(netdev, (frame_len + 32));

	/* Check skb allocation */
	if (unlikely(skb == NULL)) {
		netdev->stats.rx_errors++;
		netdev->stats.rx_dropped++;
		goto rx_irq_error;
	}

	read_rx_fifo(netdev, (unsigned char *)skb->data, frame_len);

	if (is_valid_enter_frame(netdev, skb->data) == false) {
		netdev->stats.rx_dropped++;
		dev_kfree_skb_irq(skb);
	} else {
		skb_put(skb, frame_len);
		skb->dev = netdev;
		skb->protocol = eth_type_trans(skb, netdev);
		skb->ip_summed = CHECKSUM_UNNECESSARY;
		netif_rx(skb);
		netdev->stats.rx_packets++;
		netdev->stats.rx_bytes += frame_len;
	}
	goto rx_irq_frame_done;

rx_irq_error:
	/* Clean RX fifo */
	clean_rx_fifo(netdev, frame_len);

rx_irq_frame_done:
	/* Clean RX CTL register */
	iowrite32(RX_CTL_INT_ENA, &npriv->regs->rx_ctl);

rx_irq_finish:
	return IRQ_HANDLED;
}

static irqreturn_t osci_lan_tx_irq(int irq, void *dev_id)
{
	struct net_device *netdev = dev_id;
	struct osci_lan_priv *npriv = netdev_priv(netdev);

	if (unlikely(!netdev || npriv->status == OSCI_LAN_DEV_OFF))
		return IRQ_NONE;

	/* Check Tx error */
	if (unlikely(ioread32(&npriv->regs->tx_ctl) & TX_CTL_ERROR)) {
		netdev->stats.tx_errors++;
		/* clean TX CTL error */
		iowrite32(ioread32(&npriv->regs->tx_ctl) & ~TX_CTL_ERROR,
			  &npriv->regs->tx_ctl);
	} else {
		netdev->stats.tx_packets++;
		netdev->stats.tx_bytes += npriv->packet_len;
	}

	iowrite32(ioread32(&npriv->regs->tx_ctl) & ~TX_CTL_INT_ENA,
		  &npriv->regs->tx_ctl);

	/* In osci_lan_start_xmit we disabled sending frames*/
	netif_wake_queue(netdev);

	return IRQ_HANDLED;
}

static int osci_lan_probe(struct platform_device *pdev)
{
	struct net_device *netdev;
	struct resource *res;
	struct osci_lan_priv *npriv;
	struct osci_lan_regs *addr;
	int err, irq_rx, irq_tx;

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res) {
		dev_err(&pdev->dev, "no IO memory defined\n");
		return -EINVAL;
	}

	irq_rx = platform_get_irq_byname(pdev, "rx");
	if (irq_rx < 0) {
		dev_err(&pdev->dev, "no RX IRQ defined\n");
		return -EINVAL;
	}

	irq_tx = platform_get_irq_byname(pdev, "tx");
	if (irq_tx < 0) {
		dev_err(&pdev->dev, "no TX IRQ defined\n");
		return -EINVAL;
	}

	/* FIXME: needs to be changed to devm_ioremap_resource once
	 * we move to a newer kernel
	 */
	/* addr = devm_ioremap_resource(&pdev->dev, res); */
	addr = devm_request_and_ioremap(&pdev->dev, res);
	if (IS_ERR(addr)) {
		dev_err(&pdev->dev, "Could not remap IO mem\n");
		return PTR_ERR(addr);
	}

	netdev = (struct net_device *) alloc_etherdev(sizeof(
						      struct osci_lan_priv));
	if (!netdev) {
		dev_err(&pdev->dev, "Could not allocate netdev\n");
		return -ENOMEM;
	}

	npriv = netdev_priv(netdev);
	npriv->regs = addr;

	/* The OSCI LAN specific entries in the device structure. */
	netdev->netdev_ops = &nps_netdev_ops;
	netdev->watchdog_timeo = 5 * HZ;
	netdev->ml_priv = npriv;

	/* initialize driver private data structure. */
	npriv->status = OSCI_LAN_DEV_OFF;
	npriv->msg_enable = 1; /* enable messages for netif_printk */

	netif_info(npriv, hw, netdev, "%s: port %s\n",
			 __func__, netdev->name);

	/* config GEMAC register */
	iowrite32(GEMAC_CFG_INIT_VALUE, &npriv->regs->mac_cfg);
	iowrite32(RX_CTL_INT_ENA, &npriv->regs->rx_ctl);
	osci_lan_hw_disable_control(netdev);

	/* set kernel MAC address to dev */
	osci_lan_set_mac_address(netdev, &mac_addr);

	/* irq Rx allocation */
	err = devm_request_irq(&pdev->dev, irq_rx, osci_lan_rx_irq,
			       0, "eth-rx", netdev);
	if (err) {
		netif_err(npriv, probe, netdev,
			  "OSCI LAN: Fail to allocate IRQ %d - err %d\n",
			  irq_rx, err);
		goto osci_lan_probe_error;
	}

	/* irq Tx allocation */
	err = devm_request_irq(&pdev->dev, irq_tx, osci_lan_tx_irq,
			       0, "eth-tx", netdev);
	if (err) {
		netif_err(npriv, probe, netdev,
			"OSCI LAN: Fail to allocate IRQ %d - err %d\n",
			irq_tx, err);
		goto osci_lan_probe_error;

	}

	/* We don't support MULTICAST */
	netdev->flags &= ~IFF_MULTICAST;

	/* Register the driver
	 * Should be the last thing in probe
	 */
	err = register_netdev(netdev);
	if (err != 0) {
		netif_err(npriv, probe, netdev,
			"%s: Failed to register netdev for %s, err = 0x%08x\n",
			__func__, netdev->name, (int)err);
		err = -ENODEV;
		goto osci_lan_probe_error;
	}
	return 0;

osci_lan_probe_error:
	free_netdev(netdev);
	return err;
}

static int osci_lan_remove(struct platform_device *pdev)
{
	struct net_device *netdev;
	struct osci_lan_priv *npriv;

	netdev = platform_get_drvdata(pdev);

	unregister_netdev(netdev);

	npriv = netdev_priv(netdev);

	osci_lan_hw_disable_control(netdev);

	free_netdev(netdev);

	return 0;
}

#ifdef CONFIG_OF
static const struct of_device_id osci_lan_match[] = {
	{ .compatible = "snps,oscilan" },
	{ },
};
MODULE_DEVICE_TABLE(of, osci_lan_match);
#endif

static struct platform_driver osci_lan_driver = {
	.probe = osci_lan_probe,
	.remove = osci_lan_remove,
	.driver = {
		.owner = THIS_MODULE,
		.name = "oscilan",
		.of_match_table = of_match_ptr(osci_lan_match),
	},
};

module_platform_driver(osci_lan_driver);

MODULE_LICENSE("GPL");
