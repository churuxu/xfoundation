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

#include "testsuite.h"

typedef struct _myctx {
	timer_ctx timer;
	int step;
	int val;
	long long t;
}myctx;


#define test_time(t1, t2, dur)  test((dur-40)<(t2-t1) && (t2-t1)<(dur+40) )


static void test_delayed_timer(void* arg) {
	myctx* ctx = (myctx*)arg;
	if (!ctx) {
		ctx = (myctx*)malloc(sizeof(myctx));
		memset(ctx, 0, sizeof(myctx));		
	}
	future_enter(&ctx->step);
	ctx->t = clock_get_tick();
	ctx->val++;
	test(0 == timer_add_timeout(timer_queue_get_main(), &ctx->timer, 100, test_delayed_timer, ctx));
	future_wait(&ctx->timer);
	ctx->val++;
	test(0 == timer_add_timeout(timer_queue_get_main(), &ctx->timer, 100, test_delayed_timer, ctx));
	future_wait(&ctx->timer);
	test(2 == ctx->val);
	test_time(ctx->t, clock_get_tick(), 200);
	future_leave();
	free(ctx);
	test_ok();
	return;
error:
	free(ctx);
	test_fail();
}



static void test_interval_timer_(void* arg) {
	myctx* ctx = (myctx*)arg;
	ctx->val++;
    printf("test_interval_timer_ ticked %lld\n", (long long)clock_get_timestamp());
	if (ctx->val == 3) {
		timer_cancel(timer_queue_get_main(), &ctx->timer);
		test_time(ctx->t, clock_get_tick(), 3000);
		free(ctx);
		test_ok();
	}
	return;
error:
	free(ctx);
	test_fail();
}

static void test_interval_timer(void* arg) {
	myctx* ctx = (myctx*)arg;
	if (!ctx) {
		ctx = (myctx*)malloc(sizeof(myctx));
		memset(ctx, 0, sizeof(myctx));
	}
	ctx->t = clock_get_tick();
	testzero(timer_add_interval(timer_queue_get_main(), &ctx->timer, 1000, test_interval_timer_, ctx));
	return;
error:
	free(ctx);
	test_fail();
}

static int multcount_;
static timer_ctx* timers_;
static long long mt1_, mt2_, mt3_;
#define TIMER_NUM 10000
static void test_multi_timer_(void* arg) {
	timer_ctx* ctx = (timer_ctx*)arg;
	timer_cancel(timer_queue_get_main(), ctx);
	multcount_++;
	if (multcount_ >= TIMER_NUM) {
		mt3_ = clock_get_tick();
		test_trace("%s used:%d ms\n", __FUNCTION__, (int)(mt3_ - mt2_));
		free(timers_);
		test_ok();
	}
}

static void test_multi_timer(void* arg) {
	int i;	
	timer_ctx* ctx;
	int timeout = 100;
	timers_ = malloc(sizeof(timer_ctx) * TIMER_NUM);
	mt1_ = clock_get_tick();	
	for (i = 0; i < TIMER_NUM; i++) {
		ctx = timers_ + i;
		if (i % 1000 == 0) timeout+=30;
		testzero(timer_add_interval(timer_queue_get_main(), ctx, timeout, test_multi_timer_, ctx));
	}
	mt2_ = clock_get_tick();
	test_trace("%s add timer used:%d ms max timeout:%d\n",__FUNCTION__, (int)(mt2_ - mt1_), timeout);
	return;
error:
	free(ctx);
	test_fail();
}

void test_timerqueue_init() {
	
	test_register(test_delayed_timer);
	test_register(test_interval_timer);
	test_register(test_multi_timer);
}

