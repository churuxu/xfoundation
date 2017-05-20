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
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>


typedef struct _udpctx{
	fd_t fd;
	io_ctx io;
	io_future notify;	
	char buf[1024];
	int buflen;
	struct sockaddr_storage addrbuf;
	int addrlen;	
	int step;
}udpctx;



static void udp_session(void* arg) {
	udpctx* ctx = (udpctx*)arg;

	future_enter(&ctx->step);
	testzero(io_async_init(&ctx->io, io_looper_get_main(), ctx->fd, 0));
	future_set_callback(&ctx->notify, udp_session, ctx);
	while (1) {
		ctx->addrlen = sizeof(struct sockaddr_storage);
		testzero(io_async_recvfrom(&ctx->io, ctx->buf, 1024, (struct sockaddr*)&ctx->addrbuf, &ctx->addrlen, &ctx->notify));
		future_wait(&ctx->notify);
		testzero(ctx->notify.error);

		testzero(io_async_sendto(&ctx->io, ctx->buf, ctx->notify.length, (struct sockaddr*)&ctx->addrbuf, ctx->addrlen, &ctx->notify));
		future_wait(&ctx->notify);
		testzero(ctx->notify.error);
	}
	future_leave();
	return;
error:	
	closesocket(ctx->fd);
	free(ctx);	
}


void echo_server_udp_start(const char* bindip, const char* bindport) {
	struct addrinfo* ai = NULL;
	udpctx* ctx;
	int reuse = 1;
	if (!bindip)bindip = "0.0.0.0";
	testzero(getaddrinfo(bindip, bindport, NULL, &ai));
	ctx = (udpctx*)malloc(sizeof(udpctx));
	memset(ctx, 0, sizeof(udpctx));
	ctx->fd = socket(ai->ai_family, SOCK_DGRAM, 0);
	test(0 < ctx->fd);
	setsockopt(ctx->fd, SOL_SOCKET, SO_REUSEADDR, (char *)&reuse, sizeof(reuse));
	testzero(bind(ctx->fd, ai->ai_addr, (int)ai->ai_addrlen));
	ctx->step = 0;
	udp_session(ctx);
	return;
error:
	//abort();
	return;

}

