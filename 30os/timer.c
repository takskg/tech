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
    for(idx=0; idx<MAX_TIMER; ++idx){
        timerctrl.timer[idx].flags = 0;//未使用
    }
}

struct TIMER *timer_alloc(void)
{
	int i;
	for (i = 0; i < MAX_TIMER; i++) {
		if (timerctrl.timer[i].flags == 0) {
			timerctrl.timer[i].flags = TIMER_FLAGS_ALLOC;
			return &timerctrl.timer[i];
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
	timer->timeout = timeout;
	timer->flags = TIMER_FLAGS_USING;
	return;
}

void inthandler20(int *esp)
{
    int i;
    
    io_out8(PIC0_OCW2, 0x60);	/* IRQ-00受付完了をPICに通知 */

    timerctrl.count++;
	for (i = 0; i < MAX_TIMER; i++) {
		if (timerctrl.timer[i].flags == TIMER_FLAGS_USING) {
			timerctrl.timer[i].timeout--;
			if (timerctrl.timer[i].timeout == 0) {
				timerctrl.timer[i].flags = TIMER_FLAGS_ALLOC;
				fifo8_put(timerctrl.timer[i].fifo, timerctrl.timer[i].data);
			}
		}
	}
}