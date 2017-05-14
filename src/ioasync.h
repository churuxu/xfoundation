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

#if !defined(IO_ASYNC_USE_IOCP) && !defined(IO_ASYNC_USE_POLLASYNC)
#if defined(_WIN32)
#if defined(WINAPI_FAMILY) && (WINAPI_FAMILY==WINAPI_FAMILY_APP)
#define IO_ASYNC_USE_POLLASYNC
#else
#define IO_ASYNC_USE_IOCP 
#endif
#else
#define IO_ASYNC_USE_POLLASYNC
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
#include <errno.h>
#endif
#include <stdint.h>

#ifdef IO_ASYNC_USE_POLLASYNC
#include "pollasync.h"
#endif

#ifdef __cplusplus
extern "C" {
#endif 
	
#ifndef _FD_T_DEFINED_
#define _FD_T_DEFINED_
#ifdef _WIN32
	typedef SOCKET fd_t;
#else
	typedef int fd_t;
#endif
#endif



typedef void (*io_callback)(void* udata);

typedef struct io_future{
	io_callback cb; 
	void* udata;
	int ready; 
	int error;
	size_t length;
	void* _private[14];
}io_future;

		
#if defined(IO_ASYNC_USE_IOCP)  //use iocp
	typedef struct _io_ctx{
		fd_t fd;
		uint64_t offset;
		int flags;
	}io_ctx;

	typedef HANDLE io_looper_t; //iocp HANDLE as io_looper_t
	typedef OVERLAPPED_ENTRY io_event;

	/** get main looper (for main thread use, do not destroy) */
	io_looper_t io_looper_get_main();

	/** create io event looper (for other thread use) */
	int io_looper_create(io_looper_t* looper);

	/** destroy io event looper */
	int io_looper_destroy(io_looper_t looper);

	/** wait events, results[out]; count[in,out] ; timeout is in milliseconds */
	int io_looper_wait_events(io_looper_t looper, io_event* results, int* count, int timeout);

	/** process events, results[in] */
	int io_looper_process_events(io_looper_t looper, io_event* results, int count);

	/** queue callback for run, (can pass NULL to interrupt the waiting operation) */
	int io_looper_post_callback(io_looper_t looper, io_callback cb, void* udata);
	

#elif defined(IO_ASYNC_USE_POLLASYNC) 
	typedef struct _io_ctx {
		poll_looper_t looper;
		poll_ctx pollctx;
		int flags;
	}io_ctx;

	typedef poll_event io_event;
	typedef poll_looper_t  io_looper_t; //poll_looper_t as io_looper_t
	#define io_looper_get_main  poll_looper_get_main
	#define io_looper_create  poll_looper_create
	#define io_looper_destroy  poll_looper_destroy 
	#define io_looper_wait_events  poll_looper_wait_events
	#define io_looper_process_events  poll_looper_process_events

#endif

#define IO_FLAG_RANDOM_ACCESS 1

	/** prepair for async io */
	int io_async_init(io_ctx* outctx, io_looper_t looper, fd_t fd, int flags);

	/* async io apis remark

	   after call async io apis, io_future paramter will update.

	   if io complete immediately: 
	      io_future.error = system error code, 0 means ok
		  io_future.ready = 1; 
		  io_future.length = readed or writed data length; 
		  io_future.cb will not run.
		  return value = io_future.error 

	   if io not complete immediately:
		  return value = 0;
		  io_future.ready = 0;		  
		  io_future.cb will run when io complete. in this callback, 
		     io_future.ready = 1; 
			 io_future.error = system error code, 0 means ok
			 io_future.length = readed or writed data length;

	   do not free memory of io_ctx\io_future when io not complete.

	*/


	/* async file io apis remark
	   if io_ctx has no IO_FLAG_RANDOM_ACCESS flag, io_async_read/io_async_write will ignore the offset paramter.

	*/

	/** async read file */
	int io_async_read(io_ctx* ctx, uint64_t offset, char* buf, size_t buflen, io_future* fut);

	/** async write file */
	int io_async_write(io_ctx* ctx, uint64_t offset, const char* buf, size_t buflen, io_future* fut);

	/** async connect tcp socket */
	int io_async_connect(io_ctx* ctx, const struct sockaddr* addr, int addrlen, io_future* fut);

	/** async accept tcp socket */
	int io_async_accept(io_ctx* ctx, fd_t* outfd, struct sockaddr* addrbuf, int* addrlen, io_future* fut);

	/** async read tcp socket */
	int io_async_recv(io_ctx* ctx, char* buf, size_t buflen, io_future* fut);

	/** async write tcp socket */
	int io_async_send(io_ctx* ctx, const char* buf, size_t buflen, io_future* fut);

	/** async read udp socket */	
	int io_async_recvfrom(io_ctx* ctx, char* buf, size_t buflen, struct sockaddr* addrbuf, int* addrlen, io_future* fut);

	/** async write udp socket */
	int io_async_sendto(io_ctx* ctx, const char* buf, size_t buflen, const struct sockaddr* addr, int addrlen, io_future* fut);



	
#ifdef __cplusplus
}
#endif 

