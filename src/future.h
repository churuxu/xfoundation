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



/*

stackless coroutine like

struct future_liked_struct{
	future_callback cb;
	void* udata;
	int ready;	
	...
	other fields
};


void func(void* arg){
    xxxcontext* ctx = arg;
	future_enter(ctx->status);	
	...
	call async function support future liked struct and use this func as callback ...

	future_wait(&ctx->fut);
	async operation complete ...

	future_leave();
}

*/

typedef void(*future_callback)(void* arg);

#define future_set_callback(pfu, fcb, ud) {(pfu)->cb = fcb; (pfu)->udata = ud;}


#define future_enter(linevar) {\
int* _future_linevar_ = (linevar); \
switch (*_future_linevar_) {	\
case 0:


#define future_leave()  *_future_linevar_=-1;case -1:;}}



#if defined(_MSC_VER) && defined(WINAPI_FAMILY) && (WINAPI_FAMILY==WINAPI_FAMILY_APP) 

// __LINE__ is not const when build UAP
#define future_yield(action) \
	*_future_linevar_ = (__COUNTER__+2); \
action;\
	return; \
	case (__COUNTER__+1):\

#else

#define future_yield(action)  \
	*_future_linevar_ = __LINE__; \
action;\
	return; \
	case __LINE__:\

#endif

#define future_wait(fut) \
while(!(fut)->ready){\
future_yield((void)0); continue;\
}


