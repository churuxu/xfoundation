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

#include "guiasync.h"
#include "pollasync.h"
#include "ioasync.h"
#include "workasync.h"
#include "timerqueue.h"
#include "clocks.h"


#ifdef _WIN32

#include <windows.h>


#define HANDLER_WNDCLASS  TEXT("gui_async_handler")
#define WM_POST_RUN (WM_USER+100)
#define WM_PROCESS_IO (WM_USER+101)
#define WM_PROCESS_POLL (WM_USER+102)
#define WM_PROCESS_TIMER (WM_USER+103)

static HWND wnd_handler_;
static io_looper_t main_io_looper_;
static poll_looper_t main_poll_looper_;
static timer_queue_t main_timer_queue_;

static LRESULT CALLBACK HandlerWindowProc(HWND hWnd, UINT Msg, WPARAM wParam, LPARAM lParam){  
	gui_async_callback cb;	
    switch(Msg){
        case WM_POST_RUN:           
			if (wParam) {
				cb = (gui_async_callback)wParam;
				cb((void*)lParam);
			}
            return 1;
		case WM_PROCESS_IO:
			if (wParam) {
				io_looper_process_events(main_io_looper_, (io_event*)wParam, (int)lParam);
			}			
			return 1;
		case WM_PROCESS_POLL:
			if (wParam) {
				poll_looper_process_events(main_poll_looper_, (poll_event*)wParam, (int)lParam);
			}
			return 1;
		case WM_PROCESS_TIMER:
			if (1) {
				int newdelay = 60000;
				timer_queue_process(main_timer_queue_, &newdelay);
			}
			return 1;
        default:
            break;
    }
    return DefWindowProc(hWnd, Msg, wParam, lParam); 
}


int gui_async_post_callback(gui_async_callback cb, void* userdata){
	BOOL ret = PostMessage(wnd_handler_, WM_POST_RUN, (WPARAM)cb, (LPARAM)userdata);
	return ret?0:-1;
}

static VOID CALLBACK OnTimerProc(HWND hwnd, UINT uMsg, UINT_PTR idEvent, DWORD dwTime) {
	int newdelay = 60000;
	timer_queue_process(main_timer_queue_, &newdelay);
	SetTimer(NULL, idEvent, newdelay, OnTimerProc);
}

static void on_timer_queue_change(void* udata, int newdelay) {
	if (newdelay <= 0) {
		PostMessage(wnd_handler_, WM_PROCESS_TIMER, (WPARAM)0, (LPARAM)0);
	}
	else {
		SetTimer(NULL, (UINT_PTR)udata, newdelay, OnTimerProc);
	}
}


static DWORD CALLBACK IOEventWaitThread(LPVOID arg) {
	io_event evs[32];
	int count;	
	while (1) {
		count = 32;
		if (0 == io_looper_wait_events(main_io_looper_, evs, &count, 100000)) {
			//process io events in main thread
			SendMessage(wnd_handler_, WM_PROCESS_IO, (WPARAM)evs, (LPARAM)count);
		}
	}
	return 1;
}

static DWORD CALLBACK PollEventWaitThread(LPVOID arg) {
	poll_event evs[32];
	int count;
	while (1) {
		count = 32;
		if (0 == poll_looper_wait_events(main_poll_looper_, evs, &count, 100000)) {
			//process poll events in main thread
			SendMessage(wnd_handler_, WM_PROCESS_POLL, (WPARAM)evs, (LPARAM)count);
		}
	}
	return 1;
}


#define ensure(exp)  if(!(exp))return 1;

int gui_async_init() {
	HINSTANCE hInst = NULL;
	WNDCLASS  wc = { 0 };
	UINT_PTR timerid;
	DWORD tid;
	HANDLE th;

	if (wnd_handler_)return 0;

	//create timer
	timerid = SetTimer(NULL, 0, 1, OnTimerProc);
	ensure(0 != timerid);

	//registe window class
	wc.hInstance = hInst;
	wc.lpszClassName = HANDLER_WNDCLASS;
	wc.lpfnWndProc = HandlerWindowProc;
	RegisterClass(&wc);

	//create handler window
	wnd_handler_ = CreateWindowEx(
		WS_EX_APPWINDOW, HANDLER_WNDCLASS,
		TEXT(""), 0,
		0, 0,
		0, 0,
		NULL, NULL,
		hInst, NULL);

	ensure(NULL != wnd_handler_);
			
	main_io_looper_ = io_looper_get_main();
	main_poll_looper_ = poll_looper_get_main();
	main_timer_queue_ = timer_queue_get_main();
	ensure(0 == work_async_init(NULL, gui_async_post_callback));

	timer_queue_set_clock(main_timer_queue_, clock_get_tick);
	timer_queue_set_observer(main_timer_queue_, on_timer_queue_change, (void*)timerid);
	
	//create event wait thread
	ensure(th = CreateThread(NULL, 0, IOEventWaitThread, NULL, 0, &tid));
	ensure(th = CreateThread(NULL, 0, PollEventWaitThread, NULL, 0, &tid));

	return 0;
}

#endif

