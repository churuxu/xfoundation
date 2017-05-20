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


typedef struct _acepter_ctx {
	fd_t fd;
	io_ctx io;
	io_future notify;
	struct sockaddr_storage addrbuf;
	int addrlen;
	fd_t clientfd;
	int step;
}acepter_ctx;

typedef struct _session_ctx {
	fd_t fd;
	io_ctx io;
	io_future notify;
	char buf[1024];
	int buflen;
	int step;
}session_ctx;


static void tcp_client_session(void* arg) {
	session_ctx* ctx = (session_ctx*)arg;
	//printf("tcp_client_session step:%d\n", ctx->step);
	future_enter(&ctx->step);
	testzero(io_async_init(&ctx->io, io_looper_get_main(), ctx->fd, 0));
	future_set_callback(&ctx->notify, tcp_client_session, ctx);
	while (1) {		
		testzero(io_async_recv(&ctx->io, ctx->buf, 1024, &ctx->notify));
		future_wait(&ctx->notify);
		testzero(ctx->notify.error);
		//printf("tcp server recv %d bytes\n", ctx->buflen);
		if (ctx->notify.length == 0) {
			closesocket(ctx->fd);
			free(ctx);
			return;
		}
		testzero(io_async_send(&ctx->io, ctx->buf, ctx->notify.length, &ctx->notify));
		future_wait(&ctx->notify);
		//printf("tcp server send %d bytes\n", ctx->buflen);
		testzero(ctx->notify.error);
	}
	future_leave();
	return;
error:
    //printf("tcp server session close fd:%d\n", ctx->fd);
	closesocket(ctx->fd);
	free(ctx);
}


static void tcp_accepter(void* arg) {
	acepter_ctx* ctx = (acepter_ctx*)arg;
	session_ctx* client;
	future_enter(&ctx->step);
	testzero(io_async_init(&ctx->io, io_looper_get_main(), ctx->fd, 0));
	future_set_callback(&ctx->notify, tcp_accepter, ctx);
	while (1) {
		ctx->addrlen = sizeof(struct sockaddr_storage);
		testzero(io_async_accept(&ctx->io, &ctx->clientfd, (struct sockaddr*)&ctx->addrbuf, &ctx->addrlen, &ctx->notify));
		future_wait(&ctx->notify);
		testzero(ctx->notify.error);
        //printf("tcp server accept fd:%d\n", ctx->clientfd);
		client = (session_ctx*)malloc(sizeof(session_ctx));
		memset(client, 0, sizeof(session_ctx));
		client->step = 0;
		client->fd = ctx->clientfd;
		tcp_client_session(client);
	}
	future_leave();
	return;
error:
    //printf("error(%d)", ctx->notify.error);	
	closesocket(ctx->fd);
	free(ctx);
	//abort();
}


void echo_server_tcp_start(const char* bindip, const char* bindport) {
	struct addrinfo* ai = NULL;
	acepter_ctx* ctx ;
	int reuse = 1;
	if (!bindip)bindip = "0.0.0.0";
	testzero(getaddrinfo(bindip, bindport, NULL, &ai));
	ctx = (acepter_ctx*)malloc(sizeof(acepter_ctx));
	memset(ctx, 0, sizeof(acepter_ctx));
	ctx->fd = socket(ai->ai_family, SOCK_STREAM, 0);
	test(0 < ctx->fd);
	setsockopt(ctx->fd, SOL_SOCKET, SO_REUSEADDR, (char *)&reuse, sizeof(reuse));
	testzero(bind(ctx->fd, ai->ai_addr, (int)ai->ai_addrlen));
	testzero(listen(ctx->fd, 256));
	ctx->step = 0;
	tcp_accepter(ctx);
	return;
error:
	//abort();
	return;
}



