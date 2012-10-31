/*
 *  Support for reading and writing Meta core internal registers.
 *
 *  Copyright (C) 2011 Imagination Technologies Ltd.
 *
 */

#include <linux/delay.h>
#include <linux/export.h>

#include <asm/core_reg.h>
#include <asm/io.h>
#include <asm/hwthread.h>

#define METAG_ALL_VALUES
#define METAC_ALL_VALUES
#define METAG_TXUXXRX_VALUES
#include <asm/tbx/machine.inc>
#include <asm/tbx/metagtbx.h>

#define UNIT_BIT_MASK		TXUXXRXRQ_UXX_BITS
#define REG_BIT_MASK		TXUXXRXRQ_RX_BITS
#define THREAD_BIT_MASK		TXUXXRXRQ_TX_BITS

#define UNIT_SHIFTS		TXUXXRXRQ_UXX_S
#define REG_SHIFTS		TXUXXRXRQ_RX_S
#define THREAD_SHIFTS		TXUXXRXRQ_TX_S

#define UNIT_VAL(x)		(((x) << UNIT_SHIFTS) & UNIT_BIT_MASK)
#define REG_VAL(x)		(((x) << REG_SHIFTS) & REG_BIT_MASK)
#define THREAD_VAL(x)		(((x) << THREAD_SHIFTS) & THREAD_BIT_MASK)

/*
 * core_reg_write() - modify the content of a register in a core unit.
 * @unit:	The unit to be modified.
 * @reg:	Register number within the unit.
 * @thread:	The thread we want to access.
 * @val:	The new value to write.
 *
 * Check metac_x_y.inc for a list/defines of supported units (ie: TXUPC_ID,
 * TXUTR_ID, etc), and metag.inc for the defines of regnums within the units
 * (ie: TXMASKI_REGNUM, TXPOLLI_REGNUM, etc).
 */
void core_reg_write(int unit, int reg, int thread, unsigned int val)
{
	unsigned long flags;

	/* TXUCT_ID has its own memory mapped registers */
	if (unit == TXUCT_ID) {
		void __iomem *cu_reg = __CU_addr(thread, reg);
		writel(val, cu_reg);
		return;
	}

	TBI_LOCK(flags);

	/* wait for ready */
	while (!(readl(TXUXXRXRQ) & TXUXXRXRQ_DREADY_BIT))
		udelay(10);

	/* set the value to write */
	writel(val, TXUXXRXDT);

	/* set the register to write */
	val = UNIT_VAL(unit) | REG_VAL(reg) | THREAD_VAL(thread);
	writel(val, TXUXXRXRQ);

	/* wait for finish */
	while (!(readl(TXUXXRXRQ) & TXUXXRXRQ_DREADY_BIT))
		udelay(10);

	TBI_UNLOCK(flags);
}
EXPORT_SYMBOL(core_reg_write);

/*
 * core_reg_read() - read the content of a register in a core unit.
 * @unit:	The unit to be modified.
 * @reg:	Register number within the unit.
 * @thread:	The thread we want to access.
 *
 * Check metac_x_y.inc for a list/defines of supported units (ie: TXUPC_ID,
 * TXUTR_ID, etc), and metag.inc for the defines of regnums within the units
 * (ie: TXMASKI_REGNUM, TXPOLLI_REGNUM, etc).
 */
unsigned int core_reg_read(int unit, int reg, int thread)
{
	unsigned long flags;
	unsigned int val;

	/* TXUCT_ID has its own memory mapped registers */
	if (unit == TXUCT_ID) {
		void __iomem *cu_reg = __CU_addr(thread, reg);
		val = readl(cu_reg);
		return val;
	}

	TBI_LOCK(flags);

	/* wait for ready */
	while (!(readl(TXUXXRXRQ) & TXUXXRXRQ_DREADY_BIT))
		udelay(10);

	/* set the register to read */
	val = (UNIT_VAL(unit) | REG_VAL(reg) | THREAD_VAL(thread) |
							TXUXXRXRQ_RDnWR_BIT);
	writel(val, TXUXXRXRQ);

	/* wait for finish */
	while (!(readl(TXUXXRXRQ) & TXUXXRXRQ_DREADY_BIT))
		udelay(10);

	/* read the register value */
	val = readl(TXUXXRXDT);

	TBI_UNLOCK(flags);

	return val;
}
EXPORT_SYMBOL(core_reg_read);
