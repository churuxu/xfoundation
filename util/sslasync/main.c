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

	msg = "GET / HTTP/1.1\r\nHost: www.baidu.com\r\nConnection: close\r\n\r\n";
	len = strlen(msg);

	testzero(ssl_async_send(ctx->ssl, msg, len, &ctx->notify));
	future_wait(&ctx->notify);
	testzero(ctx->notify.error);
	
	while (1) {
		testzero(ssl_async_recv(ctx->ssl, ctx->buf, 1024, &ctx->notify));
		future_wait(&ctx->notify);
		testzero(ctx->notify.error);
		
		if (ctx->notify.length == 0) {
			printf("https socket closed");
			break;
		}
		else {
			fwrite(ctx->buf, 1,ctx->notify.length,stdout);
		}
	}
	future_leave();
		
	goto clean;
error:
	printf("error %s\n", strsyserror(GetLastError()));
clean:
	
	if (ctx->fd)closesocket(ctx->fd);
	if(ctx->ssl)ssl_ctx_free(ctx->ssl);
	free(ctx);
}




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
	ssl_ctx* ssl;
	fd_t fd;
	io_ctx io;
	io_future notify;
	char buf[1024];
	int buflen;
	int step;
}session_ctx;

static ssl_config sslconf_;

static void tcp_client_session(void* arg) {
	session_ctx* ctx = (session_ctx*)arg;
	const char* msg = "HTTP/1.1 200 OK\r\nContent-Type: text/html;\r\nContent-Length: 11\r\n\r\nhello world\r\n";
	int msglen = strlen(msg);

	//printf("tcp_client_session step:%d\n", ctx->step);
	future_enter(&ctx->step);
	testzero(io_async_init(&ctx->io, io_looper_get_main(), ctx->fd, 0));
	testzero(ssl_ctx_init(&ctx->ssl, &ctx->io, &sslconf_));
	future_set_callback(&ctx->notify, tcp_client_session, ctx);
	testzero(ssl_async_handshake(ctx->ssl, &ctx->notify));
	future_wait(&ctx->notify);
	testzero(ctx->notify.error);
	printf("accept handshake ok \n");
	while (1) {
		testzero(ssl_async_recv(ctx->ssl, ctx->buf, 1024, &ctx->notify));
		future_wait(&ctx->notify);
		testzero(ctx->notify.error);
		printf("tcp server recv %d bytes\n", ctx->notify.length);
		if (ctx->notify.length == 0) {
			goto clean;
		}
		
		testzero(ssl_async_send(ctx->ssl, msg, msglen, &ctx->notify));
		future_wait(&ctx->notify);
		//printf("tcp server send %d bytes\n", ctx->buflen);
		testzero(ctx->notify.error);
	}
	future_leave();
	return;
error:
	//printf("tcp server session close fd:%d\n", ctx->fd);
	printf("session error %s\n", strsyserror(GetLastError()));
clean:
	closesocket(ctx->fd);
	if (ctx->ssl)ssl_ctx_free(ctx->ssl);
	free(ctx);
}


static void tcp_accepter(void* arg) {
	acepter_ctx* ctx = (acepter_ctx*)arg;
	session_ctx* client;
	
	sslconf_.flags = SSL_FLAG_SERVER_MODE/*|SSL_FLAG_VERIFY_PEER*/;
	sslconf_.ca = "D:\\certs\\ca_cert.crt";
	sslconf_.cert = "D:\\certs\\server_cert.crt";
	sslconf_.key = "D:\\certs\\server_key.pem";
	
	future_enter(&ctx->step);
	testzero(io_async_init(&ctx->io, io_looper_get_main(), ctx->fd, 0));
	future_set_callback(&ctx->notify, tcp_accepter, ctx);
	while (1) {
		ctx->addrlen = sizeof(struct sockaddr_storage);
		testzero(io_async_accept(&ctx->io, &ctx->clientfd, (struct sockaddr*)&ctx->addrbuf, &ctx->addrlen, &ctx->notify));
		future_wait(&ctx->notify);
		testzero(ctx->notify.error);
		printf("tcp server accept fd:%d\n", ctx->clientfd);
		client = (session_ctx*)malloc(sizeof(session_ctx));
		memset(client, 0, sizeof(session_ctx));
		
		client->step = 0;
		client->fd = ctx->clientfd;
		tcp_client_session(client);
	}
	future_leave();
	return;
error:
	printf("server error %s\n", strsyserror(GetLastError()));
	closesocket(ctx->fd);	
	free(ctx);
	//abort();
}


void ssl_http_server_start(const char* bindip, const char* bindport) {
	struct addrinfo* ai = NULL;
	acepter_ctx* ctx;
	int reuse = 1;
	if (!bindip)bindip = "0.0.0.0";
	testzero(getaddrinfo(bindip, bindport, NULL, &ai));
	ctx = (acepter_ctx*)malloc(sizeof(acepter_ctx));
	memset(ctx, 0, sizeof(acepter_ctx));
	ctx->fd = socket(ai->ai_family, SOCK_STREAM, 0);
	//test(0 < ctx->fd);
	setsockopt(ctx->fd, SOL_SOCKET, SO_REUSEADDR, (char *)&reuse, sizeof(reuse));
	testzero(bind(ctx->fd, ai->ai_addr, (int)ai->ai_addrlen));
	testzero(listen(ctx->fd, 256));
	ctx->step = 0;
	tcp_accepter(ctx);
	return;
error:
	//abort();
	printf("server error %s\n", strsyserror(GetLastError()));
	return;
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
		
	ssl_http_server_start(NULL,"443");
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

