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

#include "testconfig.h"
#include <string.h>

#ifdef _WIN32
#define mssleep(ms) Sleep(ms)
#define trace_with_tid(msg) test_trace("%s at thread:%d\n",msg, GetCurrentThreadId())
#else
#include <pthread.h>
#define mssleep(ms) usleep(ms*1000)
#define trace_with_tid(msg) test_trace("%s at  thread:%p\n",msg, pthread_self())
#endif

typedef struct _myctx{	
	work_ctx work;
	int val;
	int step;
}myctx;

void blocking_work(void* arg) {
	myctx* ctx = (myctx*)arg;
	trace_with_tid("work1 begin");
	ctx->val = 123;
	mssleep(1000);
	trace_with_tid("work1 end");
}

// use other function for work_async_run
void test_work_async1(void* arg) {
	myctx* ctx = (myctx*)arg;
	if (!ctx) {
		ctx = (myctx*)malloc(sizeof(myctx));
		memset(ctx, 0, sizeof(myctx));
		future_set_callback(&ctx->work, test_work_async1, ctx);
	}
	future_enter(&ctx->step);	
	trace_with_tid("work1 prepare");
	ctx->work.run = blocking_work;
	testzero(work_async_run(&ctx->work));	
	future_wait(&ctx->work);
	trace_with_tid("work1 callback waited");
	test(ctx->val == 123);
	future_leave();
	test_ok();
	return;
error:
	free(ctx);
	test_fail();	
}

// use this function for work_async_run
void test_work_async2(void* arg) {
	myctx* ctx = (myctx*)arg;
	if (!ctx) {
		ctx = (myctx*)malloc(sizeof(myctx));
		memset(ctx, 0, sizeof(myctx));
		future_set_callback(&ctx->work, test_work_async2, ctx);
	}
	future_enter(&ctx->step);
	trace_with_tid("work2 prepare");
	ctx->work.run = test_work_async2;
	future_yield(testzero(work_async_run(&ctx->work))); //to back thread
	trace_with_tid("work2 begin");
	mssleep(1000);
	ctx->val = 234;
	trace_with_tid("work2 end");
	future_wait(&ctx->work); //to main thread
	trace_with_tid("work2 callback waited");
	test(ctx->val == 234);
	future_leave();
	test_ok();
	return;
error:
	free(ctx);
	test_fail();
}


void test_workasync_init() {
	test_register(test_work_async1);
	test_register(test_work_async2);

}

