/* Copyright (C) 2016-2017 churuxu
* https://github.com/churuxu/cfoundations
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

#include "workasync.h"

static work_callback_runner backrunner_;
static work_callback_runner mainrunner_;


static void run_work_ctx(void* arg) {
	work_ctx* ctx = (work_ctx*)arg;
	ctx->run(ctx->udata);
	ctx->ready = 1;
	mainrunner_(ctx->cb, ctx->udata);
}

#ifdef _WIN32
#include <windows.h>
#define ERROR_INVAL 87

static DWORD CALLBACK work_routine(void* arg) {
	run_work_ctx(arg);
	return 0;
}

static int default_back_runner(work_callback cb, void* udata) {
	BOOL ret = QueueUserWorkItem(work_routine, udata, 0);
	if (!ret)return GetLastError();
	return 0;
}


#else

#include <pthread.h>
#include <errno.h>
#define ERROR_INVAL EINVAL
static void* work_routine(void* arg) {
	run_work_ctx(arg);
	return NULL;
}

int default_back_runner(work_callback cb, void* userdata) {
	int ret;
	pthread_t th;
	ret = pthread_create(&th, NULL, work_routine, userdata);
	if (!ret) {
		pthread_detach(th);
		return 0;
	}
	ret = errno ? errno : ret;	
	return ret;
}

#endif

static int default_main_runner(work_callback cb, void* udata) {
	if (cb)cb(udata);
	return 0;
}

int work_async_init(work_callback_runner backrunner, work_callback_runner mainrunner) {
	backrunner_ = backrunner;
	mainrunner_ = mainrunner;
	if (!backrunner_) {
		backrunner_ = default_back_runner;
	}
	if (!mainrunner_) {
		mainrunner_ = default_main_runner;
	}
	return 0;
}

int work_async_run(work_ctx* ctx) {
	if (!ctx || !ctx->run)return ERROR_INVAL;
	ctx->ready = 0;
	return backrunner_(run_work_ctx, ctx);
}


