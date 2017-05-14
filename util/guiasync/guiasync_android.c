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

#ifdef __ANDROID__


#include "guiasync.h"
#include "pollasync.h"
#include "clocks.h"
#include "timerqueue.h"
#include "ioutil.h"
#include <android/log.h>
#include <android/looper.h>
#include <unistd.h>
#include <stdlib.h>
#include <fcntl.h>
#include <stdio.h>
#include <errno.h>
#include <pthread.h>



#define LOGE(opt)  __android_log_print(ANDROID_LOG_ERROR, "test", "%s", opt)
#define check(exp, opt) if(!(exp)){LOGE(opt);goto error;}
#define last_error() errno?errno:-1

#define ensure(exp)  if(!(exp)){ LOGE(#exp); return last_error();}


typedef struct _callback_context {	
	gui_async_callback cb;
	void* udata;
}callback_context;

static ALooper* main_looper_;
static int cmd_fd_in_; //for write
static int cmd_fd_out_; //for read
static pthread_cond_t waitcond_;
static pthread_mutex_t waitmtx_;
static int nextdelay_;
static int timerqueueready_;

//handle command from other thread
static int on_command_fd_readable(int fd, int events, void* data){    
	int ret = 0;	
	while (1) {
		callback_context cmd;
		ret = (int)read(fd, &cmd, sizeof(cmd));
		if (ret == sizeof(cmd)) {
			cmd.cb(cmd.udata);	
		}
		ret = 0;
		break;
	} 
    return 1;
}

//handle service fd
static int on_io_service_fd_readable(int fd, int events, void* data) {
	poll_event evs[32];
	int count = 32;	
	poll_looper_t looper = (poll_looper_t)data;
	if (0 == poll_looper_wait_events(looper, evs, &count, 0) ){
		poll_looper_process_events(looper, evs, count);
	}
	return 1;
}

static void on_timer_queue_ready(void* arg) {
	int nextdelay = 60000;
	timer_queue_t timerqueue = (timer_queue_t)arg;
	timer_queue_process(timerqueue, &nextdelay);
	timerqueueready_ = 0;
}

static void on_timer_queue_change(void* udata, int newdelay) {
	pthread_mutex_lock(&waitmtx_);
	nextdelay_ = newdelay;
	pthread_mutex_unlock(&waitmtx_);
	pthread_cond_signal(&waitcond_);
}

static void* delay_wait_thread(void* arg) {
	struct timespec ts;
	int delay;
	while (1) {
		pthread_mutex_lock(&waitmtx_);
		delay = nextdelay_;
		if (delay) {
			ts.tv_sec = delay / 1000;
			ts.tv_nsec = (delay % 1000) * 1000000;
		}
		else {
			delay = 1000;
			ts.tv_sec = 1;
			ts.tv_nsec = 0;
		}
		//arm64 has no pthread_cond_timeout_np
#ifdef __LP64__
		pthread_cond_timedwait(&waitcond_, &waitmtx_, &ts);
#else
		pthread_cond_timeout_np(&waitcond_, &waitmtx_, delay);
#endif
		pthread_mutex_unlock(&waitmtx_);
		if (!timerqueueready_) {
			timerqueueready_ = 1;
			gui_async_post_callback(on_timer_queue_ready, arg);
		}
	}
	return NULL;
}


int gui_async_init(){
    int pipefd[2]; 
	pthread_t th;
	pthread_condattr_t condattr;
	poll_looper_t polllooper;
	timer_queue_t timerqueue;
	if (main_looper_)return 0;


    ensure(main_looper_ = ALooper_forThread());
	polllooper = poll_looper_get_main();
	timerqueue = timer_queue_get_main();

	ensure(polllooper);
	ensure(timerqueue);
	timer_queue_set_clock(timerqueue, clock_get_tick);
	timer_queue_set_observer(timerqueue, on_timer_queue_change, NULL);
	nextdelay_ = 60000;

#ifdef __LP64__
	//arm has no pthread_condattr_setclock
	ensure(0 == pthread_condattr_init(&condattr));
	ensure(0 == pthread_condattr_setclock(&condattr, CLOCK_MONOTONIC));
	ensure(0 == pthread_cond_init(&waitcond_, &condattr));
	pthread_condattr_destroy(&condattr);
#else
	(void)condattr;
	ensure(0 == pthread_cond_init(&waitcond_, NULL));
#endif
	ensure(0 == pthread_mutex_init(&waitmtx_, NULL));

	ensure(0 == pthread_create(&th, NULL, delay_wait_thread, timerqueue));
	
	//pthread_detach(th);

	ensure(0 == pipe(pipefd));   
 	cmd_fd_in_ = pipefd[1];
	cmd_fd_out_ = pipefd[0]; 
	ensure(0 == io_set_nonblocking(cmd_fd_out_));
    
	ensure(0 < ALooper_addFd(main_looper_, cmd_fd_out_, 0x1a10, ALOOPER_EVENT_INPUT, on_command_fd_readable, NULL));

	ensure(0 < ALooper_addFd(main_looper_, poll_looper_fd(polllooper), 0x1a11, ALOOPER_EVENT_INPUT, on_io_service_fd_readable, polllooper));

    return 0;
}


int gui_async_post_callback(gui_async_callback cb, void* userdata){
	callback_context cmd;
	int ret;
	if (!cb)return -1;
	cmd.cb = cb;
	cmd.udata = userdata;
	ret = (int)write(cmd_fd_in_, &cmd, sizeof(cmd));
	if (ret == sizeof(cmd))return 0;
	return last_error();
}

#endif

