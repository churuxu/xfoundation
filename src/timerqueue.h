/* Copyright (C) 2016-2017 churuxu 
 * https://github.com/churuxu/xfoundation
 * 
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 * 
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif 

	/*
	run timer queue loop example

	int waittime = 0;
	while(1){
		timer_queue_process(tmq, &waittime);
		sleep_ms(waittime);
	}

	*/

	/** clock for get tick, suggest return milliseconds */
	typedef uint64_t(*timer_clock_function)();

	/** callback to run when timer expired */
	typedef void(*timer_callback)(void* userdata);

	/** callback to run when timer queue added new timer and the new timer will expire earliest.
		you can interrupt waiting operation and use newdelay time to wait.
	*/
	typedef void(*timer_queue_observer)(void* udata, int newdelay);

	typedef struct _timer_queue* timer_queue_t;

	typedef struct _timer_ctx {
		timer_callback cb;
		void* udata;
		int ready;
		void* _private[10];
	}timer_ctx;

	/** get main queue (for main thread use, do not destroy) */
	timer_queue_t timer_queue_get_main();

	/** create timer queue */
	int timer_queue_create(timer_queue_t* out);

	/** destroy timer queue */
	int timer_queue_destroy(timer_queue_t tmq);

	/** process timer queue, callback of timer will run when timer expired
	  nextwait[out] is delay time when next timer will expire.
	*/
	int timer_queue_process(timer_queue_t tmq, int* nextwait);

	/** set timer_queue clock */
	int timer_queue_set_clock(timer_queue_t tmq, timer_clock_function clockfn);

	/** set timer_queue change observer */
	int timer_queue_set_observer(timer_queue_t tmq, timer_queue_observer cb, void* udata);

	/** add timer to timer_queue_t, timer will run once.
	  memory of ctx[out] must be valid befor timer expired or timer canceled.
	*/
	int timer_add_timeout(timer_queue_t tmq, timer_ctx* ctx, unsigned int timeout, timer_callback cb, void* userdata);

	/** add timer to timer_queue_t, timer will run multiple.
	  memory of ctx[out] must be valid befor timer canceled.
	*/
	int timer_add_interval(timer_queue_t tmq, timer_ctx* ctx, unsigned int interval, timer_callback cb, void* userdata);

	/** remove timer from timer_queue_t */
	int timer_cancel(timer_queue_t tmq, timer_ctx* ctx);



#ifdef __cplusplus
}
#endif 

