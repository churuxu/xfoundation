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

#ifdef POLL_ASYNC_USE_EPOLL

#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <fcntl.h>
#include <stdio.h>
#include <errno.h>


#define IO_DEBUG(...)  //printf(__VA_ARGS__)

#define last_error() (errno?errno:-1)

#define return_error(code, opt) {ret = code; IO_DEBUG("%s error %d\n", #opt, ret); return ret;}

#define looper_to_fd(looper) (int)((intptr_t)(looper))

poll_looper_t poll_looper_get_main() {
	static poll_looper_t mainlooper;
	if (!mainlooper) {
		poll_looper_create(&mainlooper);
	}
	return mainlooper;
}

int poll_looper_create(poll_looper_t* looper) {	
	int ret;
	int efd = epoll_create(65535);
	if (efd >= 0) {
		*looper = (poll_looper_t)((intptr_t)efd);
		return 0;
	}
	return_error(errno, "epoll_create");
}

int poll_looper_destroy(poll_looper_t looper) {
	close(looper_to_fd(looper));
	return 0;
}

int poll_looper_fd(poll_looper_t looper) {
	return looper_to_fd(looper);
}

int poll_looper_wait_events(poll_looper_t looper, poll_event* evs, int* count, int timeout) {
	int ret;
	int nfd = epoll_wait(looper_to_fd(looper), evs, *count, timeout);
	*count = nfd;
	IO_DEBUG("[IO]epoll waited:%d\n", nfd);
	if (nfd>0)return 0;
	return_error(errno, "epoll_wait");
}

int poll_looper_process_events(poll_looper_t looper, poll_event* results, int count) {
	int i;
	poll_event* pev;
	for (i = 0; i<count; i++) {
		pev = &(results[i]);
		poll_ctx* ctx = (poll_ctx*)(pev->data.ptr);
		ctx->events = 0;
		ctx->error = 0;
		if (pev->events & EPOLLERR) {
			ctx->error = -1;
			ctx->events |= POLL_ERROR;
		}
		if (pev->events & EPOLLIN) {
			ctx->events |= POLL_READABLE;
		}
		if (pev->events & EPOLLOUT) {
			ctx->events |= POLL_WRITEABLE;
		}
		if (ctx->flags & POLL_FLAG_ONCE) {
			if (epoll_ctl(looper_to_fd(looper), EPOLL_CTL_DEL, ctx->fd, NULL)) {
				IO_DEBUG("epoll_ctl error %d in once\n", errno);
			}
		}
		if (ctx->cb)ctx->cb(ctx->udata);
	}
	return 0;
}

int poll_register(poll_looper_t looper, poll_ctx* ctx, fd_t fd, int flag, int events, poll_callback cb, void* udata) {
	int ret;
	struct epoll_event ev;

	ctx->fd = fd;
	ctx->cb = cb;
	ctx->udata = udata;
	ctx->flags = flag;
	ctx->error = 0;
	ctx->events = 0;
	ctx->ready = 0;

	ev.data.ptr = ctx;
	ev.events = EPOLLERR | EPOLLHUP;
	if (events & POLL_READABLE) {
		ev.events |= EPOLLIN;
	}
	if (events & POLL_WRITEABLE) {
		ev.events |= EPOLLOUT;
	}	
	ret = epoll_ctl(looper_to_fd(looper), EPOLL_CTL_ADD, fd, &ev);
	if (!ret) return 0;
	if (errno == EEXIST) {
		ret = epoll_ctl(looper_to_fd(looper), EPOLL_CTL_MOD, fd, &ev);
		if (!ret) return 0;		
	}
	return_error(errno, "epoll_ctl");	
}


int poll_unregister(poll_looper_t looper, poll_ctx* ctx) {
	int ret;
	ret = epoll_ctl(looper_to_fd(looper), EPOLL_CTL_DEL, ctx->fd, NULL);
	if (!ret) return 0;
	return_error(errno, "epoll_ctl del");
}


#endif

