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

#ifdef __ANDROID__

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>
#include <unistd.h>
#include <jni.h>
#include <EGL/egl.h>
#include <GLES/gl.h>
#include <GLES/glext.h>
#include <android/asset_manager.h>
#include <android/input.h>
#include <android/sensor.h>
#include <android/log.h>
#include <android/native_activity.h>

#include "guiasync.h"
#include "testconfig.h"

#define APP_LOG_TAG "test"

#define LOGV(...)  ((void)__android_log_print(ANDROID_LOG_VERBOSE, APP_LOG_TAG, __VA_ARGS__))
#define LOGD(...)  ((void)__android_log_print(ANDROID_LOG_DEBUG, APP_LOG_TAG, __VA_ARGS__))
#define LOGI(...) ((void)__android_log_print(ANDROID_LOG_INFO, APP_LOG_TAG, __VA_ARGS__))
#define LOGW(...) ((void)__android_log_print(ANDROID_LOG_WARN, APP_LOG_TAG, __VA_ARGS__))
#define LOGE(...) ((void)__android_log_print(ANDROID_LOG_ERROR, APP_LOG_TAG, __VA_ARGS__))

typedef void (*ActivityRenderHandler)(void* udata);
typedef int (*ActivityEventHandler)(const AInputEvent* e, void* udata);

typedef struct _ActivityContext {
	ANativeActivity* activity;
	ANativeWindow* window;
	AInputQueue* queue;
	ARect rect;
	
	EGLDisplay egldisplay;
	EGLSurface eglsurface;
	EGLContext eglcontext;
		
	ActivityEventHandler onInputEvent;
	ActivityRenderHandler onRender;
	void* udata;
}ActivityContext;


static void onRender(void* udata) {
	GLfloat q3[] = {
		-0.5,-0.5,
		0.5,-0.5,
		0.5,0.5,
		-0.5,0.5
	};
	GLfloat col1[] = {
		1, 0, 0, 1.0f,
		0, 1, 0, 1.0f,
		0, 0, 1, 1.0f,
		0, 0, 0, 1.0f
	};

	glClearColor(0.5, 0.5, 0.5, 0);
	glClear(GL_COLOR_BUFFER_BIT);

	glPushMatrix();
	glScalef(0.5f, 0.5f, 0.5f);
	glColor4f(0.5, 0.6, 0.5, 0);

	glEnableClientState(GL_COLOR_ARRAY);
	glEnableClientState(GL_VERTEX_ARRAY);

	glColorPointer(4, GL_FLOAT, 0, col1);
	glVertexPointer(2, GL_FLOAT, 0, q3);

	glDrawArrays(GL_TRIANGLE_FAN, 0, 4);

	glDisableClientState(GL_VERTEX_ARRAY);
	glDisableClientState(GL_COLOR_ARRAY);

	glPopMatrix();
	glFlush();
}


static int onInputEvent(const AInputEvent* ev, void* udata){
	ActivityContext* ctx = (ActivityContext*)udata;
	int type = AInputEvent_getType(ev);
	if(type == AINPUT_EVENT_TYPE_KEY){
		int code = AKeyEvent_getKeyCode(ev);
		int act = AKeyEvent_getAction(ev);
		if(act == AKEY_EVENT_ACTION_UP && code == AKEYCODE_BACK){
			ANativeActivity_finish(ctx->activity);
			return 1;
		}
	}
	return 0;
}



static int EGLCreate(ANativeWindow* window, EGLDisplay* egldisplay, EGLSurface* eglsurface, EGLContext* eglcontext) {
	const EGLint attribs[] = {
		EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
		EGL_BLUE_SIZE, 8,
		EGL_GREEN_SIZE, 8,
		EGL_RED_SIZE, 8,
		EGL_NONE
	};
	EGLint format;
	EGLint numConfigs;
	EGLConfig config;
	EGLSurface surface;
	EGLContext context;
	EGLDisplay display;
	
	display = eglGetDisplay(EGL_DEFAULT_DISPLAY);

	if (!eglInitialize(display, 0, 0))LOGE("eglInitialize error");
	if (!eglChooseConfig(display, attribs, &config, 1, &numConfigs))LOGE("eglChooseConfig error");
	if (!eglGetConfigAttrib(display, config, EGL_NATIVE_VISUAL_ID, &format))LOGE("eglGetConfigAttrib error");

	ANativeWindow_setBuffersGeometry(window, 0, 0, format);

	surface = eglCreateWindowSurface(display, config, window, NULL);
	if (!surface)LOGE("eglCreateWindowSurface error");
	context = eglCreateContext(display, config, NULL, NULL);
	if (!context)LOGE("eglCreateContext error");

	if (eglMakeCurrent(display, surface, surface, context) == EGL_FALSE) {
		LOGE("Unable to eglMakeCurrent");		
		return -1;
	}

	*egldisplay = display;
	*eglsurface = surface;
	*eglcontext = context;
	return 0;
}

static void EGLDestroy(EGLDisplay display, EGLSurface surface, EGLContext context) {	
	eglMakeCurrent(display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
	if (context != EGL_NO_CONTEXT) {
		eglDestroyContext(display, context);
	}
	if (surface != EGL_NO_SURFACE) {
		eglDestroySurface(display, surface);
	}
	if (display != EGL_NO_DISPLAY) {
		eglTerminate(display);
	}
}

static void EGLResize(int w, int h) {
	static const GLfloat PI = 3.1415f;
	GLfloat fovy = 10;
	GLfloat aspect = 1;
	GLfloat zNear = -1;
	GLfloat zFar = 1;

	GLfloat top = zNear * ((GLfloat)tan(fovy * PI / 360.0));
	GLfloat bottom = -top;
	GLfloat left = bottom * aspect;
	GLfloat right = top * aspect;
	
	glViewport(0, 0, w, h);
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();
	glFrustumf(left, right, bottom, top, zNear, zFar);

	glEnable(GL_CULL_FACE);
	glShadeModel(GL_SMOOTH);
	glDisable(GL_DEPTH_TEST);
	glEnable(GL_TEXTURE_2D);
}


static int onInputQueueEvents(int fd, int events, void* data) {
	AInputEvent* event = NULL;
	ActivityContext* ctx = (ActivityContext*)data;
	int32_t evtype = 0;
	int32_t handled = 0;
	while (AInputQueue_getEvent(ctx->queue, &event) >= 0) {
		evtype = AInputEvent_getType(event);
		//LOGV("New input event: type=%d\n", evtype);
		if (AInputQueue_preDispatchEvent(ctx->queue, event)) {
			LOGV("AInputQueue_preDispatchEvent \n");
			continue;
		}
		handled = 0;
		if (ctx->onInputEvent != NULL) handled = ctx->onInputEvent(event, ctx->udata);
		AInputQueue_finishEvent(ctx->queue, event, handled);
	}
	return 1;
}



static void* onSaveInstanceState(ANativeActivity* activity, size_t* outLen) {
    LOGI("SaveInstanceState: %p\n", activity);
    return NULL;
}

static void onConfigurationChanged(ANativeActivity* activity) {   
    LOGI("ConfigurationChanged: %p\n", activity);
}

static void onLowMemory(ANativeActivity* activity) {
    LOGI("LowMemory: %p\n", activity);
}

static void onWindowFocusChanged(ANativeActivity* activity, int focused) {
    LOGI("WindowFocusChanged: %p -- %d\n", activity, focused);
}

static void onNativeWindowCreated(ANativeActivity* activity, ANativeWindow* window) {
	ActivityContext* ctx = (ActivityContext*)activity->instance;
    LOGI("NativeWindowCreated: %p -- %p\n", activity, window);
	ctx->window = window;
	EGLCreate(window, &ctx->egldisplay, &ctx->eglsurface, &ctx->eglcontext);
}

static void onNativeWindowDestroyed(ANativeActivity* activity, ANativeWindow* window) {
	ActivityContext* ctx = (ActivityContext*)activity->instance;
    LOGI("NativeWindowDestroyed: %p -- %p\n", activity, window);
	EGLDestroy(ctx->egldisplay, ctx->eglsurface, ctx->eglcontext);
}



static void onContentRectChanged(ANativeActivity* activity,const ARect* r) {
	ActivityContext* ctx = (ActivityContext*)activity->instance;
    LOGI("onContentRectChanged: %p - {%d,%d,%d,%d}\n", activity,r->left,r->top,r->right,r->bottom);  
	memcpy(&ctx->rect, r, sizeof(ARect));
	EGLResize(r->right - r->left, r->bottom - r->top);
}

static void onNativeWindowRedrawNeeded(ANativeActivity* activity, ANativeWindow* w) {
	ActivityContext* ctx = (ActivityContext*)activity->instance;
    LOGI("onNativeWindowRedrawNeeded: %p\n", activity);	
	if (ctx->onRender) {
		ctx->onRender(ctx->udata);
	}
	eglSwapBuffers(ctx->egldisplay, ctx->eglsurface);
}

static void onNativeWindowResized(ANativeActivity* activity, ANativeWindow* w) {
    LOGI("onNativeWindowResized: %p\n", activity);
}

static void onInputQueueCreated(ANativeActivity* activity, AInputQueue* queue) {
	ActivityContext* ctx = (ActivityContext*)activity->instance;
	ALooper* looper = ALooper_forThread();
	LOGI("InputQueueCreated: %p -- %p\n", activity, queue);
	ctx->queue = queue;
	AInputQueue_attachLooper(queue, looper, 199999, onInputQueueEvents, ctx);	

	
}

static void onInputQueueDestroyed(ANativeActivity* activity, AInputQueue* queue) {
	LOGI("InputQueueDestroyed: %p -- %p\n", activity, queue);
	AInputQueue_detachLooper(queue);
}


static void onStart(ANativeActivity* activity) {
	LOGI("Start: %p\n", activity);
}

static void onStop(ANativeActivity* activity) {
	LOGI("Stop: %p\n", activity);
}


static void onResume(ANativeActivity* activity) {
	LOGI("Resume: %p\n", activity);
	gui_async_init();
	run_all_tests(NULL);
}

static void onPause(ANativeActivity* activity) {
	LOGI("Pause: %p\n", activity);
}

static void onDestroy(ANativeActivity* activity) {
	ActivityContext* ctx = (ActivityContext*)activity->instance;
	LOGI("Destroy: %p\n", activity);
	free(ctx);
}

static char data_path_[1024];
const char* get_local_data_path() {
	return  data_path_;
}
void set_local_data_path(const char* dir) {
	snprintf(data_path_, 1024, "%s", dir);
}

#ifdef __cplusplus
extern "C"
#endif
void ANativeActivity_onCreate(ANativeActivity* activity,
        void* savedState, size_t savedStateSize) {  
	ActivityContext* ctx;
	LOGI("ANativeActivity_onCreate %p -- %p -- %d", activity, savedState, (int)savedStateSize);

	ctx = (ActivityContext*)malloc(sizeof(ActivityContext));
	memset(ctx, 0, sizeof(ActivityContext));

	set_local_data_path(activity->internalDataPath);

	ctx->activity = activity;
	ctx->udata = ctx;
	ctx->onInputEvent = onInputEvent;
	ctx->onRender = onRender;

	activity->instance = ctx;
	
	activity->callbacks->onConfigurationChanged = onConfigurationChanged;
	activity->callbacks->onContentRectChanged  = onContentRectChanged;
	activity->callbacks->onDestroy = onDestroy;
	activity->callbacks->onInputQueueCreated = onInputQueueCreated;
	activity->callbacks->onInputQueueDestroyed = onInputQueueDestroyed;
	activity->callbacks->onLowMemory = onLowMemory;
	activity->callbacks->onNativeWindowCreated =  onNativeWindowCreated;
	activity->callbacks->onNativeWindowDestroyed =  onNativeWindowDestroyed;
	activity->callbacks->onNativeWindowRedrawNeeded =  onNativeWindowRedrawNeeded;
	activity->callbacks->onNativeWindowResized =  onNativeWindowResized;
	activity->callbacks->onPause = onPause;
	activity->callbacks->onResume = onResume;
	activity->callbacks->onSaveInstanceState = onSaveInstanceState;
	activity->callbacks->onStart = onStart;
	activity->callbacks->onStop = onStop;
	activity->callbacks->onWindowFocusChanged = onWindowFocusChanged;	
}

#endif
