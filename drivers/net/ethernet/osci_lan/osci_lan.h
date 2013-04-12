/*******************************************************************************

  OSCI LAN Network Linux driver based on EZchip Network Linux driver

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

#ifndef _OSCI_LAN_H
#define _OSCI_LAN_H

/* driver status definitions  */
#define	OSCI_LAN_DEV_OFF		0
#define	OSCI_LAN_DEV_ON			1
#define	OSCI_LAN_BROADCAST_MODE		0
#define	OSCI_LAN_PROMISC_MODE		1

/* driver global definitions*/
#define OSCI_LAN_ETH_PKT_SZ		(ETH_FRAME_LEN + 20)
#define OSCI_LAN_LAN_BUFFER_SIZE	0x600 /* 1.5K */
#define OSCI_LAN_REG_SIZE		4

/* GEMAC config register definitions */
#define GEMAC_CFG_INIT_VALUE		0x70C8501F

/* TX CTL register definitions */
#define TX_CTL_BUSY			0x00008000
#define TX_CTL_ERROR			0x00004000
#define TX_CTL_INT_ENA			0x00001000
#define TX_CTL_RESET			0x00000800
#define TX_CTL_LENGTH_MASK		0x000007FF

/* RX CTL register definitions */
#define RX_CTL_BUSY			0x00008000
#define RX_CTL_ERROR			0x00004000
#define RX_CTL_CRC			0x00002000
#define RX_CTL_INT_ENA			0x00001000
#define RX_CTL_RESET			0x00000800
#define RX_CTL_LENGTH_MASK		0x000007FF

/* LAN register definitions  */
struct osci_lan_regs {
	uint32_t	__iomem tx_ctl;
	uint32_t	__iomem txbuf_sts;
	uint32_t	__iomem txbuf_data;
	uint32_t	__iomem pad0;
	uint32_t	__iomem rx_ctl;
	uint32_t	__iomem rxbuf_sts;
	uint32_t	__iomem rxbuf_data;
	uint32_t	__iomem pad1;
	uint32_t	__iomem pad2[8];
	uint32_t	__iomem mac_cfg;
};

/* driver private data structure */
struct osci_lan_priv {
	int			msg_enable;
	int			status; /* OSCI_LAN_DEV_OFF/OSCI_LAN_DEV_ON */
	unsigned char		address[ETH_ALEN]; /* mac address */
	int			down_event;
	int			packet_len;
	int			rx_mode;
	struct osci_lan_regs	*regs;
};

#endif /* _OSCI_LAN_H */
