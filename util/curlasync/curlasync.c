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
#include "curlasync.h"
#include <string.h>
#include <stdlib.h>
#include <string.h>

typedef struct curl_ctx{	
	poll_ctx pollctx;	
	fd_t sock;
	curl_async_callback cb;
	void* userdata;
}curl_ctx;


static CURLM *curl_multi_;
static timer_ctx timer_;
static int timer_started_;

#define DEBUG_TRACE(...) //printf(__VA_ARGS__)

static int curl_debug_trace(CURL *handle,curl_infotype type,char *data,size_t size,void *userptr){
	DEBUG_TRACE("[%d]%s\n",type,data);
	return 0;
}

static void check_multi_info(void){
	//char *done_url;
	CURLMsg *message;
	int pending;
	CURL *easy_handle;
	curl_ctx* ctx = NULL;
	while((message = curl_multi_info_read(curl_multi_, &pending))) {
		switch(message->msg) {
		case CURLMSG_DONE:			
			easy_handle = message->easy_handle;
			//curl_easy_getinfo(easy_handle, CURLINFO_EFFECTIVE_URL, &done_url);
			curl_easy_getinfo(easy_handle, CURLINFO_PRIVATE, &ctx);
			curl_easy_setopt(easy_handle, CURLOPT_PRIVATE, NULL);
			//curl_easy_getinfo(easy_handle, CURLINFO_PRIVATE, &ctx);
			//printf("%s DONE\n", done_url);			
			if(ctx && ctx->cb)ctx->cb(ctx->userdata);
			free(ctx);
			curl_multi_remove_handle(curl_multi_, easy_handle);
			break;
		default:			
			//fprintf(stderr, "CURLMSG default\n");
			break;
		}
	}
}



static void on_poll_result(void* arg){
	int running_handles = 0; 
	curl_ctx* ctx = (curl_ctx*)arg;
	int pollop = ctx->pollctx.events;
	int curlop = 0;
	DEBUG_TRACE("CURL on_poll_result fd:%d ev:%d \n", (int)ctx->sock, pollop);
#if (POLL_ERROR==CURL_CSELECT_ERR) && (POLL_READABLE==CURL_CSELECT_IN) && (POLL_WRITEABLE==CURL_CSELECT_OUT)
	curlop = pollop;
#else
	if(pollop & POLL_ERROR)curlop |= CURL_CSELECT_ERR;	
	if(pollop & POLL_READABLE)curlop |= CURL_CSELECT_IN;	
	if(pollop & POLL_WRITEABLE)curlop |= CURL_CSELECT_OUT;
#endif
	curl_multi_socket_action(curl_multi_, ctx->sock, curlop, &running_handles);	
	check_multi_info();
}


static timer_ctx timer2_;
static int handle_socket(CURL *easy, curl_socket_t s, int action, void *userp, void *socketp){
	int ret = 0;
	curl_ctx* ctx = NULL;	
	DEBUG_TRACE("CURL POLL fd:%d action:%d #%d\n", (int)s, action, GetCurrentThreadId());
	curl_easy_getinfo(easy, CURLINFO_PRIVATE, &ctx);
	ctx->sock = s;
	switch(action) {
	case CURL_POLL_IN:
		ret = poll_register(poll_looper_get_main(), &ctx->pollctx, s, 0, POLL_READABLE, on_poll_result, ctx);
		break;
	case CURL_POLL_OUT:
		ret = poll_register(poll_looper_get_main(), &ctx->pollctx, s, 0, POLL_WRITEABLE, on_poll_result, ctx);
		break;
	case CURL_POLL_INOUT:		
		ret = poll_register(poll_looper_get_main(), &ctx->pollctx, s, 0, POLL_READABLE|POLL_WRITEABLE, on_poll_result, ctx);
		break;	
	case CURL_POLL_REMOVE:		
		ret = poll_unregister(poll_looper_get_main(), &ctx->pollctx);
		break;
	default:
		break;
	}
	if(ret){
		DEBUG_TRACE("error %d #%d\n",ret,GetCurrentThreadId());
		return -1;
	}
	return 0;
}

static void on_timeout(void* arg){
	int running_handles;
	timer_started_ = 0;
	DEBUG_TRACE("on_timeout\n");
	curl_multi_socket_action(curl_multi_, CURL_SOCKET_TIMEOUT, 0, &running_handles);
	//curl_multi_perform(curl_multi_, &running_handles);
	check_multi_info();
}

static int start_timeout(CURLM *multi, long timeout_ms, void *userp){
	if(timer_started_){
		DEBUG_TRACE("timeout cancel\n");
		timer_cancel(timer_queue_get_main(), &timer_);
	}
	if(timeout_ms>=0){	
		timer_started_ = 1;
		DEBUG_TRACE("start_timeout\n");
		timer_add_timeout(timer_queue_get_main(), &timer_,timeout_ms,on_timeout,userp);
	}else{
		timer_started_ = 0;
	}
	return 0;
}

int curl_async_init(){
	curl_global_init(CURL_GLOBAL_ALL);
	curl_multi_ = curl_multi_init();
	curl_multi_setopt(curl_multi_, CURLMOPT_SOCKETFUNCTION, handle_socket);
	curl_multi_setopt(curl_multi_, CURLMOPT_TIMERFUNCTION, start_timeout);
	return 0;
}



int curl_async_perform(CURL* curl, curl_async_callback cb, void* userdata){
	curl_ctx* ctx = (curl_ctx*)malloc(sizeof(curl_ctx)); //free in check_multi_info()
	memset(ctx,0,sizeof(curl_ctx));
	ctx->cb = cb;
	ctx->userdata = userdata;
	curl_easy_setopt(curl, CURLOPT_PRIVATE, ctx);
	
	//curl_easy_setopt(curl, CURLOPT_DEBUGFUNCTION, curl_debug_trace);	
	//curl_easy_setopt(curl, CURLOPT_VERBOSE, 1L);
	
	return curl_multi_add_handle(curl_multi_, curl);	
}

