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

#if !defined(POLL_ASYNC_USE_KQUEUE) && !defined(POLL_ASYNC_USE_EVENTSELECT)  && !defined(POLL_ASYNC_USE_EPOLL)
#if defined(__APPLE__)
#define POLL_ASYNC_USE_KQUEUE
#elif defined(_WIN32)
#define POLL_ASYNC_USE_EVENTSELECT
#else 
#define POLL_ASYNC_USE_EPOLL
#endif
#endif

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netdb.h>

#ifdef POLL_ASYNC_USE_KQUEUE
#include <sys/event.h>
#else
#include <sys/epoll.h>
#endif
#endif

#ifdef __cplusplus
extern "C" {
#endif 


#if defined(POLL_ASYNC_USE_EVENTSELECT) 
	typedef DWORD poll_event; 
	typedef struct _poll_looper* poll_looper_t;
#elif defined(POLL_ASYNC_USE_KQUEUE) 	
	typedef struct kevent poll_event;
	typedef void* poll_looper_t;
#elif defined(POLL_ASYNC_USE_EPOLL) 
	typedef struct epoll_event poll_event;
	typedef void* poll_looper_t;
#endif

#ifndef _FD_T_DEFINED_
#define _FD_T_DEFINED_
#ifdef _WIN32
	typedef SOCKET fd_t;
#else
	typedef int fd_t;
#endif
#endif


#define POLL_READABLE     1
#define POLL_WRITEABLE    2
#define POLL_ERROR        4

#define POLL_FLAG_ONCE   1


typedef void (*poll_callback)(void* data);

typedef struct _poll_ctx {
	poll_callback cb;
	void* udata;
	int ready;	
	int events;
	int error;
	int flags;
	fd_t fd;
}poll_ctx;

	/** get main looper (for main thread use, do not destroy) */
	poll_looper_t poll_looper_get_main();

	/** create poll event looper */
	int poll_looper_create(poll_looper_t* looper);

	/** destroy poll event looper */
	int poll_looper_destroy(poll_looper_t looper);

	/** wait events, results[out]; count[in,out] ; timeout is in milliseconds */
	int poll_looper_wait_events(poll_looper_t looper, poll_event* results, int* count, int timeout);

	/** process events, results[in] */
	int poll_looper_process_events(poll_looper_t looper, poll_event* results, int count);
	
	/** register fd to poll looper */
	int poll_register(poll_looper_t looper, poll_ctx* outctx, fd_t fd, int flag, int events, poll_callback cb, void* udata);
	
	/** unregister from poll looper */
	int poll_unregister(poll_looper_t looper, poll_ctx* ctx);


#if defined(POLL_ASYNC_USE_EVENTSELECT)
	/** wake up the waiting operation */
	int poll_looper_wake(poll_looper_t looper);
#else 
	/* to wake up epoll/kqueue waiting operation, use pipe|eventfd etc. */

	/* use this fd to poll the poll_looper_t has events */
	int poll_looper_fd(poll_looper_t looper);

#endif	
	
#ifdef __cplusplus
}
#endif 

