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
#define last_error() GetLastError()
#else
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <errno.h>

#define last_error() errno
#endif

typedef struct {	
	poll_ctx pollctx;
	fd_t fd;
	char buf[1024];
	int buflen;	
	char addrbuf[128];
	int addrlen;	
	int step;	
	struct addrinfo* ai;
}myctx;



static void test_socket_poll_(void* arg) {
	myctx* ctx = (myctx*)arg;
	test(0 == (ctx->pollctx.events & POLL_ERROR));
	if (ctx->step == 0) {
		test(0 != (ctx->pollctx.events & POLL_WRITEABLE));
		ctx->step = 1;
		ctx->buflen = (int)send(ctx->fd, "1234567890", 10, 0);
		test(ctx->buflen == 10);
		testzero(poll_register(poll_looper_get_main(), &ctx->pollctx, ctx->fd, 0, POLL_READABLE, test_socket_poll_, ctx));
	}
	else {
		test(0 != (ctx->pollctx.events & POLL_READABLE));
		ctx->step++;		
		ctx->buflen = (int)recv(ctx->fd, ctx->buf, 1024, 0);
		test(10 == ctx->buflen);
		test(0 == memcmp("1234567890", ctx->buf, 10));
		testzero(poll_unregister(poll_looper_get_main(), &ctx->pollctx));
		closesocket(ctx->fd);
		free(ctx);
		test_ok();		
	}
	return;
error:
	if (ctx->fd)closesocket(ctx->fd);
	free(ctx);
	test_fail();
}

static void test_socket_poll(void* arg) {
	int ret;
	myctx* ctx = (myctx*)arg;
	if (!ctx) {		
		test(ctx = (myctx*)malloc(sizeof(myctx)));
		memset(ctx, 0, sizeof(myctx));
		testzero(getaddrinfo("127.0.0.1", TEST_PORT_TCP, NULL, &ctx->ai));
		ctx->fd = socket(ctx->ai->ai_family, SOCK_STREAM, 0);
		test(ctx->fd > 0);		
		ctx->step = 0;
	}	
	testzero(io_set_nonblocking(ctx->fd));
	ret = connect(ctx->fd, ctx->ai->ai_addr, (int)ctx->ai->ai_addrlen);
	if (ret) {
		test(io_is_in_progress(last_error()));
		testzero(poll_register(poll_looper_get_main(), &ctx->pollctx, ctx->fd, POLL_FLAG_ONCE, POLL_WRITEABLE, test_socket_poll_, ctx));
		return;
	}
	ctx->pollctx.events = POLL_WRITEABLE;	
	test_socket_poll_(ctx);
	return;
error:
	if (ctx->fd)closesocket(ctx->fd);
	free(ctx);
	test_fail();
}


void test_pollasync_init() {

	test_register(test_socket_poll);
}
