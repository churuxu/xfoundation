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
#include "timerqueue.h"
#include "sslasync.h"
#include "future.h"
#include <string.h>
#include <stdlib.h>
#include <string.h>

#include "openssl/ssl.h"

#define STATUS_HANDSHAKE_READ     1
#define STATUS_HANDSHAKE_ONREAD   2
#define STATUS_HANDSHAKE_WRITE    3
#define STATUS_HANDSHAKE_ONWRITE  4
#define STATUS_HANDSHAKE_FINISHED 5

#define SEND_BUFFER_SIZE 2048
#define RECV_BUFFER_SIZE 2048

struct _ssl_ctx {
	io_ctx* io_;
	io_future onrecv_;
	io_future onsend_;
	io_future* fut_;
	SSL_CTX* ssl_ctx_;
	SSL* ssl_;
	BIO* bio_in_;
	BIO* bio_out_;	
	char* outrecvbuf_;	
	size_t outrecvbuflen_;
	size_t outsendbuflen_;
	int sended_;
	int recved_;
	int handshaked_;
	char sendbuf_[SEND_BUFFER_SIZE];
	char recvbuf_[RECV_BUFFER_SIZE];
};



#define DEBUG_TRACE(...) //printf(__VA_ARGS__)

static void init_openssl_lib() {
	static int inited = 0;
	if (!inited) {
		SSL_library_init();
		SSLeay_add_ssl_algorithms();
		SSL_load_error_strings();
		ERR_load_SSL_strings();
	}
}

int ssl_ctx_init(ssl_ctx** out, io_ctx* io, ssl_config* config) {
	ssl_ctx* ctx = NULL;
	init_openssl_lib();
	ctx = (ssl_ctx*)malloc(sizeof(ssl_ctx));
	memset(ctx, 0, sizeof(ssl_ctx) - SEND_BUFFER_SIZE - RECV_BUFFER_SIZE);
	ctx->io_ = io;
	ctx->bio_in_ = BIO_new(BIO_s_mem());
	ctx->bio_out_ = BIO_new(BIO_s_mem());
	ctx->ssl_ctx_ = SSL_CTX_new(TLS_client_method());
	ctx->ssl_ = SSL_new(ctx->ssl_ctx_);
	
	SSL_set_bio(ctx->ssl_, ctx->bio_in_, ctx->bio_out_);
	SSL_set_connect_state(ctx->ssl_);
	*out = ctx;
	return 0;
}

int ssl_ctx_free(ssl_ctx* ctx) {
	return 0;
}

#define future_set_result(pfut, len) {pfut->ready = 1; pfut->length = len;pfut->cb(pfut->udata);}

static void ssl_handshake_work(void* arg) {
	int ret, len, err;
	ssl_ctx* ctx = (ssl_ctx*)arg;
	//if (ctx->result_.error) {
		//some error , cause finished 
	//	goto error;
	//}
	if (ctx->recved_) {
		ctx->recved_ = 0;
		len = BIO_write(ctx->bio_in_, ctx->recvbuf_, ctx->onrecv_.length);
	}
	ret = SSL_do_handshake(ctx->ssl_);
	err = SSL_get_error(ctx->ssl_, ret);
	if (err == SSL_ERROR_WANT_READ) {
		ctx->recved_ = 1;
		ctx->onrecv_.cb = ssl_handshake_work;
		ctx->onrecv_.udata = ctx;
		ret = io_async_recv(ctx->io_, ctx->recvbuf_, RECV_BUFFER_SIZE, &ctx->onrecv_);
		if (ret)goto error;
	}
	//else if (err == SSL_ERROR_WANT_WRITE) {
		len = BIO_read(ctx->bio_out_, ctx->sendbuf_, SEND_BUFFER_SIZE);
		ctx->onsend_.cb = ssl_handshake_work;
		ctx->onsend_.udata = ctx;
		ret = io_async_send(ctx->io_, ctx->sendbuf_, len, &ctx->onsend_);
		if (ret)goto error;
	//}
	else if (err == 0) {
		//finish
		if (ctx->fut_ && ctx->fut_->cb) {
			future_set_result(ctx->fut_, 0);
		}
	}
	return;
error:
	{
		future_set_result(ctx->fut_, 0);
	}	
}

int ssl_async_handshake(ssl_ctx* ctx, io_future* fut) {
	fut->ready = 0;
	fut->error = 0;
	ctx->fut_ = fut;
	ssl_handshake_work(ctx);
	return 0;
}


static void ssl_async_onrecv(void* arg) {
	int len;
	ssl_ctx* ctx = (ssl_ctx*)arg;
	len = BIO_write(ctx->bio_in_, ctx->recvbuf_, ctx->onrecv_.length);
	len = SSL_read(ctx->ssl_, ctx->outrecvbuf_, ctx->outrecvbuflen_);
	if (len > 0) {		
		future_set_result(ctx->fut_, len);
	}
}

int ssl_async_recv(ssl_ctx* ctx, char* buf, size_t buflen, io_future* fut) {	
	int len;
	fut->ready = 0;
	fut->error = 0;
	len = SSL_read(ctx->ssl_, buf, buflen);
	if (len > 0) {		
		future_set_result(ctx->fut_, len);
	}
	else {
		ctx->onrecv_.cb = ssl_async_onrecv;
		ctx->onrecv_.udata = ctx;
		return io_async_recv(ctx->io_, ctx->recvbuf_, RECV_BUFFER_SIZE, &ctx->onrecv_);
	}
	return 0;
}


static void ssl_async_onsend(void* arg) {
	int len;
	ssl_ctx* ctx = (ssl_ctx*)arg;
	len = ctx->outsendbuflen_;;
	if (len > 0) {
		future_set_result(ctx->fut_, len);
	}
}

int ssl_async_send(ssl_ctx* ctx, const char* buf, size_t buflen, io_future* fut) {
	int len;
	fut->ready = 0;
	fut->error = 0;
	len = SSL_write(ctx->ssl_, buf, buflen);
	len = BIO_read(ctx->bio_out_, ctx->sendbuf_, SEND_BUFFER_SIZE);
	ctx->outsendbuflen_ = buflen;
	ctx->onsend_.cb = ssl_async_onsend;
	ctx->onsend_.udata = ctx;
	return io_async_send(ctx->io_, ctx->sendbuf_, len, &ctx->onsend_);
}
