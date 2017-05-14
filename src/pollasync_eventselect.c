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

#include "pollasync.h"

#ifdef POLL_ASYNC_USE_EVENTSELECT

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>



#define IO_DEBUG(...) // printf(__VA_ARGS__);

#define last_error() (GetLastError()?GetLastError():-1)


#define MAX_POLL_EVENTS 64

struct _poll_looper{
	DWORD pollcount_; //count
	HANDLE pollevents_[MAX_POLL_EVENTS]; //event object array
	poll_ctx* pollctxs_[MAX_POLL_EVENTS]; //ctx array
};


poll_looper_t poll_looper_get_main(){
	static poll_looper_t mainlooper;
	if (!mainlooper) {
		poll_looper_create(&mainlooper);
	}
	return mainlooper;
}

int poll_looper_create(poll_looper_t* looper){	
	HANDLE hevent ;
	poll_looper_t plooper;
	*looper = NULL;
	if(!looper)return ERROR_INVALID_PARAMETER;
	hevent = CreateEvent(NULL, FALSE, FALSE, NULL);
	if(!hevent)return last_error();
	plooper = (poll_looper_t)malloc(sizeof(struct _poll_looper));
	if(!plooper){ CloseHandle(hevent); return ERROR_OUTOFMEMORY; }
	ZeroMemory(plooper, sizeof(struct _poll_looper));
	plooper->pollevents_[0] = hevent;
	plooper->pollcount_ = 1;
	*looper = plooper;
	return 0;
}


int poll_looper_destroy(poll_looper_t looper){
	int maxcount = MAX_POLL_EVENTS;
	HANDLE* phevent = looper->pollevents_;
	while(*phevent && maxcount){
		CloseHandle(*phevent);
		phevent ++;
		maxcount --;
	}
	free(looper);
	return 0;
}

int poll_looper_wake(poll_looper_t looper){
	BOOL bret = SetEvent(looper->pollevents_[0]);
	if(!bret)return last_error();
	return 0;
}

static int poll_looper_clear_null_ctx(poll_looper_t looper) {
	DWORD l = 1, r = 0;
	HANDLE hev;
	poll_ctx* ctx;
	for (; r < looper->pollcount_; r++) {
		ctx = looper->pollctxs_[r];
		if (ctx) {
			if (r != l) {
				looper->pollctxs_[l] = ctx;				
				hev = looper->pollevents_[r];
				looper->pollevents_[r] = looper->pollevents_[l];
				looper->pollevents_[l] = hev;
			}
			l++;
		}
	}
	for (r = l; r < looper->pollcount_; r++) {
		looper->pollctxs_[r] = NULL;
	}
	looper->pollcount_ = l;
	return 0;
}

int poll_looper_wait_events(poll_looper_t looper, poll_event* results, int* count, int timeout){
	DWORD dwret;
	
	IO_DEBUG("wait object %d\n", looper->pollcount_);
#ifdef POLL_AYSNC_WITH_WIN32_MSG
	dwret = MsgWaitForMultipleObjectsEx(looper->pollcount_, looper->pollevents_, timeout, QS_ALLEVENTS, MWMO_ALERTABLE | MWMO_INPUTAVAILABLE);
#else
	dwret = WaitForMultipleObjectsEx(looper->pollcount_, looper->pollevents_, FALSE, timeout, TRUE);
#endif
	if (dwret >= WAIT_OBJECT_0 && dwret < (WAIT_OBJECT_0 + MAX_POLL_EVENTS)) {
		*results = dwret;
		*count = 1;
		return 0;
	}
	return last_error();
}



#define wsaevents_to_pollevents_(ctx,  pev, wsaevs, wsaev, wsabit) {\
	int errorc = wsaevs.iErrorCode[wsabit];\
	if (wsaevs.lNetworkEvents & wsaev) {\
		if (errorc) {\
			ctx->events |= POLL_ERROR;\
			ctx->error = errorc;\
		}\
		else {\
			ctx->events |= pev;\
		}\
	}\
}

int poll_looper_process_events(poll_looper_t looper, poll_event* results, int count) { //run in main thread
	WSANETWORKEVENTS wsaevents;
	DWORD index = *results;
	poll_ctx* ctx = looper->pollctxs_[index];
	if (ctx) {
		if (0 == WSAEnumNetworkEvents(ctx->fd, looper->pollevents_[index], &wsaevents)) {
			ctx->error = 0;
			ctx->events = 0;
			wsaevents_to_pollevents_(ctx, POLL_READABLE, wsaevents, FD_READ, FD_READ_BIT);
			wsaevents_to_pollevents_(ctx, POLL_WRITEABLE, wsaevents, FD_WRITE, FD_WRITE_BIT);
			wsaevents_to_pollevents_(ctx, POLL_READABLE, wsaevents, FD_ACCEPT, FD_ACCEPT_BIT);
			wsaevents_to_pollevents_(ctx, POLL_WRITEABLE, wsaevents, FD_CONNECT, FD_CONNECT_BIT);
			wsaevents_to_pollevents_(ctx, POLL_READABLE | POLL_WRITEABLE, wsaevents, FD_CLOSE, FD_CLOSE_BIT);
			IO_DEBUG("fd:%d events:%d\n", (int)ctx->fd, ctx->events);
		}
		else {
			ctx->events = POLL_ERROR;
			ctx->error = last_error();
		}

		if (ctx->events) {
			if((ctx->flags & POLL_FLAG_ONCE)){
				looper->pollctxs_[index] = NULL;
			}

			if(ctx->cb)ctx->cb(ctx->udata);
		}		
	}	
	poll_looper_clear_null_ctx(looper);	
	return 0;
}


static long pollevents__to_wsaevents(int pollev) {
	long events = FD_CLOSE;
	if (pollev & POLL_WRITEABLE) {
		events |= FD_WRITE;
		events |= FD_CONNECT;
	}
	if (pollev & POLL_READABLE) {
		events |= FD_READ;
		events |= FD_ACCEPT;
	}
	return events;
}

int poll_register(poll_looper_t looper, poll_ctx* outctx, fd_t fd, int flag, int events, poll_callback cb, void* udata){
	DWORD i;
	int ret, newcount;
	long newwsaevents = 0;
	HANDLE newevent = NULL;
	poll_ctx* ctx = NULL;
	int finded = 0;
	ret = 0;

	for (i = 0; i < looper->pollcount_; i++) {
		ctx = looper->pollctxs_[i];
		if (ctx && ctx->fd == fd) {
			//finded
			if(ctx != (poll_ctx*)outctx){
				ctx = (poll_ctx*)outctx;
				ctx->cb = cb;
				ctx->udata = udata;
				ctx->fd = fd;
				looper->pollctxs_[i] = ctx;
			}
			finded = 1;
			break;
		}
	}
	if(finded){
		newwsaevents = pollevents__to_wsaevents(events);
		newevent = looper->pollevents_[i];
	}else{
		//not find,  add pollctx to array
		if(looper->pollcount_ >= MAX_POLL_EVENTS)return ERROR_NETWORK_BUSY;
		newwsaevents = pollevents__to_wsaevents(events);
		ctx = (poll_ctx*)outctx;
		ctx->cb = cb;
		ctx->udata = udata;
		ctx->fd = fd;
		ctx->error = 0;
		ctx->events = 0;
		ctx->ready = 0;
		newcount = looper->pollcount_;
		if (!looper->pollevents_[newcount]) {
			newevent = CreateEvent(NULL, FALSE, FALSE, NULL);
			if(!newevent)return last_error();
			looper->pollevents_[newcount] = newevent;
		}else{
			newevent = looper->pollevents_[newcount];
		}
		looper->pollctxs_[newcount] = ctx;
		looper->pollcount_++;
	}
	ctx->flags = flag;
	IO_DEBUG("WSAEventSelect(%d %p %d)\n", (int)fd, newevent, (int)newwsaevents);
	ret = WSAEventSelect(fd, newevent, newwsaevents);
	if(ret)return last_error();
	//wake wait operation
	SetEvent(looper->pollevents_[0]);
	return 0;
}

int poll_unregister(poll_looper_t looper, poll_ctx* uctx){
	DWORD i;
	int ret = 0;
	poll_ctx* ctx = NULL;

	for (i = 0; i < looper->pollcount_; i++) {
		ctx = looper->pollctxs_[i];
		if (ctx && ctx->fd == uctx->fd) {
			looper->pollctxs_[i] = NULL;
			IO_DEBUG("WSAEventSelect(%d NULL 0)\n", (int)ctx->fd);
			ret = WSAEventSelect(ctx->fd, NULL, 0);
			if(ret)ret = last_error();
			break;
		}
	}
	return ret;
}





#endif


