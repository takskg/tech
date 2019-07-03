#include "bootpack.h"

#define PIT_CTRL	0x0043
#define PIT_CNT0	0x0040

#define TIMER_FLAGS_ALLOC		1	/* 確保した状態 */
#define TIMER_FLAGS_USING		2	/* タイマ作動中 */

struct TIMERCTRL timerctrl;

void init_pit(void)
{
    int idx;

    io_out8(PIT_CTRL, 0x34);
    io_out8(PIT_CNT0, 0x9c);
    io_out8(PIT_CNT0, 0x2e);

    timerctrl.count = 0;
    timerctrl.next = 0xffffffff; /* 最初は作動中のタイマがないので */
    timerctrl.using = 0;
	for (idx = 0; idx < MAX_TIMER; idx++) {
		timerctrl.timers0[idx].flags = 0; /* 未使用 */
	}
}

struct TIMER *timer_alloc(void)
{
	int i;
	for (i = 0; i < MAX_TIMER; i++) {
		if (timerctrl.timers0[i].flags == 0) {
			timerctrl.timers0[i].flags = TIMER_FLAGS_ALLOC;
			return &timerctrl.timers0[i];
		}
	}
	return 0; /* 見つからなかった */
}

void timer_free(struct TIMER *timer)
{
	timer->flags = 0; /* 未使用 */
	return;
}

void timer_init(struct TIMER *timer, struct FIFO8 *fifo, unsigned char data)
{
	timer->fifo = fifo;
	timer->data = data;
	return;
}

void timer_settime(struct TIMER *timer, unsigned int timeout)
{
    int e, i, j;

	timer->timeout = timeout + timerctrl.count;
	timer->flags = TIMER_FLAGS_USING;
	e = io_load_eflags();
	io_cli();
	/* どこに入れればいいかを探す */
	for (i = 0; i < timerctrl.using; i++) {
		if (timerctrl.timers[i]->timeout >= timer->timeout) {
			break;
		}
	}
	/* うしろをずらす */
	for (j = timerctrl.using; j > i; j--) {
		timerctrl.timers[j] = timerctrl.timers[j - 1];
	}
	timerctrl.using++;
	/* あいたすきまに入れる */
	timerctrl.timers[i] = timer;
	timerctrl.next = timerctrl.timers[0]->timeout;
	io_store_eflags(e);
	return;
}

void inthandler20(int *esp)
{
    int i, j;

    io_out8(PIC0_OCW2, 0x60);	/* IRQ-00受付完了をPICに通知 */

    timerctrl.count++;
	if (timerctrl.next > timerctrl.count) {
		return;
	}
	for (i = 0; i < timerctrl.using; i++) {
		/* timersのタイマは全て動作中のものなので、flagsを確認しない */
		if (timerctrl.timers[i]->timeout > timerctrl.count) {
			break;
		}
		/* タイムアウト */
		timerctrl.timers[i]->flags = TIMER_FLAGS_ALLOC;
		fifo8_put(timerctrl.timers[i]->fifo, timerctrl.timers[i]->data);
	}
	/* ちょうどi個のタイマがタイムアウトした。残りをずらす。 */
	timerctrl.using -= i;
	for (j = 0; j < timerctrl.using; j++) {
		timerctrl.timers[j] = timerctrl.timers[i + j];
	}
	if (timerctrl.using > 0) {
		timerctrl.next = timerctrl.timers[0]->timeout;
	} else {
		timerctrl.next = 0xffffffff;
	}
}