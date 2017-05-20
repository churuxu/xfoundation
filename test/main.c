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

#ifdef _WIN32
#include <windows.h>
#endif

#ifdef _MSC_VER
#pragma comment(lib,"ws2_32.lib")
#endif

static poll_looper_t main_poll_looper_;
static io_looper_t main_io_looper_;

#ifdef IO_ASYNC_USE_IOCP

static poll_event evs_[20];
static int count_ = 0;


static void on_poll_result(void* arg) {
	HANDLE hev = (HANDLE)arg;
	poll_looper_process_events(main_poll_looper_, evs_, count_);
	SetEvent(hev);
}


static DWORD CALLBACK PollThread(LPVOID arg) {
	HANDLE hev = CreateEvent(NULL, FALSE, FALSE, NULL);
	while (1) {
		count_ = 20;
		if (0 == poll_looper_wait_events(main_poll_looper_, evs_, &count_, 60000)) {
			io_looper_post_callback(main_io_looper_, on_poll_result, hev);
			WaitForSingleObject(hev, INFINITE);
		}
	}
	return 1;
}

#define main_work_queue_init()
#define main_work_queue_process()

int main_work_queue_post(work_callback cb, void* udata) {
	return io_looper_post_callback(main_io_looper_, cb, udata);
}
#else

typedef struct workitem {
	work_callback cb;
	void* udata;
}workitem;

#ifdef _WIN32
static CRITICAL_SECTION workqueuelock_;
static memory_stream_t workqueue_;

int main_work_queue_init() {
	InitializeCriticalSection(&workqueuelock_);
	return memory_stream_open(&workqueue_, 8192);
}
int main_work_queue_post(work_callback cb, void* udata) {
	workitem item = {cb, udata};
	int ret;
	EnterCriticalSection(&workqueuelock_);
	ret = (sizeof(item) == memory_stream_write(workqueue_, &item, sizeof(item)));
	LeaveCriticalSection(&workqueuelock_);
	poll_looper_wake(main_poll_looper_);
	return !ret;
}

void main_work_queue_process() {
	workitem item[32];
	size_t ret;
	size_t i;
	work_callback cb;
	EnterCriticalSection(&workqueuelock_);
	ret = memory_stream_read(workqueue_, item, sizeof(workitem)*32);
	LeaveCriticalSection(&workqueuelock_);
	ret /= sizeof(workitem);
	for (i = 0; i < ret; i++) {
		cb = item[i].cb;
		if(cb)cb(item[i].udata);
	}
}
#else
static int pipefd[2];

int main_work_queue_init() {
	pipe(pipefd);
	return io_set_nonblocking(pipefd[1]);	
}


int main_work_queue_post(work_callback cb, void* udata) {
	workitem item = { cb, udata };
	int ret;	
	ret = (sizeof(item) == write(pipefd[1], &item, sizeof(item)));	
	return !ret;
}

void main_work_queue_process() {
	int ret = 0;
	workitem cmd;
	while (1) {		
		ret = (int)read(fd, &cmd, sizeof(cmd));
		if (ret == sizeof(cmd)) {
			if(cmd.cb)cmd.cb(cmd.udata);
		}		
		break;
	}
	return 1;
}

#endif

#endif


const char* get_local_data_path(){
	return ".";	
}

void on_test_result(int total, int ok, int fail) {
	int ret;
	if (total > 0 && (total == ok) && fail == 0) {
		ret = 0;
	} else {
		ret = 1;
	}
	exit(ret);
}

int main() {
	io_event ctxs[20];
	int count;
	int nextwait = 0;
	uint64_t t1; 
	timer_queue_t timerqueue;

#ifdef _WIN32
	WSADATA wsa;
	WSAStartup(MAKEWORD(2,2), &wsa);
#endif
	
	printf("tick clock:%lld\n", (long long)clock_get_tick());
	printf("time clock:%lld\n", (long long)clock_get_timestamp());

	main_io_looper_ = io_looper_get_main();
	main_poll_looper_ = poll_looper_get_main();
	timerqueue = timer_queue_get_main();	
	ensure(main_io_looper_);
	ensure(main_poll_looper_);
	ensure(timerqueue);

    timer_queue_set_clock(timerqueue, clock_get_tick);

	main_work_queue_init();
	work_async_init(NULL, main_work_queue_post);

#ifdef IO_ASYNC_USE_IOCP	
	CreateThread(NULL, 0, PollThread, NULL, 0, NULL);
#endif

	t1 = clock_get_tick();

	run_all_tests(on_test_result);

	while (1) {
		main_work_queue_process();
		timer_queue_process(timerqueue, &nextwait);		
		count = 20;
		if (nextwait > 10000 || nextwait < 0)nextwait = 10000;
#ifdef NDEBUG
		if (clock_get_tick() - t1 > (5 * 60 * 1000)) {
			test_trace("test runed too long");
			exit(1);
		}
#endif
		if (0 == io_looper_wait_events(main_io_looper_, ctxs, &count, nextwait)) {
			io_looper_process_events(main_io_looper_, ctxs, count);
		}
	}
	return 1;
}


