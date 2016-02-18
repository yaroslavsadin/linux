/**
 * Analog Devices ADV7511 HDMI transmitter driver
 *
 * Copyright 2012 Analog Devices Inc.
 *
 * Licensed under the GPL-2.
 */

#include <linux/slab.h>
#include <linux/module.h>
#include <linux/i2c.h>
#include <linux/regmap.h>
#include <linux/interrupt.h>
#include <linux/delay.h>

#include "adv7511.h"

/* ADI recommanded values for proper operation. */
static const struct reg_default adv7511_fixed_registers[] = {
	{ 0x98, 0x03 },
	{ 0x9a, 0xe0 },
	{ 0x9c, 0x30 },
	{ 0x9d, 0x61 },
	{ 0xa2, 0xa4 },
	{ 0xa3, 0xa4 },
	{ 0xe0, 0xd0 },
	{ 0xf9, 0x00 },
	{ 0x55, 0x02 },
	{ 0xd6, 0xc0 }, /* MJ ignore HPD pin */
};


#define ADV7511_HDMI_CFG_MODE_MASK	0x2
#define ADV7511_HDMI_CFG_MODE_DVI	0x0
#define ADV7511_HDMI_CFG_MODE_HDMI	0x2
#define ADV7511_ASPECT_RATIO_MASK	0x2
#define ADV7511_ASPECT_RATIO_16_9	0x2
#define ADV7511_ASPECT_RATIO_4_3	0x0

#define ADV7511_HPD_HIGH		0x40
#define ADV7511_MONITOR_CONNECT		0x20
#define ADV7511_HPD_MONITOR_MASK	0x60

#define ADV7511_PACKET_MEM_SPD		0
#define ADV7511_PACKET_MEM_MPEG		1
#define ADV7511_PACKET_MEM_ACP		2
#define ADV7511_PACKET_MEM_ISRC1	3
#define ADV7511_PACKET_MEM_ISRC2	4
#define ADV7511_PACKET_MEM_GM		5
#define ADV7511_PACKET_MEM_SPARE1	6
#define ADV7511_PACKET_MEM_SPARE2	7

#define ADV7511_PACKET_MEM_DATA_REG(x) ((x) * 0x20)
#define ADV7511_PACKET_MEM_UPDATE_REG(x) ((x) * 0x20 + 0x1f)
#define ADV7511_PACKET_MEM_UPDATE_ENABLE BIT(7)


/**
* @brief enable packet sending
*
* @param adv7511
* @param packet
*/
int adv7511_packet_enable(struct adv7511 *adv7511, unsigned int packet)
{
	if (packet & 0xff)
		regmap_update_bits(adv7511->regmap, ADV7511_REG_PACKET_ENABLE0,
				   packet, 0xff);

	if (packet & 0xff00) {
		packet >>= 8;
		regmap_update_bits(adv7511->regmap, ADV7511_REG_PACKET_ENABLE1,
				   packet, 0xff);
	}

	return 0;
}

/**
* @brief disable packet sending
*
* @param adv7511
* @param packet
*/
int adv7511_packet_disable(struct adv7511 *adv7511, unsigned int packet)
{
	if (packet & 0xff)
		regmap_update_bits(adv7511->regmap, ADV7511_REG_PACKET_ENABLE0,
				   packet, 0x00);

	if (packet & 0xff00) {
		packet >>= 8;
		regmap_update_bits(adv7511->regmap, ADV7511_REG_PACKET_ENABLE1,
				   packet, 0x00);
	}

	return 0;
}

/**
* @brief set link parameters
*
* @param adv7511
* @param config
*/
static void adv7511_set_link_config(struct adv7511 *adv7511,
	const struct adv7511_link_config *config)
{
	unsigned int val;
	enum adv7511_input_sync_pulse sync_pulse;

	switch (config->id) {
	case ADV7511_INPUT_ID_12_15_16BIT_RGB444_YCbCr444:
		sync_pulse = ADV7511_INPUT_SYNC_PULSE_NONE;
		break;
	default:
		sync_pulse = config->sync_pulse;
		break;
	}

	switch (config->id) {
	case ADV7511_INPUT_ID_16_20_24BIT_YCbCr422_EMBEDDED_SYNC:
	case ADV7511_INPUT_ID_8_10_12BIT_YCbCr422_EMBEDDED_SYNC:
		adv7511->embedded_sync = true;
		break;
	default:
		adv7511->embedded_sync = false;
		break;
	}

	regmap_update_bits(adv7511->regmap, ADV7511_REG_I2C_FREQ_ID_CFG,
		0xf, config->id);
	regmap_update_bits(adv7511->regmap, ADV7511_REG_VIDEO_INPUT_CFG1, 0x7e,
		(config->input_color_depth << 4) | (config->input_style << 2));
	regmap_write(adv7511->regmap, ADV7511_REG_VIDEO_INPUT_CFG2,
		(config->reverse_bitorder << 6) |
		(config->bit_justification << 3));
	regmap_write(adv7511->regmap, ADV7511_REG_TIMING_GEN_SEQ,
		(sync_pulse << 2) |
		(config->timing_gen_seq << 1));
	regmap_write(adv7511->regmap, 0xba,
		(config->clock_delay << 5));

	regmap_update_bits(adv7511->regmap, ADV7511_REG_TMDS_CLOCK_INV,
		0x08, config->tmds_clock_inversion << 3);
	/* 0x17[1] = 1, aspect ratio = 16:9*/
	regmap_update_bits(adv7511->regmap, ADV7511_REG_VIDEO_INPUT_CFG2,
		ADV7511_ASPECT_RATIO_MASK, ADV7511_ASPECT_RATIO_16_9);

	/* enable HDMI mode */
	regmap_update_bits(adv7511->regmap, ADV7511_REG_HDCP_HDMI_CFG,
			   ADV7511_HDMI_CFG_MODE_MASK,
			   ADV7511_HDMI_CFG_MODE_HDMI);

	adv7511->hsync_polarity = config->hsync_polarity;
	adv7511->vsync_polarity = config->vsync_polarity;
}


/**
* @brief read the registers of ADV7511 for debug
*
* @param adv7511
*/
static void adv7511_read_config(struct adv7511 *adv7511)
{
	int ret;
	int i;

	/* read the registers of ADV7511 for debug*/
	/* use edid_buf to store regs */
	for (i = 0; i < 16; i++) {
		ret = regmap_bulk_read(adv7511->regmap, 16*i,
				       (void *)&adv7511->edid_buf[16*i], 16);
		if (ret) {
			dev_info(&adv7511->i2c_main->dev,
				 "read adv7511 config error");
			return;
		}
	}

	for (i = 0; i < 16; i++) {
		dev_dbg(&adv7511->i2c_main->dev, "%1x   %2.2x %2.2x %2.2x %2.2x | %2.2x %2.2x %2.2x %2.2x | %2.2x %2.2x %2.2x %2.2x | %2.2x %2.2x %2.2x %2.2x\n",
			i,
			adv7511->edid_buf[0+i*16], adv7511->edid_buf[1+i*16],
			adv7511->edid_buf[2+i*16], adv7511->edid_buf[3+i*16],
			adv7511->edid_buf[4+i*16], adv7511->edid_buf[5+i*16],
			adv7511->edid_buf[6+i*16], adv7511->edid_buf[7+i*16],
			adv7511->edid_buf[8+i*16], adv7511->edid_buf[9+i*16],
			adv7511->edid_buf[10+i*16], adv7511->edid_buf[11+i*16],
			adv7511->edid_buf[12+i*16], adv7511->edid_buf[13+i*16],
			adv7511->edid_buf[14+i*16],
			adv7511->edid_buf[15+i*16]);
	}
}


/**
* @brief get edid info
*	 through edid info, the timing of monitor can be acquired and
*        can be used to set the parameters of ARC PGU.
* @param adv7511
* @param block
*
* @return
*/
static int adv7511_get_edid_block(struct adv7511 *adv7511, unsigned int block)
{
	struct i2c_msg xfer[2];
	uint8_t offset;
	int i;
	int ret;
	unsigned int val;

	regmap_write(adv7511->regmap, ADV7511_REG_EDID_SEGMENT, block);
	/* wait EDID ready */

	do {
		ret = regmap_read(adv7511->regmap, ADV7511_REG_INT(0), &val);
		if (ret)
			return ret;
	} while ((val & 0x04) != 0x04);
	/* clear the bits */

	regmap_write(adv7511->regmap, ADV7511_REG_INT(0), val);

	/* Break this apart, hopefully more I2C controllers will support 64
		* byte transfers than 256 byte transfers */

	xfer[0].addr = adv7511->i2c_edid->addr;
	xfer[0].flags = 0;
	xfer[0].len = 1;
	xfer[0].buf = &offset;
	xfer[1].addr = adv7511->i2c_edid->addr;
	xfer[1].flags = I2C_M_RD;
	xfer[1].len = 64;
	xfer[1].buf = adv7511->edid_buf;

	offset = 0;

	/* read 128 bytes of edid */
	for (i = 0; i < 2; ++i) {
		ret = i2c_transfer(adv7511->i2c_edid->adapter,
				   xfer, ARRAY_SIZE(xfer));
		if (ret < 0)
			return ret;
		else if (ret != 2)
			return -EIO;

		xfer[1].buf += 64;
		offset += 64;
	}
	/* show part of edid info */
	dev_dbg(&adv7511->i2c_main->dev, "%x,%x,%x,%x,%x,%x\n",
		adv7511->edid_buf[0],
		adv7511->edid_buf[15], adv7511->edid_buf[16],
		adv7511->edid_buf[31], adv7511->edid_buf[32],
		adv7511->edid_buf[47]);
	return 0;
}

/*
	adi,input-id -
		0x00:
		0x01:
		0x02:
		0x03:
		0x04:
		0x05:
	adi,sync-pulse - Selects the sync pulse
		0x00: Use the DE signal as sync pulse
		0x01: Use the HSYNC signal as sync pulse
		0x02: Use the VSYNC signal as sync pulse
		0x03: No external sync pulse
	adi,bit-justification -
		0x00: Evently
		0x01: Right
		0x02: Left
	adi,up-conversion -
		0x00: zero-order up conversion
		0x01: first-order up conversion
	adi,timing-generation-sequence -
		0x00: Sync adjustment first, then DE generation
		0x01: DE generation first then sync adjustment
	adi,vsync-polarity - Polarity of the vsync signal
		0x00: Passthrough
		0x01: Active low
		0x02: Active high
	adi,hsync-polarity - Polarity of the hsync signal
		0x00: Passthrough
		0x01: Active low
		0x02: Active high
	adi,reverse-bitorder - If set the bitorder is reveresed
	adi,tmds-clock-inversion - If set use tdms clock inversion
	adi,clock-delay - Clock delay for the video data clock
		0x00: -1200 ps
		0x01:  -800 ps
		0x02:  -400 ps
		0x03: no dealy
		0x04:   400 ps
		0x05:   800 ps
		0x06:  1200 ps
		0x07:  1600 ps
	adi,input-style - Specifies the input style used
		0x02: Use input style 1
		0x01: Use input style 2
		0x03: Use Input style 3
	adi,input-color-depth - Selects the input format color depth
		0x03: 8-bit per channel
		0x01: 10-bit per channel
		0x02: 12-bit per channel
*/

/**
* @brief parse the options of ADV7511's input. Through these options, ADV7511
*	 can work with different graphical devices, such ARC PGU, GPU, etc..
*
* @param np
* @param config
*
* @return
*/
static int adv7511_parse_dt(struct device_node *np,
			    struct adv7511_link_config *config)
{
	int ret;
	u32 val;

	ret = of_property_read_u32(np, "adi,input-id", &val);
	if (ret < 0)
		return ret;
	config->id = val;

	ret = of_property_read_u32(np, "adi,sync-pulse", &val);
	if (ret < 0)
		config->sync_pulse = ADV7511_INPUT_SYNC_PULSE_NONE;
	else
		config->sync_pulse = val;

	ret = of_property_read_u32(np, "adi,bit-justification", &val);
	if (ret < 0)
		return ret;
	config->bit_justification = val;

	ret = of_property_read_u32(np, "adi,up-conversion", &val);
	if (ret < 0)
		config->up_conversion = ADV7511_UP_CONVERSION_ZERO_ORDER;
	else
		config->up_conversion = val;

	ret = of_property_read_u32(np, "adi,timing-generation-sequence", &val);
	if (ret < 0)
		return ret;
	config->timing_gen_seq = val;

	ret = of_property_read_u32(np, "adi,vsync-polarity", &val);
	if (ret < 0)
		return ret;
	config->vsync_polarity = val;

	ret = of_property_read_u32(np, "adi,hsync-polarity", &val);
	if (ret < 0)
		return ret;
	config->hsync_polarity = val;

	config->reverse_bitorder = of_property_read_bool(np,
		"adi,reverse-bitorder");
	config->tmds_clock_inversion = of_property_read_bool(np,
		"adi,tmds-clock-inversion");

	ret = of_property_read_u32(np, "adi,clock-delay", &val);
	if (ret)
		return ret;
	config->clock_delay = val;

	ret = of_property_read_u32(np, "adi,input-style", &val);
	if (ret)
		return ret;
	config->input_style = val;

	ret = of_property_read_u32(np, "adi,input-color-depth", &val);
	if (ret)
		return ret;
	config->input_color_depth = val;

	return 0;
}


static const struct regmap_config adv7511_regmap_config = {
	.reg_bits = 8,
	.val_bits = 8,

	.max_register = 0xff,
	.cache_type = REGCACHE_NONE,
};


static const int edid_i2c_addr = 0x7e;
static const int packet_i2c_addr = 0x70;
static const int cec_i2c_addr = 0x78;

static int adv7511_probe(struct i2c_client *i2c,
	const struct i2c_device_id *id)
{
	struct adv7511_link_config link_config;
	struct adv7511 *adv7511;
	unsigned int val;
	int ret;

	if (i2c->dev.of_node) {
		ret = adv7511_parse_dt(i2c->dev.of_node, &link_config);
		if (ret)
			return ret;
	} else {
		if (!i2c->dev.platform_data)
			return -EINVAL;
		link_config = *(struct adv7511_link_config *)
			      i2c->dev.platform_data;
	}

	adv7511 = devm_kzalloc(&i2c->dev, sizeof(*adv7511), GFP_KERNEL);
	if (!adv7511)
		return -ENOMEM;

	adv7511->regmap = devm_regmap_init_i2c(i2c, &adv7511_regmap_config);
	if (IS_ERR(adv7511->regmap))
		return PTR_ERR(adv7511->regmap);

	/* power done the chip first */
	regmap_update_bits(adv7511->regmap, ADV7511_REG_POWER,
			ADV7511_POWER_POWER_DOWN, ADV7511_POWER_POWER_DOWN);
	/* detect cable and monitor */
	ret = regmap_read(adv7511->regmap, ADV7511_REG_STATUS, &val);

	if (ret)
		return ret;
	if ((val & ADV7511_HPD_MONITOR_MASK) == (ADV7511_HPD_HIGH |
		ADV7511_MONITOR_CONNECT)) {
		dev_dbg(&i2c->dev, "monitor connected");
	} else {
		dev_dbg(&i2c->dev, "no monitor connected");
	}
	/* power up the chip */
	regmap_update_bits(adv7511->regmap, ADV7511_REG_POWER,
			ADV7511_POWER_POWER_DOWN, 0x0);
	/* HPD is both from cdc and hpd pin */
	regmap_update_bits(adv7511->regmap, ADV7511_REG_POWER2, 0xc0, 0x0);

	/* write fix regs */
	ret = regmap_register_patch(adv7511->regmap, adv7511_fixed_registers,
		ARRAY_SIZE(adv7511_fixed_registers));
	if (ret)
		return ret;

	regmap_write(adv7511->regmap, ADV7511_REG_EDID_I2C_ADDR,
		     edid_i2c_addr);
	regmap_write(adv7511->regmap, ADV7511_REG_PACKET_I2C_ADDR,
		     packet_i2c_addr);
	regmap_write(adv7511->regmap, ADV7511_REG_CEC_I2C_ADDR,
		     cec_i2c_addr);

	adv7511->i2c_main = i2c;
	adv7511->i2c_edid = i2c_new_dummy(i2c->adapter,
					  edid_i2c_addr >> 1);
	adv7511->i2c_packet = i2c_new_dummy(i2c->adapter,
					    packet_i2c_addr >> 1);
	if (!adv7511->i2c_edid)
		return -ENOMEM;

	dev_dbg(&i2c->dev, "irq. %d\n", i2c->irq);
	/* CEC is unused for now */
	regmap_write(adv7511->regmap, ADV7511_REG_CEC_CTRL,
		ADV7511_CEC_CTRL_POWER_DOWN);

	adv7511->current_edid_segment = -1;

	i2c_set_clientdata(i2c, adv7511);

#ifdef CONFIG_AXS_ADV7511_AUDIO
	adv7511_audio_init(&i2c->dev);
#endif

	/* default setting of adv7511 */
	adv7511_set_link_config(adv7511, &link_config);
	adv7511_read_config(adv7511);

	return 0;

err_i2c_unregister_device:
	i2c_unregister_device(adv7511->i2c_edid);

	return ret;
}

static int adv7511_remove(struct i2c_client *i2c)
{
	struct adv7511 *adv7511 = i2c_get_clientdata(i2c);

	i2c_unregister_device(adv7511->i2c_edid);


	return 0;
}


#ifdef CONFIG_OF
static const struct of_device_id adv7511_match[] = {
	{ .compatible = "adi, adv7511" },
	{},
};
#endif


static const struct i2c_device_id adv7511_ids[] = {
	{ "adv7511", 0 },
	{}
};

static struct i2c_driver adv7511_driver = {
	.driver = {
		.name = "adv7511",
		.of_match_table = of_match_ptr(adv7511_match),
	},
	.id_table = adv7511_ids,
	.probe = adv7511_probe,
	.remove = adv7511_remove,
};

static int __init adv7511_init(void)
{
	return i2c_add_driver(&adv7511_driver);
}
module_init(adv7511_init);

static void __exit adv7511_exit(void)
{
	i2c_del_driver(&adv7511_driver);
}
module_exit(adv7511_exit);

MODULE_AUTHOR("Lars-Peter Clausen <lars@metafoo.de>");
MODULE_DESCRIPTION("ADV7511 HDMI transmitter driver");
MODULE_LICENSE("GPL");
