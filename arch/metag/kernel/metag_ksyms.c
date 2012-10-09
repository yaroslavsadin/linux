#include <linux/export.h>
#include <linux/linkage.h>
#include <linux/string.h>
#include <linux/mm.h>
#include <linux/user.h>
#include <linux/interrupt.h>
#include <linux/hardirq.h>

#include <asm/setup.h>
#include <asm/checksum.h>
#include <asm/uaccess.h>
#include <asm/traps.h>
#include <asm/ftrace.h>

#define METAG_ALL_VALUES
#define METAC_ALL_VALUES
#include <asm/tbx/metagtbx.h>

/* uaccess symbols */
EXPORT_SYMBOL(__copy_user_zeroing);
EXPORT_SYMBOL(__copy_user);
EXPORT_SYMBOL(__get_user_asm_b);
EXPORT_SYMBOL(__get_user_asm_w);
EXPORT_SYMBOL(__get_user_asm_d);
EXPORT_SYMBOL(__put_user_asm_b);
EXPORT_SYMBOL(__put_user_asm_w);
EXPORT_SYMBOL(__put_user_asm_d);
EXPORT_SYMBOL(__put_user_asm_l);
EXPORT_SYMBOL(__strncpy_from_user);
EXPORT_SYMBOL(strnlen_user);
EXPORT_SYMBOL(__do_clear_user);

EXPORT_SYMBOL(pTBI_get);
EXPORT_SYMBOL(meta_memoffset);
EXPORT_SYMBOL(kick_register_func);
EXPORT_SYMBOL(kick_unregister_func);
#ifdef CONFIG_SMP
EXPORT_SYMBOL(get_trigger_mask);
#else
EXPORT_SYMBOL(global_trigger_mask);
#endif

EXPORT_SYMBOL(empty_zero_page);
EXPORT_SYMBOL(kernel_thread);

EXPORT_SYMBOL(pfn_base);

/* TBI symbols */
EXPORT_SYMBOL(__TBI);
EXPORT_SYMBOL(__TBIFindSeg);
EXPORT_SYMBOL(__TBIPoll);
EXPORT_SYMBOL(__TBITimeStamp);
EXPORT_SYMBOL(__TBITransStr);
EXPORT_SYMBOL(__TBICodeCacheFlush);
EXPORT_SYMBOL(__TBIDataCacheFlush);

#define DECLARE_EXPORT(name) extern void name(void); EXPORT_SYMBOL(name)

/* libgcc functions */
DECLARE_EXPORT(__ashldi3);
DECLARE_EXPORT(__ashrdi3);
DECLARE_EXPORT(__lshrdi3);
DECLARE_EXPORT(__udivsi3);
DECLARE_EXPORT(__divsi3);
DECLARE_EXPORT(__umodsi3);
DECLARE_EXPORT(__modsi3);
DECLARE_EXPORT(__muldi3);

/* Maths functions */
EXPORT_SYMBOL(div_u64);
EXPORT_SYMBOL(div_s64);

/* String functions */
EXPORT_SYMBOL(memcpy);
EXPORT_SYMBOL(memset);
EXPORT_SYMBOL(memmove);

#ifdef CONFIG_FUNCTION_TRACER
EXPORT_SYMBOL(mcount);
EXPORT_SYMBOL(mcount_wrapper);
#endif
