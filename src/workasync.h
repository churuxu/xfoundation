/* Copyright (C) 2016-2017 churuxu
* https://github.com/churuxu/cfoundations
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


#ifdef __cplusplus
extern "C" {
#endif
	
	typedef void(*work_callback)(void* udata);
	typedef int(*work_callback_runner)(work_callback cb, void* udata);

	typedef struct _work_ctx {
		work_callback cb; //callback when work done
		void* udata;  //argument for cb and run
		int ready;
		int error;
		work_callback run; //do work (do not free work_ctx memory in run function)
	}work_ctx;

	/** init async work service, 
	   backrunner for run in backgournd thread, if backrunner is null, work_ctx will run use new thread
	   mainrunner for run in main thread. if mainrunner is null, callback will run use work thread
	 */
	int work_async_init(work_callback_runner backrunner, work_callback_runner mainrunner);

	/** post work, ( work_ctx memory must be valid befor work_ctx.cb run) */
	int work_async_run(work_ctx* ctx);


#ifdef __cplusplus
}
#endif