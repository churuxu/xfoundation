#include "guiasync.h"
#include "pollasync.h"
#include "clocks.h"
#include "timerqueue.h"
#include "ioutil.h"
#include "workasync.h"

#if defined(__APPLE__) 

#include <dispatch/dispatch.h>
#import <Foundation/Foundation.h>

/*
 
 */

static dispatch_source_t iosource_;
static dispatch_source_t timersource_;
static poll_looper_t poll_looper_;
static timer_queue_t timer_queue_;


//restart timer use newdelay (in milliseconds)
static void on_timer_queue_change(void* udata, int newdelay) {
    uint64_t interval = (uint64_t)newdelay * 1000000ULL;
    dispatch_source_set_timer(timersource_, dispatch_walltime(NULL,0), interval, 1000000);
}

int gui_async_init(){
    poll_looper_ = poll_looper_get_main();
    timer_queue_ = timer_queue_get_main();
    timer_queue_set_observer(timer_queue_, on_timer_queue_change, NULL);
    timer_queue_set_clock(timer_queue_, clock_get_tick);
	
	work_async_init(NULL, gui_async_post_callback);
    
    iosource_ = dispatch_source_create(DISPATCH_SOURCE_TYPE_READ, poll_looper_fd(poll_looper_), 0, dispatch_get_main_queue());
    
    timersource_=dispatch_source_create(DISPATCH_SOURCE_TYPE_TIMER, 0, 0, dispatch_get_main_queue());
    
    
    dispatch_source_set_event_handler(iosource_, ^{
        poll_event ev[20];
        int count = 20;
        while(1){
            if(0!=poll_looper_wait_events(poll_looper_, ev, &count, 0))break;
            poll_looper_process_events(poll_looper_, ev, count);
        }
    });
    
    
    dispatch_source_set_event_handler(timersource_, ^{
        int nextwait = 0;
        timer_queue_process(timer_queue_, &nextwait);
        on_timer_queue_change(NULL, nextwait);
    });
    
    dispatch_resume(iosource_);
    dispatch_resume(timersource_);
	return 0;
}

int gui_async_post_callback(gui_async_callback cb, void* userdata){
    if(!cb)return EINVAL;
    dispatch_async(dispatch_get_main_queue(), ^{
        cb(userdata);
    });
    return 0;
}
#endif


