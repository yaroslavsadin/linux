/*
 *  dashtty.c - tty driver for Dash channels interface.
 *
 *  Copyright (C) 2007,2008,2012 Imagination Technologies
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive
 * for more details.
 *
 */

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/export.h>
#include <linux/slab.h>
#include <linux/timer.h>
#include <linux/tty.h>
#include <linux/tty_driver.h>
#include <linux/tty_flip.h>
#include <linux/string.h>
#include <linux/sched.h>
#include <linux/console.h>
#include <linux/uaccess.h>
#include <linux/kthread.h>

#include <asm/da.h>

/* Channel error codes */
#define CONAOK	0
#define CONERR	1
#define CONBAD	2
#define CONPRM	3
#define CONADR	4
#define CONCNT	5
#define CONCBF	6
#define CONCBE	7
#define CONBSY	8

/* Default channel for the console */
#define CONSOLE_CHANNEL      1

/* First channel available for tty use. */
#define FIRST_TTY_CHANNEL    0

#define NUM_TTY_CHANNELS     6

/* Auto allocate */
#define DA_TTY_MAJOR        0

/* A speedy poll rate helps the userland debug process connection response.
 * But, if you set it too high then no other userland processes get much
 * of a look in.
 */
#define DA_TTY_POLL (HZ / 50)

static int num_channels_need_poll;

static struct timer_list poll_timer;

static struct tty_driver *channel_driver;

static struct task_struct *dashtty_thread;

#define RX_BUF_SIZE 1024

enum {
	INCHR = 1,
	OUTCHR,
	RDBUF,
	WRBUF,
	RDSTAT
};

/* One struct dashtty exists per open channel. */
struct dashtty {
	struct tty_struct *tty;
	int count;
	char rx_buf[RX_BUF_SIZE];
};

struct dashbuf {
	struct list_head entry;
	struct dashtty *tty;
	unsigned char *buf;
	int count;
	int chan;
};

static struct dashtty *dashtty_ttys[NUM_TTY_CHANNELS];
static struct list_head dashtty_output_queue;
static spinlock_t dashtty_q_lock;
static wait_queue_head_t dashtty_waitqueue;

/*
 * Low-level DA channel access routines
 */
static int chancall(int bios_function, int channel, int arg2, void *arg3,
			void *arg4)
{
	register int   bios_function__ __asm__("D1Ar1") = bios_function;
	register int   channel__       __asm__("D0Ar2") = channel;
	register int   arg2__          __asm__("D1Ar3") = arg2;
	register void *arg3__          __asm__("D0Ar4") = arg3;
	register void *arg4__          __asm__("D1Ar5") = arg4;
	register int   bios_call       __asm__("D0Ar6") = 3;
	register int   result          __asm__("D0Re0");

	__asm__ volatile (
		"SETL   [A0StP++], %6,%5\n\t"
		"SETL   [A0StP++], %4,%3\n\t"
		"SETL   [A0StP++], %2,%1\n\t"
		"ADD    A0StP, A0StP, #8\n\t"
		"SWITCH #0x0C30208\n\t"
		"GETD   %0, [A0StP+#-8]\n\t"
		"SUB    A0StP, A0StP, #(4*6)+8\n\t"
		: "=r" (result)/* outs */
		: "r" (bios_function__), "r" (channel__), "r" (arg2__),
		  "r" (arg3__), "r" (arg4__), "r" (bios_call) /* ins */
		: "memory");

	return result;
}

/*
 * Attempts to fetch count bytes from channel channel and returns actual count.
 */
static int fetch_data(int channel)
{
	struct dashtty *dashtty = dashtty_ttys[channel];
	struct tty_struct *tty = dashtty->tty;
	int received = 0;

	if (chancall(RDBUF, channel, RX_BUF_SIZE, (void *)dashtty->rx_buf,
		     &received) == CONAOK) {
		if (received) {
			int space;
			unsigned char *cbuf;

			space = tty_prepare_flip_string(tty, &cbuf, received);

			if (space <= 0)
				return 0;

			memcpy(cbuf, dashtty->rx_buf, space);
			tty_flip_buffer_push(tty);
		}
	}

	return received;
}

/*
 *	Finds the next channel to poll
 */
static int find_channel_to_poll(void)
{
	static int last_polled_channel = FIRST_TTY_CHANNEL;

	int this_channel = last_polled_channel;

	while (++this_channel != last_polled_channel) {
		if (this_channel >= NUM_TTY_CHANNELS)
			this_channel = 0;

		if (dashtty_ttys[this_channel]) {
			last_polled_channel = this_channel;
			break;
		}
	}
	return last_polled_channel;
}

static int put_data(void *arg)
{
	struct dashbuf *dbuf;
	int number_written;

	__set_current_state(TASK_RUNNING);
	while (!kthread_should_stop()) {
		/*
		 * Pick up all the output buffers and write them out.
		 *
		 * FIXME: should we check with ASE how much room we have?
		 * Ideally, this will already have been done by write_room ??
		 */
		spin_lock(&dashtty_q_lock);

		while (!list_empty(&dashtty_output_queue)) {
			dbuf = list_entry(dashtty_output_queue.next,
					  struct dashbuf, entry);

			chancall(WRBUF, dbuf->chan, dbuf->count,
				 (void *)dbuf->buf, &number_written);

			if (list_is_last(&dbuf->entry,
					 &dashtty_output_queue))
				list_del_init(&dashtty_output_queue);
			else
				list_del(&dbuf->entry);

			/*
			 * FIXME: should we only be doing wake when we
			 * know something is sleeping?
			 */
			tty_wakeup(dbuf->tty->tty);

			kfree(dbuf->buf);
			kfree(dbuf);
		}
		spin_unlock(&dashtty_q_lock);

		wait_event_interruptible(dashtty_waitqueue,
					 !list_empty(&dashtty_output_queue));
	}

	return 0;
}

/*
 *	This gets called every DA_TTY_POLL and polls the channels for data
 */
static void dashtty_timer(unsigned long ignored)
{
	struct dashtty *dtty;
	int this_channel;

	/* If there are no ports open do nothing and don't poll again. */
	if (num_channels_need_poll == 0)
		return;

	this_channel = find_channel_to_poll();
	dtty = dashtty_ttys[this_channel];

	/* Did we find a channel to poll ? */
	if (dtty)
		fetch_data(this_channel);

	mod_timer_pinned(&poll_timer, jiffies + DA_TTY_POLL);
}

static void add_poll_timer(struct timer_list *poll_timer)
{
	init_timer(poll_timer);

	poll_timer->function = dashtty_timer;
	poll_timer->expires = jiffies + DA_TTY_POLL;

	/*
	 * Always attach the timer to the boot CPU.
	 */
	add_timer_on(poll_timer, 0);
}

static int dashtty_open(struct tty_struct *tty, struct file *filp)
{
	struct dashtty *dashtty;
	int line;

	if (!tty)
		return -ENODEV;

	line = tty->index;

	if (line < 0 || line >= NUM_TTY_CHANNELS)
		return -ENODEV;

	dashtty = dashtty_ttys[line];
	if (!dashtty) {
		dashtty = kzalloc(sizeof(struct dashtty), GFP_KERNEL);

		if (!dashtty)
			return -ENOMEM;

		dashtty->tty = tty;
		dashtty_ttys[line] = dashtty;
	}

	dashtty_ttys[line]->count++;

	/*
	 * Don't add the poll timer if we're opening a console. This
	 * avoids the overhead of polling the Dash but means it is not
	 * possible to have a login on /dev/console.
	 *
	 */
	if (line == CONSOLE_CHANNEL)
		return 0;

	if (num_channels_need_poll == 0)
		add_poll_timer(&poll_timer);

	num_channels_need_poll++;

	return 0;
}

static void dashtty_close(struct tty_struct *tty, struct file *filp)
{
	int line;

	if (!tty)
		return;

	line = tty->index;

	if (line < 0 || line >= NUM_TTY_CHANNELS)
		return;

	dashtty_ttys[line]->count--;

	if (!dashtty_ttys[line]->count) {
		kfree(dashtty_ttys[line]);
		dashtty_ttys[line] = NULL;
	}

	if (line == CONSOLE_CHANNEL)
		return;

	num_channels_need_poll--;

	if (num_channels_need_poll <= 0)
		del_timer(&poll_timer);
}

static int dashtty_write(struct tty_struct *tty, const unsigned char *buf,
			 int count)
{
	struct dashtty *dtty;
	struct dashbuf *dbuf;
	int channel;

	if (count <= 0)
		return 0;

	/* Determine the channel */
	channel = FIRST_TTY_CHANNEL + tty->index;
	dtty = dashtty_ttys[channel];
	BUG_ON(!dtty);

	dbuf = kzalloc(sizeof(*dbuf), GFP_KERNEL);
	if (!dbuf)
		return 0;

	dbuf->buf = kzalloc(count, GFP_KERNEL);
	if (!dbuf->buf) {
		kfree(dbuf);
		return 0;
	}

	memcpy(dbuf->buf, buf, count);
	dbuf->count = count;
	dbuf->chan = channel;
	dbuf->tty = dtty;

	/*
	 * Enqueue the output buffer.
	 *
	 * The reason that we asynchronously write the buffer is because
	 * there's no way to tell what CPU we're currently executing on.
	 * If we were to write the buffer synchornously then because dash
	 * channels are per-CPU the buffer would be written to the channel
	 * of whatever CPU we're running on.
	 *
	 * What we actually want to happen is have all input and output
	 * done on one CPU.
	 */
	spin_lock(&dashtty_q_lock);
	list_add_tail(&dbuf->entry, &dashtty_output_queue);
	spin_unlock(&dashtty_q_lock);

	wake_up_interruptible(&dashtty_waitqueue);

	/*
	 * FIXME: This is slightly optimistic. Because we're deferring
	 * the output until later it is impossible to predict whether we
	 * will actually write "count" bytes.
	 */
	return count;
}

/*
 * Ask the stub how much room there is in the channel
 */
static int dashtty_write_room(struct tty_struct *tty)
{
	unsigned int data[2];
	static unsigned int actual_space;
	int channel;

	if (actual_space)
		return actual_space;

	channel = FIRST_TTY_CHANNEL + tty->index;

	/* Is there any data waiting ? */
	if (chancall(RDSTAT, channel, 0, NULL, &data[0]) == CONAOK) {
		/* There was a bug in LogieDash (1.2.8a at least) where
		 * the two values were returned the WRONG WAY AROUND!!!
		 */
		actual_space = data[1];
	}

	return actual_space;
}

static int dashtty_chars_in_buffer(struct tty_struct *tty)
{
	return 0;               /* we're not buffering */
}

static const struct tty_operations dashtty_ops = {
	.open = dashtty_open,
	.close = dashtty_close,
	.write = dashtty_write,
	.write_room = dashtty_write_room,
	.chars_in_buffer = dashtty_chars_in_buffer,
};

static int __init dashtty_init(void)
{
	int ret;

	if (!metag_da_enabled())
		return -ENODEV;

	channel_driver = alloc_tty_driver(NUM_TTY_CHANNELS);
	if (!channel_driver)
		return -ENOMEM;

	channel_driver->owner = THIS_MODULE;
	channel_driver->driver_name = "ttyDA";
	channel_driver->name = "ttyDA";
	channel_driver->major = DA_TTY_MAJOR;
	channel_driver->minor_start = 0;
	channel_driver->type = TTY_DRIVER_TYPE_SERIAL;
	channel_driver->subtype = SERIAL_TYPE_NORMAL;
	channel_driver->init_termios = tty_std_termios;
	channel_driver->init_termios.c_cflag =
	    B38400 | CS8 | CREAD | HUPCL | CLOCAL;
	channel_driver->flags = TTY_DRIVER_REAL_RAW;

	tty_set_operations(channel_driver, &dashtty_ops);

	spin_lock_init(&dashtty_q_lock);
	INIT_LIST_HEAD(&dashtty_output_queue);
	init_waitqueue_head(&dashtty_waitqueue);

	dashtty_thread = kthread_create(put_data, NULL, "ttyDA");
	if (IS_ERR(dashtty_thread)) {
		pr_info("Couldn't create dashtty thread\n");
		return PTR_ERR(dashtty_thread);
	}
	kthread_bind(dashtty_thread, 0);
	wake_up_process(dashtty_thread);

	ret = tty_register_driver(channel_driver);

	if (ret < 0) {
		pr_info("Couldn't install dashtty driver: err %d\n",
			ret);
		put_tty_driver(channel_driver);
		return ret;
	}

	return 0;
}

static void dashtty_exit(void)
{
	kthread_stop(dashtty_thread);
	del_timer(&poll_timer);
	tty_unregister_driver(channel_driver);
	put_tty_driver(channel_driver);
}

module_init(dashtty_init);
module_exit(dashtty_exit);

#ifdef CONFIG_DA_CONSOLE

static void dash_console_write(struct console *co, const char *s,
			       unsigned int count)
{
	int actually_written;

	chancall(WRBUF, CONSOLE_CHANNEL, count, (void *)s, &actually_written);
}

static struct tty_driver *dash_console_device(struct console *c, int *index)
{
	*index = c->index;
	return channel_driver;
}

struct console dash_console = {
	.name = "ttyDA",
	.write = dash_console_write,
	.device = dash_console_device,
	.flags = CON_PRINTBUFFER,
	.index = 1,
};

#endif
