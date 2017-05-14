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

#include "../src/xfoundation.h"
#if defined(_MSC_VER) && (_MSC_VER < 1900)
#define snprintf(buf,buflen,...) _snprintf_s(buf,buflen,buflen,__VA_ARGS__)
#endif
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

typedef void(*test_func)(void*);

typedef void(*test_result_callback)(int total, int ok, int fail);

void test_register(test_func fn);

void test_start(test_result_callback cb);

void test_print(const char* msg);

void test_ok_(const char* name);

void test_fail_(const char* name);


#define test_trace(...) \
	if(1){\
		char buf[1024];\
		snprintf(buf,1024,__VA_ARGS__);\
		test_print(buf);\
	}

#define test_ok() test_ok_(__FUNCTION__)

#define test_fail() test_fail_(__FUNCTION__)

#define test_trace_error(exp)  test_trace("[ ERROR ] %s %s  at\n%s(%d)\n",__FUNCTION__, exp, __FILE__,__LINE__ )

#define test_trace_error_code(exp, code)  test_trace("[ ERROR ] %s (%d) %s  at\n%s(%d)\n",__FUNCTION__,code, exp, __FILE__,__LINE__ )





#define test(exp) \
	if(!(exp)){\
		test_trace_error(#exp);\
		goto error;\
	}\

#define testzero(exp) \
{int ret = exp;\
	if((ret)){\
		test_trace_error_code(#exp, ret);\
		goto error;\
	}\
}


#define ensure(exp) \
	if(!(exp)){\
		test_trace_error(#exp);\
		abort();\
	}\

#define ensurezero(exp) \
{int ret = exp;\
	if((ret)){\
		test_trace_error_code(#exp, ret);\
		abort();\
	}\
}


