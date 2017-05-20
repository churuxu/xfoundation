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
#if defined(_MSC_VER) && (_MSC_VER < 1900)
#define snprintf(buf,buflen,...) _snprintf_s(buf,buflen,buflen,__VA_ARGS__)
#endif

#define _FILE_OFFSET_BITS 64
#define _LARGEFILE64_SOURCE

#include "ioasync.h"


#ifdef IO_ASYNC_USE_POLLASYNC
#include "pollasync.h"

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#ifndef _WIN32
#include <fcntl.h>
#include <unistd.h>
#endif
#ifdef __APPLE__
#define lseek64 lseek
#endif

#define IO_DEBUG(...)  //printf(__VA_ARGS__)

#ifdef _WIN32
#define last_error() (GetLastError()?GetLastError():-1)
#define return_inval() return ERROR_INVALID_PARAMETER
#define is_valid_socket(s) (s!=INVALID_SOCKET)
#define io_is_in_progress(code) ((code) == WSAEINPROGRESS || (code) == WSAEWOULDBLOCK)
#define io_is_would_block(code) ((code) == WSAEWOULDBLOCK || (code) == ERROR_IO_PENDING) 

static int io_set_nonblocking(fd_t fd) {
	u_long nblock = 1;
	int ret = ioctlsocket(fd, FIONBIO, &nblock);
	if (ret)return WSAGetLastError();
	return 0;
}

#else
#define last_error() (errno?errno:-1)
#define return_inval() return EINVAL
#define is_valid_socket(s) (s>=0)
#define io_is_in_progress(code) ((code) == EINPROGRESS)
#define io_is_would_block(code) ((code) == EWOULDBLOCK) 

static int io_set_nonblocking(fd_t fd) {
	int flags = fcntl(fd, F_GETFL, 0);
	if (flags < 0) return -1;
	if (fcntl(fd, F_SETFL, flags | O_NONBLOCK) < 0)return -1;
	return 0;
}

#endif


typedef struct io_future_impl {
	io_callback cb;
	void* udata;
	int ready;
	int error;
	size_t length;	
	io_ctx* io;
	char* buf;
	size_t buflen;
	fd_t* outfd;
	struct sockaddr* addrbuf;
	int* paddrlen;
	int addrlen;	
}io_future_impl;

typedef char accert_ctx_size[(sizeof(io_future) >= sizeof(io_future_impl)) ? 1 : -1];

#define future_get_fd(pfut) (pfut->io->pollctx.fd)

#define goto_error(code, opt) {ret = code; IO_DEBUG("%s error %d\n", #opt, ret); goto error;}

#define return_error(code, opt) {ret = code; ctx->ready = 1; ctx->error = ret?ret:-1; IO_DEBUG("%s error %d\n", #opt, ret); return ctx->error;}
#define return_ok() {ctx->ready = 1; ctx->error = 0; return 0;}

int io_async_init(io_ctx* outctx, io_looper_t looper, fd_t fd, int flags) {	
	io_set_nonblocking(fd);
	outctx->looper = looper;
	outctx->pollctx.fd = fd;
	outctx->flags = flags;
	return 0;
}


#ifndef _WIN32 

static void io_handle_read(void* arg) {
	io_future_impl* ctx = (io_future_impl*)arg;
	int ret = (int)read(future_get_fd(ctx), ctx->buf, ctx->buflen);
	if (ret<0) {
		ctx->error = last_error();
		ctx->length = 0;
	}
	else {
		ctx->error = 0;
		ctx->length = ret;
	}
	ctx->ready = 1;
	if (ctx->cb)ctx->cb(ctx->udata);
}

static void io_handle_write(void* arg) {
	io_future_impl* ctx = (io_future_impl*)arg;
	int ret = (int)write(future_get_fd(ctx), ctx->buf, ctx->buflen);
	if (ret == ctx->buflen) {
		ctx->error = 0;
		ctx->ready = 1;
		if (ctx->cb)ctx->cb(ctx->udata);
		return;
	}
	if (ret < 0) {
		ctx->error = last_error();
		ctx->ready = 1;
		if (ctx->cb)ctx->cb(ctx->udata);
		return;
	}
	ret = poll_register(ctx->io->looper, &ctx->io->pollctx, future_get_fd(ctx), POLL_FLAG_ONCE, POLL_WRITEABLE, io_handle_write, ctx);
	if (ret) {
		ctx->error = ret;
		ctx->ready = 1;
		if (ctx->cb)ctx->cb(ctx->udata);
	}
}


int io_async_read(io_ctx* ioctx, uint64_t offset, char* buf, size_t buflen, io_future* pctx) {
	int ret;
	int errc;
	io_future_impl* ctx;
	fd_t fd;
	if (!ioctx || !buf || !pctx)return_inval();
	ctx = (io_future_impl*)pctx;
	fd = ioctx->pollctx.fd;

	if (ioctx->flags & IO_FLAG_RANDOM_ACCESS) {
		lseek64(fd, offset, SEEK_SET);
	}
	ret = (int)read(fd, buf, buflen);
	if (ret >= 0) {
		ctx->length = ret;
		return_ok();
	}
	errc = last_error();
	if (io_is_would_block(errc)) {
		ctx->io = ioctx;
		ctx->buf = buf;
		ctx->buflen = buflen;
		ctx->ready = 0;
		return poll_register(ctx->io->looper, &ctx->io->pollctx, fd, POLL_FLAG_ONCE, POLL_READABLE, io_handle_read, ctx);
	}
	return_error(errc, "read");
}



int io_async_write(io_ctx* ioctx, uint64_t offset, const char* buf, size_t buflen, io_future* pctx) {
	int ret;
	io_future_impl* ctx;
	fd_t fd;
	if (!ioctx || !buf || !pctx)return_inval();
	ctx = (io_future_impl*)pctx;
	fd = ioctx->pollctx.fd;

	if (ioctx->flags & IO_FLAG_RANDOM_ACCESS) {
		lseek64(fd, offset, SEEK_SET);
	}
	ret = (int)write(fd, buf, buflen);
	if (ret == buflen) {
		ctx->length = ret;
		return_ok();
	}
	else if (ret >= 0) {
		ctx->io = ioctx;
		ctx->buf = (char*)buf;
		ctx->length = buflen;
		ctx->buflen = buflen - ret;
		ctx->ready = 0;
		return poll_register(ctx->io->looper, &ctx->io->pollctx, fd, POLL_FLAG_ONCE, POLL_WRITEABLE, io_handle_write, ctx);
	}
	return_error(last_error(), "write");
}

#else //win32 

int io_async_read(io_ctx* ioctx, uint64_t offset, char* buf, size_t buflen, io_future* pctx) {
	BOOL bret;
	int ret;	
	DWORD len = 0;
	io_future_impl* ctx;
	fd_t fd;
	if (!ioctx || !buf || !pctx)return_inval();
	ctx = (io_future_impl*)pctx;
	fd = ioctx->pollctx.fd;

	if (ioctx->flags & IO_FLAG_RANDOM_ACCESS) {
		LARGE_INTEGER fp;
		fp.QuadPart = offset;
		if (!SetFilePointerEx((HANDLE)fd, fp, NULL, FILE_BEGIN)) {
			return_error(GetLastError(), "SetFilePointerEx");
		}
	}
	bret = ReadFile((HANDLE)fd, buf, (DWORD)buflen, &len, NULL);
	if (bret) {
		ctx->length = len;
		return_ok();
	}
	return_error(last_error(), "ReadFile");
}


int io_async_write(io_ctx* ioctx, uint64_t offset, const char* buf, size_t buflen, io_future* pctx) {
	BOOL bret;
	int ret;	
	DWORD len = 0;
	io_future_impl* ctx;
	fd_t fd;
	if (!ioctx || !buf || !pctx)return_inval();
	ctx = (io_future_impl*)pctx;
	fd = ioctx->pollctx.fd;

	if (ioctx->flags & IO_FLAG_RANDOM_ACCESS) {
		LARGE_INTEGER fp;
		fp.QuadPart = offset;
		if (!SetFilePointerEx((HANDLE)fd, fp, NULL, FILE_BEGIN)) {
			return_error(GetLastError(), "SetFilePointerEx");
		}
	}	
	bret = WriteFile((HANDLE)fd, buf, (DWORD)buflen, &len, NULL);
	if (bret) { 
		ctx->length = len;
		return_ok();
	}
	return_error(last_error(), "ReadFile");
}


#endif


static void io_handle_connect(void* arg) {
	io_future_impl* ctx = (io_future_impl*)arg;
	ctx->error = ctx->io->pollctx.error;
	ctx->ready = 1;
	if (ctx->cb)ctx->cb(ctx->udata);
}
int io_async_connect(io_ctx* ioctx, const struct sockaddr* addr, int addrlen, io_future* pctx) {
	int ret;
	int errc;
	io_future_impl* ctx;
	fd_t fd;
	if (!ioctx || !addr || !addrlen)return_inval();
	ctx = (io_future_impl*)pctx;
	fd = ioctx->pollctx.fd;

	ret = (int)connect(fd, addr, addrlen);
	if (ret == 0) return_ok();
	errc = last_error();
	if (io_is_in_progress(errc)) {
		ctx->ready = 0;
		ctx->io = ioctx;
		return poll_register(ctx->io->looper, &ctx->io->pollctx, fd, POLL_FLAG_ONCE, POLL_WRITEABLE, io_handle_connect, ctx);
	}
	return_error(errc, "connect");
}


static void io_handle_accept(void* arg) {
	io_future_impl* ctx = (io_future_impl*)arg;
	socklen_t addrlen = ctx->paddrlen?*ctx->paddrlen:0;
	fd_t fd = future_get_fd(ctx);
	fd_t client = accept(fd, ctx->addrbuf, &addrlen);
	if (!is_valid_socket(client)) {
		ctx->error = last_error();
		if (io_is_in_progress(ctx->error)) {
			if (0 == poll_register(ctx->io->looper, &ctx->io->pollctx, fd, POLL_FLAG_ONCE, POLL_READABLE, io_handle_accept, ctx)) {
				return;
			}
		}
		else {
			IO_DEBUG("accept error(%d)\n", last_error());
			*ctx->outfd = 0;
		}
	}
	else {
		ctx->error = 0;
		*ctx->outfd = client;
		if (ctx->paddrlen)*ctx->paddrlen = addrlen;
	}
	ctx->ready = 1;
	if (ctx->cb)ctx->cb(ctx->udata);
}
int io_async_accept(io_ctx* ioctx, fd_t* outfd, struct sockaddr* addrbuf, int* paddrlen, io_future* pctx) {
	int ret;	
	int errc;
	io_future_impl* ctx;
	fd_t client;
	fd_t fd;
	socklen_t addrlen = paddrlen ? *paddrlen : 0;
	if (!outfd || !pctx)return_inval();

	ctx = (io_future_impl*)pctx;
	fd = ioctx->pollctx.fd;

	client = accept(fd, addrbuf, &addrlen);
	if (is_valid_socket(client)) {
		*outfd = client;
		*paddrlen = addrlen;
		return_ok();
	}
	errc = last_error();
	if (io_is_would_block(errc)) {
		ctx->io = ioctx;
	    ctx->outfd = outfd;
		ctx->addrbuf = addrbuf;
		ctx->paddrlen = paddrlen;
		ctx->ready = 0;
		return poll_register(ctx->io->looper, &ctx->io->pollctx, fd, POLL_FLAG_ONCE, POLL_READABLE, io_handle_accept, ctx);
	}	
	return_error(errc, "accept");
}



static void io_handle_recv(void* arg) {
	io_future_impl* ctx = (io_future_impl*)arg;	
	fd_t fd = future_get_fd(ctx);
	int ret = (int)recv(fd, ctx->buf, (int)ctx->buflen, 0);
	if (ret<0) {
		IO_DEBUG("recv error(%d)\n", last_error());
		ctx->error = last_error();	
		ctx->length = 0;
	}
	else {
		ctx->error = 0;
		ctx->length = ret;		
	}
	ctx->ready = 1;
	if (ctx->cb)ctx->cb(ctx->udata);
}

int io_async_recv(io_ctx* ioctx, char* buf, size_t buflen, io_future* pctx) {
	int ret;
	int errc;	
	io_future_impl* ctx;
	fd_t fd;
	if (!buf || !pctx)return_inval();
	ctx = (io_future_impl*)pctx;
	fd = ioctx->pollctx.fd;

	ret = (int)recv(fd, buf, (int)buflen, 0);
	if (ret >= 0) {
		ctx->length = ret;
		return_ok();
	}
	errc = last_error();
	if (io_is_would_block(errc)) {	
		ctx->io = ioctx;
		ctx->buf = buf;	
		ctx->buflen = buflen;
		ctx->ready = 0;
		return poll_register(ctx->io->looper, &ctx->io->pollctx, fd, POLL_FLAG_ONCE, POLL_READABLE, io_handle_recv, ctx);
	}
	return_error(errc, "recv");
}


static void io_handle_send(void* arg) {
	io_future_impl* ctx = (io_future_impl*)arg;
	fd_t fd = future_get_fd(ctx);
	int ret = (int)send(fd, ctx->buf, (int)ctx->buflen, 0);
	if (ret == ctx->buflen) {		
		ctx->error = 0;
		ctx->ready = 1;
		if (ctx->cb)ctx->cb(ctx->udata);
		return;
	}
	if (ret < 0) {
		ctx->error = last_error();
		ctx->ready = 1;
		if (ctx->cb)ctx->cb(ctx->udata);
		return;
	}
	ret = poll_register(ctx->io->looper, &ctx->io->pollctx, fd, POLL_FLAG_ONCE, POLL_WRITEABLE, io_handle_send, ctx);
	if (ret) {
		ctx->error = ret;
		ctx->ready = 1;
		if (ctx->cb)ctx->cb(ctx->udata);
	}	
}

int io_async_send(io_ctx* ioctx, const char* buf, size_t buflen, io_future* pctx) {
	int ret;	
	io_future_impl* ctx;	
	fd_t fd;
	if (!buf || !pctx)return_inval();
	ctx = (io_future_impl*)pctx;
	fd = ioctx->pollctx.fd;
	ret = (int)send(fd, buf, (int)buflen, 0);
	if (ret == buflen) {
		ctx->length = ret;
		return_ok();
	}else if (ret >=0 ){	
		ctx->io = ioctx;
		ctx->buf = (char*)buf;
		ctx->length = buflen;
		ctx->buflen = buflen - ret;
		ctx->ready = 0;
		return poll_register(ctx->io->looper, &ctx->io->pollctx, fd, POLL_FLAG_ONCE, POLL_WRITEABLE, io_handle_send, ctx);
	}
	return_error(last_error(), "send");
}


static void io_handle_recvfrom(void* arg) {
	io_future_impl* ctx = (io_future_impl*)arg;
	socklen_t addrlen = ctx->paddrlen ? *ctx->paddrlen : 0;
	fd_t fd = future_get_fd(ctx);
	int ret = (int)recvfrom(fd, ctx->buf, (int)ctx->buflen, 0, ctx->addrbuf, &addrlen);
	if (ret<0) {
		ctx->error = last_error();		
	}
	else {
		ctx->error = 0;
		ctx->length = ret;	
		if(ctx->paddrlen)*ctx->paddrlen = addrlen;
	}
	ctx->ready = 1;
	if (ctx->cb)ctx->cb(ctx->udata);
}

int io_async_recvfrom(io_ctx* ioctx, char* buf, size_t buflen, struct sockaddr* addrbuf, int* paddrlen, io_future* pctx) {
	int ret;
	int errc;
	io_future_impl* ctx;
	fd_t fd;
	socklen_t addrlen = paddrlen ? *paddrlen : 0;
	if (!buf || !pctx)return_inval();
	ctx = (io_future_impl*)pctx;
	fd = ioctx->pollctx.fd;
	ret = (int)recvfrom(fd, buf, (int)buflen, 0, addrbuf, &addrlen);
	if (ret >= 0) {
		ctx->length = ret;
		if (paddrlen)*paddrlen = addrlen;		
		return_ok();
	}
	errc = last_error();
	if (io_is_would_block(errc)) {
		ctx->io = ioctx;
		ctx->buf = buf;
		ctx->buflen = buflen;
		ctx->addrbuf = addrbuf;
		ctx->paddrlen = paddrlen;
		ctx->ready = 0;
		return poll_register(ctx->io->looper, &ctx->io->pollctx, fd, POLL_FLAG_ONCE, POLL_READABLE, io_handle_recvfrom, ctx);
	}
	return_error(errc, "recvfrom");
}


static void io_handle_sendto(void* arg) {
	io_future_impl* ctx = (io_future_impl*)arg;
	fd_t fd = future_get_fd(ctx);
	int ret = (int)sendto(fd, ctx->buf, (int)ctx->buflen, 0, ctx->addrbuf, ctx->addrlen);
	if (ret < 0) {
		ctx->error = last_error();		
	}
	else {
		ctx->error = 0;
		ctx->length = ret;
	}
	ctx->ready = 1;
	if (ctx->cb)ctx->cb(ctx->udata);
}

int io_async_sendto(io_ctx* ioctx, const char* buf, size_t buflen, const struct sockaddr* addr, int addrlen, io_future* pctx) {
	int ret;
	int errc;
	io_future_impl* ctx;
	fd_t fd;
	if (!buf || !pctx)return_inval();
	ctx = (io_future_impl*)pctx;
	fd = ioctx->pollctx.fd;

	ret = (int)sendto(fd, buf, (int)buflen, 0, addr, addrlen);
	if (ret >= 0) {
		ctx->length = ret;		
		return_ok();
	}
	errc = last_error();
	if (io_is_would_block(errc)) {
		ctx->io = ioctx;
		ctx->buf = (char*)buf;
		ctx->buflen = buflen;
		ctx->addrbuf = (struct sockaddr*)addr;
		ctx->addrlen = addrlen;
		ctx->ready = 0;
		return poll_register(ctx->io->looper, &ctx->io->pollctx, fd, POLL_FLAG_ONCE, POLL_WRITEABLE, io_handle_sendto, ctx);
	}
	return last_error();
}



#endif

