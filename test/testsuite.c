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

#include "testsuite.h"
#include <errno.h>

#if defined(_WIN32)
#include <windows.h>

#define last_error() GetLastError()
#elif  defined(__ANDROID__) 
#include <android/log.h>
#endif

#ifndef _WIN32
#define last_error() errno
#endif

static int ok_count_;
static int fail_count_;
static int total_count_;
static int cur_running_;
static test_func testfns_[8192];
static test_result_callback resultcb_;

static void test_run_next(){
	test_func fn;	
	cur_running_++;
	fn = testfns_[cur_running_];
	if (fn) {
		fn(NULL);
	}	
}

static void test_print_report() {
	test_trace("\n========== TOTAL:%d  OK:%d  FAIL:%d ==========\n",
		total_count_, ok_count_, fail_count_);	
}

static void test_check_finish() {
	if ((total_count_ - ok_count_ - fail_count_) == 0) {
		test_print_report();
		if(resultcb_)resultcb_(total_count_, ok_count_, fail_count_);
	}
}


void test_register(test_func fn) {
	testfns_[total_count_] = fn;
	total_count_++;
}

void test_start(test_result_callback cb){	
	if(cb)resultcb_ = cb;
	cur_running_ = -1;
	test_run_next();
}


void test_print(const char* msg) {
	printf("%s", msg); 
#if defined(_WIN32)
	OutputDebugStringA(msg);
#elif defined(__ANDROID__)
	__android_log_write(ANDROID_LOG_INFO, "test", msg);
#endif
}

void test_ok_(const char* name) {
	ok_count_++;
	test_trace("[ OK ] %s\n", name);
	test_check_finish();
	test_run_next();
}

void test_fail_(const char* name) {
	fail_count_++;
	test_trace("[Fail] %s\n", name);
	test_check_finish();
	test_run_next();
}



