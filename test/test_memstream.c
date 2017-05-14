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
#include <string.h>



static void set_test_mem_pointer(memory_stream_t mems, int r) {
	char buf[32];
	if (r) {
		memory_stream_write(mems, "xxxxxxxxxx", r);
		memory_stream_read(mems, buf, r);
	}
}


static int test_memstream_func(memory_stream_t mems){
	char buf[32];
	test(0==memory_stream_read(mems, buf, 6));	
	test(0==memory_stream_write(mems, "12345678901",11));
	test(10==memory_stream_write(mems, "1234567890",10));	
	test(6==memory_stream_read(mems, buf, 6));	
	test(memcmp(buf, "123456", 6)==0);	
	test(4==memory_stream_read(mems, buf, 6));	
	test(memcmp(buf, "7890", 4)==0);

	return 0;
error:
	return -1;

}

static void test_memstream(void* arg){	
	
	int i;
	memory_stream_t mems = NULL;
	for (i = 0; i < 11; i++) {
		test(0 == memory_stream_open(&mems, 10));
		set_test_mem_pointer(mems, i);
		if(test_memstream_func(mems))goto error;
		memory_stream_close(mems);
	}
	
	test_ok();
	return;
error:
	if(mems)memory_stream_close(mems);
	test_fail();
}



void test_memstream_init() {
	test_register(test_memstream);	
}

