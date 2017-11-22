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
#include "curlasync.h"
#include <stdio.h>
#include <stdlib.h>

#ifdef _MSC_VER
#pragma comment(lib,"ws2_32.lib")
#pragma comment(lib,"libcurl.lib")
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



typedef struct http_ctx{
	CURL* curl;
	int status;
	size_t len;
	char buf[1025];	
	char errmsg[CURL_ERROR_SIZE];
}http_ctx;

static size_t http_write_callback(char *ptr, size_t size, size_t nmemb, void *userdata){
	size_t len = size * nmemb;
	http_ctx* ctx = (http_ctx*)userdata;
	ctx->len += len;
	return len;

	if(!ctx->len){
		ctx->len = len;
		memcpy(ctx->buf, ptr, min(len,1024));
		ctx->buf[min(len,1024)]=0;
	}	
	return len;
}

int async_request(http_ctx* http, const char* url, io_callback cb, void* userdata){

	CURL* handle = curl_easy_init();
	http->curl = handle;
	http->len = 0;
	http->buf[0] = 0;
	http->errmsg[0] = 0;
	curl_easy_setopt(handle, CURLOPT_URL, url);
	curl_easy_setopt(handle, CURLOPT_SSL_VERIFYPEER, 0);
	curl_easy_setopt(handle, CURLOPT_WRITEFUNCTION, http_write_callback);
	curl_easy_setopt(handle, CURLOPT_WRITEDATA, http);
	curl_easy_setopt(handle, CURLOPT_ERRORBUFFER, http->errmsg);

	return curl_async_perform(handle, cb, userdata);
}

void onresult(void* arg){
	long httpstat = 0;
	char* done_url = NULL;
	http_ctx* ctx = (http_ctx*)arg;
	curl_easy_getinfo(ctx->curl, CURLINFO_EFFECTIVE_URL, &done_url);
	curl_easy_getinfo(ctx->curl, CURLINFO_RESPONSE_CODE, &httpstat); 
	if (httpstat) {
		printf("%s result (%d) (%d)bytes #%d \n", done_url, httpstat, (int)ctx->len, GetCurrentThreadId());
	}
	else {
		printf("%s result (%s) #%d \n", done_url, ctx->errmsg, GetCurrentThreadId());
	}
	
}



int main(){
	int ret;
	http_ctx* ctx;
	poll_event evs[20];
	int count;
	int nextwait = 0;
	poll_looper_t polllooper;
	timer_queue_t timerqueue;
	
	curl_async_init();

	polllooper = poll_looper_get_main();
	timerqueue = timer_queue_get_main();
		
	
	

	ctx = (http_ctx*)malloc(sizeof(http_ctx));
	//zerocall(async_request(ctx, "https://github.com/", onresult, ctx));

	ctx = (http_ctx*)malloc(sizeof(http_ctx));
	//async_request(ctx, "http://192.149.11.111", onresult, ctx);

	ctx = (http_ctx*)malloc(sizeof(http_ctx));
	zerocall(async_request(ctx, "https://www.baidu.com/", onresult, ctx));

	ctx = (http_ctx*)malloc(sizeof(http_ctx));
	zerocall(async_request(ctx, "http://www.baidu.com/", onresult, ctx));

	while (1) {
		timer_queue_process(timerqueue, &nextwait);
		count = 20;
		if (0 == poll_looper_wait_events(polllooper, evs, &count, nextwait)){
			poll_looper_process_events(polllooper, evs, count);
		}
	}

	return 1;
}

