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

#ifdef _WIN32

#include "guiasync.h"
#include "testconfig.h"
#include <stdio.h>
#include <stdlib.h>
#include <tchar.h>

#ifdef _MSC_VER
#pragma comment(lib,"ws2_32.lib")
#endif


const char* get_local_data_path(){
	return ".";
}

void on_test_result(int total, int ok, int fail) {
	CHAR buf[256];
	int ret;

	if (total > 0 && (total == ok) && fail == 0) {
		ret = 0;
	}
	else {
		ret = 1;
	}
	buf[0] = 0;
	snprintf(buf,256,"total:%d  ok:%d  fail:%d", total, ok, fail);
	MessageBoxA(NULL, buf, "Test Result", MB_OK);
	PostQuitMessage(ret);
}

int WINAPI _tWinMain(HINSTANCE hInst, HINSTANCE hPrev, LPTSTR lpCmdLine, int nCmdShow) {
	MSG msg;

	if (gui_async_init()){		
		return 1;
	}
	
	run_all_tests(on_test_result);

	MessageBoxA(NULL, "Test Running...", "Test", MB_OK);

	while (GetMessage(&msg, NULL, 0, 0)) {
		TranslateMessage(&msg);
		DispatchMessage(&msg);

	}
	return (int)msg.wParam;
}

#endif

