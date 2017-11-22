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

#include "../src/xfoundation.h"
#include "sslasync.h"
#include <stdio.h>
#include <stdlib.h>

#ifdef _MSC_VER
#pragma comment(lib,"ws2_32.lib")
#pragma comment(lib,"libssl.lib")
#pragma comment(lib,"libcrypto.lib")
#if defined(_MSC_VER) && (_MSC_VER < 1900)
#define snprintf(buf,buflen,...) _snprintf_s(buf,buflen,buflen,__VA_ARGS__)
#endif
#endif
#ifdef _WIN32
const char* strwin32error(int code){
	static char buf[512];	
    int len = FormatMessageA(FORMAT_MESSAGE_FROM_SYSTEM|FORMAT_MESSAGE_IGNORE_INSERTS,NULL,code,1033,buf,512,NULL);
	if(len <= 0){
		snprintf(buf, 512, "unknown error %d\n", code);
	}
	return buf;
}
#define strsyserror strwin32error
#else
#define strsyserror strerror
#endif

#define zerocall(exp) if((ret = exp) !=0){ printf("%s\n", strsyserror(ret)); return 1; };


typedef struct _connection {
	ssl_ctx* ssl;
	fd_t fd;
	io_ctx io;
	io_future notify;
	struct addrinfo* ai;
	int step;
	char buf[1024];
}connection;

#define testzero(exp) if(exp)goto error;
#define test(exp) exp

static void test_ssl_socket(void* arg) {
	char* msg;
	int len;
	connection* ctx = (connection*)arg;
	if (!ctx) {
		test(ctx = (connection*)malloc(sizeof(connection)));
		memset(ctx, 0, sizeof(connection));
		len = getaddrinfo("www.baidu.com", "443", NULL, &ctx->ai);
		ctx->fd = socket(ctx->ai->ai_family, SOCK_STREAM, 0);
		test(ctx->fd > 0);
		testzero(io_async_init(&ctx->io, io_looper_get_main(), ctx->fd, 0));
		ctx->step = 0;

		testzero(ssl_ctx_init(&ctx->ssl, &ctx->io, NULL));

		future_set_callback(&ctx->notify, test_ssl_socket, ctx);
	}

	future_enter(&ctx->step);
	testzero(io_async_connect(&ctx->io, ctx->ai->ai_addr, (int)ctx->ai->ai_addrlen, &ctx->notify));
	future_wait(&ctx->notify);
	testzero(ctx->notify.error);

	testzero(ssl_async_handshake(ctx->ssl, &ctx->notify));
	future_wait(&ctx->notify);
	testzero(ctx->notify.error);

	msg = "GET / HTTP/1.1\r\nHost: www.baidu.com\r\n\r\n";
	len = strlen(msg);

	testzero(ssl_async_send(ctx->ssl, msg, len, &ctx->notify));
	future_wait(&ctx->notify);
	testzero(ctx->notify.error);
	
	testzero(ssl_async_recv(ctx->ssl, ctx->buf, 1024, &ctx->notify));
	future_wait(&ctx->notify);
	testzero(ctx->notify.error);
	msg = ctx->buf;
	//msg = ctx->notify

	future_leave();
		
	goto clean;
error:

clean:
	printf("%s\n", strsyserror(GetLastError()));
	if (ctx->fd)closesocket(ctx->fd);
	free(ctx);
}


int main(){
	
	io_event evs[20];
	int count;
	int nextwait = 0;
	io_looper_t iolooper;
	timer_queue_t timerqueue;
#ifdef _WIN32
	WSADATA wsa;
	WSAStartup(MAKEWORD(2, 2), &wsa);
#endif	


	iolooper = io_looper_get_main();
	timerqueue = timer_queue_get_main();
		
	test_ssl_socket(NULL);
	
	while (1) {
		timer_queue_process(timerqueue, &nextwait);
		count = 20;
		if (0 == io_looper_wait_events(iolooper, evs, &count, nextwait)){
			io_looper_process_events(iolooper, evs, count);
		}
	}

	return 1;
}

