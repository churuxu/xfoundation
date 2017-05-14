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


typedef struct {
	io_ctx io;
	io_future notify;	
	fd_t fd;
	char buf[1024];		
	char addrbuf[128];
	int addrlen;
	struct addrinfo* ai;
	int step;	
}myctx;



static void test_stream_file(void* arg) {
	char filename[512];
	myctx* ctx = (myctx*)arg;
	if (!ctx) {		
		test(ctx = (myctx*)malloc(sizeof(myctx)));
		memset(ctx, 0, sizeof(myctx));
		snprintf(filename, 512, "%s/%s", TEST_DIR, "stream.tmp");
		ctx->fd = open_for_write(filename);
		test(ctx->fd > 0);
		testzero(io_async_init(&ctx->io, io_looper_get_main(), ctx->fd, 0));
		ctx->step = 0;
		ctx->notify.cb = test_stream_file;
		ctx->notify.udata = ctx;
	}
	future_enter(ctx->step);
		
	testzero(io_async_write(&ctx->io, 0, "1234567890", 10, &ctx->notify));
	future_wait(&ctx->notify);
	testzero(ctx->notify.error);
	test(ctx->notify.length == 10);
		
	testzero(io_async_write(&ctx->io, 0, "0987654321", 10, &ctx->notify));
	future_wait(&ctx->notify);
	testzero(ctx->notify.error);
	test(ctx->notify.length == 10);

	closefile(ctx->fd);

	snprintf(filename, 512, "%s/%s", get_local_data_path(), "stream.tmp");
	ctx->fd = open_for_read(filename);
	test(ctx->fd > 0);
	testzero(io_async_init(&ctx->io, io_looper_get_main(), ctx->fd, 0));

	memset(&ctx->notify, 0, sizeof(ctx->notify));
	ctx->notify.cb = test_stream_file;
	ctx->notify.udata = ctx;

	testzero(io_async_read(&ctx->io, -1, ctx->buf, 1024, &ctx->notify));
	future_wait(&ctx->notify); 
	testzero(ctx->notify.error);
	test(ctx->notify.length == 20);
	test(memcmp(ctx->buf, "12345678900987654321", 20) == 0);

	future_leave();

	test_ok();
	goto clean;
error:	
	test_fail();
clean:
	if (ctx->fd)closefile(ctx->fd);
	free(ctx);	
}

static void test_random_access_file(void* arg) {
	char filename[512];
	myctx* ctx = (myctx*)arg;
	if (!ctx) {
		test(ctx = (myctx*)malloc(sizeof(myctx)));
		memset(ctx, 0, sizeof(myctx));

		snprintf(filename, 512, "%s/%s", get_local_data_path(), "randacc.tmp");
		ctx->fd = open_for_write(filename);
		test(ctx->fd > 0);
		testzero(io_async_init(&ctx->io, io_looper_get_main(), ctx->fd, IO_FLAG_RANDOM_ACCESS));
		ctx->step = 0;
		future_set_callback(&ctx->notify, test_random_access_file, ctx);
	}
	future_enter(ctx->step);

	
	testzero(io_async_write(&ctx->io, 0, "1234567890", 10, &ctx->notify));
	future_wait(&ctx->notify);
	testzero(ctx->notify.error);
	test(ctx->notify.length == 10);
	
	test(0 == io_async_write(&ctx->io, 5, "0987654321", 10, &ctx->notify));
	future_wait(&ctx->notify);
	testzero(ctx->notify.error);
	test(ctx->notify.length == 10);
		
	test(0 == io_async_write(&ctx->io, 2, "aaaaa", 5, &ctx->notify));
	future_wait(&ctx->notify);
	testzero(ctx->notify.error);
	test(ctx->notify.length == 5);

	closefile(ctx->fd);

	snprintf(filename, 512, "%s/%s", get_local_data_path(), "randacc.tmp");
	ctx->fd = open_for_read(filename);
	test(ctx->fd > 0);
	testzero(io_async_init(&ctx->io, io_looper_get_main(), ctx->fd, IO_FLAG_RANDOM_ACCESS));

	memset(&ctx->notify, 0, sizeof(ctx->notify));
	future_set_callback(&ctx->notify, test_random_access_file, ctx);

	testzero(io_async_read(&ctx->io, 10, ctx->buf, 1024, &ctx->notify));
	future_wait(&ctx->notify);	
	testzero(ctx->notify.error);
	test(ctx->notify.length == 5);
	test(memcmp(ctx->buf, "54321", 5) == 0);
		
	testzero(io_async_read(&ctx->io, 0, ctx->buf, 1024, &ctx->notify));
	future_wait(&ctx->notify);
	testzero(ctx->notify.error);
	test(ctx->notify.length == 15);
	test(memcmp(ctx->buf, "12aaaaa87654321", 15) == 0);

	future_leave();

	test_ok();	
	goto clean;
error:
	test_fail();
clean:
	if (ctx->fd)closefile(ctx->fd);
	free(ctx);
}



static void test_tcp_socket(void* arg) {
	myctx* ctx = (myctx*)arg;
	if (!ctx) {
		test(ctx = (myctx*)malloc(sizeof(myctx)));
		memset(ctx, 0, sizeof(myctx));
		testzero(getaddrinfo("127.0.0.1", TEST_PORT_TCP, NULL, &ctx->ai));
		ctx->fd = socket(ctx->ai->ai_family, SOCK_STREAM, 0);
		test(ctx->fd > 0);
		testzero(io_async_init(&ctx->io, io_looper_get_main(), ctx->fd, 0));
		ctx->step = 0;
		future_set_callback(&ctx->notify, test_tcp_socket, ctx);
	}

	future_enter(ctx->step);
	testzero(io_async_connect(&ctx->io, ctx->ai->ai_addr, (int)ctx->ai->ai_addrlen, &ctx->notify));
	future_wait(&ctx->notify);
	testzero(ctx->notify.error);


	testzero(io_async_send(&ctx->io, "1234567890", 10, &ctx->notify));
	future_wait(&ctx->notify);
	testzero(ctx->notify.error);
	test(ctx->notify.length == 10);
		
	testzero(io_async_recv(&ctx->io, ctx->buf, 1024, &ctx->notify));
	future_wait(&ctx->notify);
	testzero(ctx->notify.error);
	test(ctx->notify.length == 10);
	test(0 == memcmp(ctx->buf, "1234567890", 10));
		
	testzero(io_async_send(&ctx->io, "987654321", 9, &ctx->notify));
	future_wait(&ctx->notify);
	testzero(ctx->notify.error);
	test(ctx->notify.length == 9);
	
	testzero(io_async_recv(&ctx->io, ctx->buf, 1024, &ctx->notify));
	future_wait(&ctx->notify);
	testzero(ctx->notify.error);
	test(ctx->notify.length == 9);
	test(0 == memcmp(ctx->buf, "987654321", 9));

	future_leave();

	test_ok();
	goto clean;
error:
	test_fail();
clean:
	if (ctx->fd)closesocket(ctx->fd);
	free(ctx);
}


static void test_udp_socket(void* arg) {
	myctx* ctx = (myctx*)arg;
	if (!ctx) {
		test(ctx = (myctx*)malloc(sizeof(myctx)));
		memset(ctx, 0, sizeof(myctx));
		testzero(getaddrinfo("127.0.0.1", TEST_PORT_TCP, NULL, &ctx->ai));
		ctx->fd = socket(ctx->ai->ai_family, SOCK_DGRAM, 0);
		test(ctx->fd > 0);
		testzero(io_async_init(&ctx->io, io_looper_get_main(), ctx->fd, 0));
		ctx->step = 0;
		future_set_callback(&ctx->notify, test_udp_socket, ctx);
	}

	future_enter(ctx->step);

	testzero(io_async_sendto(&ctx->io, "1234567890", 10, ctx->ai->ai_addr, (int)ctx->ai->ai_addrlen, &ctx->notify));
	future_wait(&ctx->notify);
	testzero(ctx->notify.error);
	test(ctx->notify.length == 10);

	ctx->addrlen = 128;
	testzero(io_async_recvfrom(&ctx->io, ctx->buf, 1024, (struct sockaddr*)ctx->addrbuf, &ctx->addrlen, &ctx->notify));
	future_wait(&ctx->notify);
	testzero(ctx->notify.error);
	test(ctx->notify.length == 10);
	test(0 == memcmp(ctx->buf, "1234567890", 10));
	test(ctx->ai->ai_addrlen == ctx->addrlen);
	test(0 == memcmp(ctx->addrbuf, ctx->ai->ai_addr, ctx->addrlen));

	test(0 == io_async_sendto(&ctx->io, "987654321", 9, ctx->ai->ai_addr, (int)ctx->ai->ai_addrlen, &ctx->notify));
	future_wait(&ctx->notify);
	testzero(ctx->notify.error);
	test(ctx->notify.length == 9);
	
	ctx->addrlen = 128;
	test(0 == io_async_recvfrom(&ctx->io, ctx->buf, 1024, (struct sockaddr*)ctx->addrbuf, &ctx->addrlen, &ctx->notify));
	future_wait(&ctx->notify);
	testzero(ctx->notify.error);
	test(ctx->notify.length == 9);
	test(0 == memcmp(ctx->buf, "987654321", 9));
	test(ctx->ai->ai_addrlen == ctx->addrlen);
	test(0 == memcmp(ctx->addrbuf, ctx->ai->ai_addr, ctx->addrlen));

	future_leave();
	test_ok();
	goto clean;
error:
	test_fail();
clean:
	if (ctx->fd)closesocket(ctx->fd);
	free(ctx);
}

 



void test_ioasync_init() {



	test_register(test_stream_file);
	test_register(test_random_access_file);
	test_register(test_tcp_socket);
	test_register(test_udp_socket);
	
}
