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


#include "ioasync.h"

typedef struct _ssl_config {
	int server;
}ssl_config;

typedef struct _ssl_ctx ssl_ctx;

int ssl_ctx_init(ssl_ctx** out, io_ctx* io, ssl_config* config);

int ssl_ctx_free(ssl_ctx* ctx);

/** async connect tcp socket */
int ssl_async_handshake(ssl_ctx* ctx, io_future* fut);

/** async read tcp socket */
int ssl_async_recv(ssl_ctx* ctx, char* buf, size_t buflen, io_future* fut);

/** async write tcp socket */
int ssl_async_send(ssl_ctx* ctx, const char* buf, size_t buflen, io_future* fut);
