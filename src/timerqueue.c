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

#include "timerqueue.h"
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <time.h>

#ifdef _WIN32
#ifndef ERROR_OUTOFMEMORY
#define ERROR_OUTOFMEMORY 14
#endif
#else
#define ERROR_OUTOFMEMORY ENOMEM
#endif

typedef struct _timer_ctx_internal timer_ctx_internal;

struct _timer_ctx_internal{
	timer_callback cb; //callback
	void* userdata; //user data
	int ready;
	int duration;
	uint64_t expire;
	size_t signature;
	timer_ctx_internal* prev;
	timer_ctx_internal* next;	
	
	char loop;	
};

struct _timer_queue{
	timer_clock_function clock_get_tick_;
	timer_ctx_internal* first_timer_;
	timer_ctx_internal* cur_timer_;
	timer_queue_observer observer_cb_;
	void* observer_udata_;
};

static uint64_t timer_default_clock() {
	return (uint64_t)(clock() / (CLOCKS_PER_SEC / 1000));
}


timer_queue_t timer_queue_get_main() {
	static timer_queue_t mainqueue;
	if (!mainqueue) {
		timer_queue_create(&mainqueue);
	}
	return mainqueue;
}

int timer_queue_create(timer_queue_t* out){
	timer_queue_t tmq;
	*out = NULL;
	tmq = (timer_queue_t)malloc(sizeof(struct _timer_queue));	
	if(!tmq)return ERROR_OUTOFMEMORY;
	memset(tmq, 0, sizeof(struct _timer_queue));
	tmq->clock_get_tick_ = timer_default_clock;
	*out = tmq;
	return 0;
}

int timer_queue_destroy(timer_queue_t tmq){
	free(tmq);
	return 0;
}

int timer_queue_set_clock(timer_queue_t tmq, timer_clock_function clockfn) {
	tmq->clock_get_tick_ = clockfn;
	return 0;
}

int timer_queue_set_observer(timer_queue_t tmq, timer_queue_observer cb, void* udata) {
	tmq->observer_cb_ = cb;
	tmq->observer_udata_ = udata;
	return 0;
}



//TODO: use rb-tree or some other algorithm to manage timers

static void timer_node_add(timer_queue_t tmq, timer_ctx_internal* node) {
	timer_ctx_internal* cur = tmq->first_timer_;
	if (!cur) {
		tmq->first_timer_ = node;
		tmq->first_timer_->prev = NULL;
		node->next = NULL;
		return;
	}
	while (cur) {
		if (cur->expire >= node->expire) {
			node->next = cur;
			node->prev = cur->prev;
			if (cur->prev)cur->prev->next = node;
			cur->prev = node;
			if (cur == tmq->first_timer_) {
				tmq->first_timer_ = node;
			}
			return;
	}
		if (cur->next) {
			cur = cur->next;
		}
		else {
			cur->next = node;
			node->prev = cur;
			node->next = NULL;
			return;
		}
}
}

static void timer_node_remove(timer_queue_t tmq, timer_ctx_internal* node) {
	if (node->prev) {
		node->prev->next = node->next;
	}
	else {
		if(node == tmq->first_timer_)
			tmq->first_timer_ = node->next;
	}
	if (node->next)node->next->prev = node->prev;
	if (tmq->first_timer_)tmq->first_timer_->prev = NULL;
	if (tmq->cur_timer_) {
		if (tmq->cur_timer_ == node) {
			tmq->cur_timer_ = node->next;
		}
	}
	node->next = NULL;
	node->prev = NULL;
}



static int timer_add(timer_queue_t tmq, timer_ctx* out, unsigned int interval, int loop, timer_callback cb, void* userdata){	
	uint64_t ticknow;	
	timer_ctx_internal* result;

	if(!out ||!cb)return -1;

	result = (timer_ctx_internal*)out;

	ticknow = tmq->clock_get_tick_();
	result->ready = 0;
	result->cb = cb;
	result->duration = interval;
	result->expire = ticknow + interval;
	result->loop = loop;
	
	result->userdata = userdata;
	result->next = NULL;
	result->prev = NULL;
	

	timer_node_add(tmq, result);

	if (tmq->observer_cb_ && tmq->first_timer_ == result) {
		tmq->observer_cb_(tmq->observer_udata_, interval);
	}
	return 0;
}

int timer_add_timeout(timer_queue_t tmq, timer_ctx* out, unsigned int timeout, timer_callback cb, void* udata) {
	return timer_add(tmq, out, timeout, 0, cb, udata);
}

int timer_add_interval(timer_queue_t tmq, timer_ctx* out, unsigned int interval, timer_callback cb, void* udata) {
	return timer_add(tmq, out, interval, 1, cb, udata);
}


int timer_cancel(timer_queue_t tmq, timer_ctx* ctx){	
	timer_node_remove(tmq, (timer_ctx_internal*)ctx);
	return 0;
}


int timer_queue_process(timer_queue_t tmq, int* nextsleeptime){
	
	uint64_t ticknow;
	timer_ctx_internal* cur;

	ticknow = tmq->clock_get_tick_();

	tmq->cur_timer_ = tmq->first_timer_;
	while(tmq->cur_timer_){
		cur = tmq->cur_timer_;
		if(cur->expire <= ticknow){	
			cur->ready = 1;
			if (!cur->loop) {
				timer_node_remove(tmq,cur);
				cur->cb(cur->userdata);
				continue;
			}
			cur->cb(cur->userdata); 
			if (cur != tmq->cur_timer_)continue; // current timer is canceled 
			if(cur->loop){
				timer_node_remove(tmq,cur);
				cur->expire += cur->duration;
				if(cur->expire < ticknow){
					cur->expire = ticknow + 1;
				}
				cur->ready = 0;
				timer_node_add(tmq,cur);				
			}
		}else{
			break;
		}
		tmq->cur_timer_ = cur->next;
	}
	
	if(nextsleeptime){
		if(tmq->first_timer_){
			if(tmq->first_timer_->expire < ticknow){
				*nextsleeptime = 1;
			}else{
				*nextsleeptime = (int)(tmq->first_timer_->expire - ticknow);
			}
		}else{
			*nextsleeptime = 0x7fffffff;
		}
	}
	return 0;	
}





