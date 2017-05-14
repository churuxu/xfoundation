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


#define _FILE_OFFSET_BITS 64
#define _LARGEFILE64_SOURCE

#include "pollasync.h"

#ifdef POLL_ASYNC_USE_KQUEUE

#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <fcntl.h>
#include <stdio.h>
#include <errno.h>


#define IO_DEBUG(...)  //printf(__VA_ARGS__)

#define last_error() (errno?errno:-1)

#define return_error(code, opt) {ret = code; IO_DEBUG("%s error %d\n", #opt, ret); return ret;}


poll_looper_t poll_looper_get_main() {
	static poll_looper_t mainlooper;
	if (!mainlooper) {
		poll_looper_create(&mainlooper);
	}
	return mainlooper;
}


int poll_looper_create(poll_looper_t* looper) {
	int ret;
	int efd = kqueue();
	if (efd >= 0) {
		*looper = (poll_looper_t)((intptr_t)efd);
		return 0;
	}
	return_error(errno, "kqueue");
}

int poll_looper_destroy(poll_looper_t looper) {
	close((int)looper);
	return 0;
}

int poll_looper_fd(poll_looper_t looper) {
	return (int)looper;
}

int poll_looper_wait_events(poll_looper_t looper, poll_event* evs, int* count, int waittime) {
    int ret;
    struct timespec ts;
	ts.tv_sec = waittime / 1000;
	ts.tv_nsec = (waittime % 1000) * 1000000;
	int nfd = kevent((int)looper, NULL, 0, evs, *count, &ts);
	*count = nfd;
	IO_DEBUG("[IO]kevent waited:%d\n", nfd);
	if (nfd>0)return 0;
	return_error(errno, "kevent");
}


int poll_looper_process_events(poll_looper_t looper, poll_event* results, int count) {
	int i;
	poll_event* pev;
	for (i = 0; i<count; i++) {
		pev = &(results[i]);
		poll_ctx* ctx = (poll_ctx*)(pev->udata);
		ctx->events = 0;
		ctx->error = 0;
		if (pev->flags & EV_ERROR) {
			ctx->error = -1;
			ctx->events |= POLL_ERROR;
		}
		if (pev->filter & EVFILT_READ) {
			ctx->events |= POLL_READABLE;
		}
		if (pev->filter & EVFILT_WRITE) {
			ctx->events |= POLL_WRITEABLE;
		}
		if (ctx->flags & POLL_FLAG_ONCE) {
			//epoll_ctl(eq_handle_, EPOLL_CTL_DEL, ctx->fd, NULL);
		}
		if (ctx->cb)ctx->cb(ctx->udata);
	}
	return 0;
}

int poll_unregister(poll_looper_t looper, poll_ctx* ctx){
	int ret;
	struct kevent ev;	
	ev.ident = ctx->fd;
	ev.flags = EV_DELETE;
	ret = kevent((int)looper, &ev, 1, NULL, 0, NULL);
	if (!ret) return 0;
	//return_error(errno, "kevent del");
    return 0;
}

int poll_register(poll_looper_t looper, poll_ctx* ctx, fd_t fd, int flag, int events, poll_callback cb, void* udata) {	
	int ret;
	struct kevent ev;

	ctx->fd = fd;
	ctx->cb = cb;
	ctx->udata = udata;
	ctx->flags = flag;
	ctx->error = 0;
	ctx->events = 0;
	ctx->ready = 0;

	ev.ident = fd;
	ev.flags = EV_ADD;
	ev.filter = 0;
    ev.udata = ctx;
    ev.data = 0;
    ev.fflags = 0;
    
	if (events & POLL_READABLE) {
		ev.filter |= EVFILT_READ;
	}
	if (events & POLL_WRITEABLE) {
		ev.filter |= EVFILT_WRITE;
	}
	if (flag & POLL_FLAG_ONCE) {
		ev.flags |= EV_ONESHOT;
	}
	ret = kevent((int)looper, &ev, 1, NULL, 0, NULL);
	if (!ret) return 0;
	return_error(errno, "kevent add");
}


#endif

