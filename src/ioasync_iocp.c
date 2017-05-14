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

#if defined(_MSC_VER) && (_MSC_VER < 1900)
#define snprintf(buf,buflen,...) _snprintf_s(buf,buflen,buflen,__VA_ARGS__)
#endif

#include "ioasync.h"

#ifdef IO_ASYNC_USE_IOCP

#include <mswsock.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>



#define IO_RUN_KEY   0x7feeff11



#define IO_DEBUG(...)  //printf(__VA_ARGS__);

#define offset_of(TYPE, MEMBER) ((size_t) &((TYPE *)0)->MEMBER)
#define container_of(ptr, type, member)  (type *)( (char *)ptr - offset_of(type,member) )

typedef void (*pre_handle_callback)(DWORD transfered, void* comkey, void* fut);


typedef struct io_future_impl {
	io_callback cb;
	void* udata;
	int ready;
	int error;
	size_t length;	
	OVERLAPPED overlapped;
	WSABUF wsabuf;	  //for WSASend/WSARecv
	DWORD wsaflags;  //for WSASend/WSARecv		
	char* paddrbuf;
	int* paddrlen;
	pre_handle_callback precb;
}io_future_impl;

typedef char accert_ctx_size[(sizeof(io_future) >= sizeof(io_future_impl)) ? 1 : -1];

#define async_set_future_pos(fut, offset)  *(uint64_t*)(&(fut->overlapped.Offset)) = (offset)
#define async_add_ctx_pos(ctx, len) *(uint64_t*)(&(ctx->offset)) += (len)

#define last_error() (GetLastError()?GetLastError():-1)


typedef BOOL (WINAPI *GetQueuedCompletionStatusExProc)(HANDLE,LPOVERLAPPED_ENTRY,ULONG,PULONG,DWORD,BOOL);

static GUID GuidAcceptEx = WSAID_ACCEPTEX;
static GUID GuidConnectEx = WSAID_CONNECTEX;
static GUID GuidDisconnectEx = WSAID_DISCONNECTEX;
static GUID GuidGetAcceptExSockaddrs = WSAID_GETACCEPTEXSOCKADDRS;

static LPFN_ACCEPTEX AcceptEx_;
static LPFN_CONNECTEX ConnectEx_;
static LPFN_DISCONNECTEX DisconnectEx_;
static LPFN_GETACCEPTEXSOCKADDRS GetAcceptExSockaddrs_;
static GetQueuedCompletionStatusExProc GetQueuedCompletionStatusEx_;

#ifdef _DEBUG
static void error_print(int code, const char* operation){
	char errmsg[256];	
	char displaymsg[512];
	errmsg[0] = 0;
    FormatMessageA(FORMAT_MESSAGE_FROM_SYSTEM|FORMAT_MESSAGE_IGNORE_INSERTS,NULL,code,1033,errmsg,256,NULL);
	snprintf(displaymsg, 512, "%s error %d %s\n", operation, code, errmsg);	
	OutputDebugStringA(displaymsg);	
}
#else
#define error_print(code, opt)
#endif



#define goto_error(code, opt) {ret = code; error_print(ret, opt); goto error;}
#define return_error(code, opt) {ret = code; error_print(ret, opt); return ret?ret:-1;}


static int get_socket_ex_functions(SOCKET s) {
	int ret;
	DWORD len;
	ret = WSAIoctl(s, SIO_GET_EXTENSION_FUNCTION_POINTER, &GuidAcceptEx, sizeof(GuidAcceptEx), &AcceptEx_, sizeof(AcceptEx_), &len, NULL, NULL);
	ret = ret | WSAIoctl(s, SIO_GET_EXTENSION_FUNCTION_POINTER, &GuidConnectEx, sizeof(GuidConnectEx), &ConnectEx_, sizeof(ConnectEx_), &len, NULL, NULL);
	ret = ret | WSAIoctl(s, SIO_GET_EXTENSION_FUNCTION_POINTER, &GuidDisconnectEx, sizeof(GuidDisconnectEx), &DisconnectEx_, sizeof(DisconnectEx_), &len, NULL, NULL);
	ret = ret | WSAIoctl(s, SIO_GET_EXTENSION_FUNCTION_POINTER, &GuidGetAcceptExSockaddrs, sizeof(GuidGetAcceptExSockaddrs), &GetAcceptExSockaddrs_, sizeof(GetAcceptExSockaddrs_), &len, NULL, NULL);
	if(ret)error_print(WSAGetLastError(), "WSAIoctl");
	return ret;
}


static int io_api_init() {
	SOCKET s;
	WSADATA wsa;
	int ret;

	WSAStartup(MAKEWORD(2, 2), &wsa);

	GetQueuedCompletionStatusEx_ = (GetQueuedCompletionStatusExProc)GetProcAddress(GetModuleHandle(TEXT("kernel32.dll")), "GetQueuedCompletionStatusEx");

	//get extension api from a generic tcp socket
	s = socket(AF_INET, SOCK_STREAM, 0);
	if (s == INVALID_SOCKET)goto_error(WSAGetLastError(), "socket");
	if (get_socket_ex_functions(s))goto error;
	closesocket(s);

	return 0;

error:	
	if (s != INVALID_SOCKET)closesocket(s);
	return ret;
}

io_looper_t io_looper_get_main() {
	static io_looper_t mainlooper;
	if (!mainlooper) {
		io_looper_create(&mainlooper);
	}
	return mainlooper;
}

int io_looper_create(io_looper_t* looper){
	static long inited;
	io_looper_t plooper;
	*looper = NULL;
	if(!looper)return ERROR_INVALID_PARAMETER;
	if(!inited){
		inited = 1;
		io_api_init();
	}
	plooper = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, 0);
	if(!plooper)return last_error();
	*looper = plooper;
	return 0;
}

int io_looper_destroy(io_looper_t looper){
	CloseHandle(looper);
	return 0;
}


int io_looper_wait_events(io_looper_t looper, io_event* results, int* count, int waittime){
	BOOL bret;
	ULONG resultcount=0;

	if(!results || !count || *count<=0)return ERROR_INVALID_PARAMETER;
	if(GetQueuedCompletionStatusEx_){ //has GetQueuedCompletionStatusEx (on vista or later)	
		bret = GetQueuedCompletionStatusEx_(looper, results, *count, &resultcount, waittime, TRUE);
		IO_DEBUG("[IO] GetQueuedCompletionStatusEx_ return:%d, count:%d\n",bret, resultcount);
		if(!bret)resultcount = 0; //resultcount may > 0 when dequeue nonthing
	}else{
		results->lpCompletionKey = 0;
		results->lpOverlapped = NULL;
		bret = GetQueuedCompletionStatus(looper, &(results->dwNumberOfBytesTransferred), 
			&(results->lpCompletionKey), &(results->lpOverlapped), waittime);
		IO_DEBUG("[IO] GetQueuedCompletionStatus return:%d\n",bret);
		if(results->lpOverlapped || results->lpCompletionKey == IO_RUN_KEY)resultcount = 1;
	}
	if(resultcount>0){
		*count = resultcount;		
		return 0;
	}
	return last_error();
}

int io_looper_process_events(io_looper_t looper, io_event* results, int count){
	int i,len;	
	BOOL bret;
	DWORD temp;
	io_future_impl* ctx;
	io_callback cb;
	for(i=0;i<count;i++){
		io_event* result = &(results[i]);
		if (result->dwNumberOfBytesTransferred == IO_RUN_KEY) {	
			cb = (io_callback)result->lpCompletionKey;
			if(cb)cb(result->lpOverlapped);
			continue;
		}
		ctx = container_of(result->lpOverlapped, io_future_impl, overlapped);
		if(!ctx)continue;
		
		bret = GetOverlappedResult(NULL, result->lpOverlapped, &temp, FALSE);
		if (bret) { //result ok
			ctx->error = 0;	
			len = result->dwNumberOfBytesTransferred;			
		} else { //result error 
			ctx->error = last_error();
			len = 0;	
		}
		ctx->length = len;
		ctx->ready = 1;
		if (ctx->precb) ctx->precb(len, (void*)result->lpCompletionKey, ctx);
		if(ctx->cb)ctx->cb(ctx->udata);
	}
	return 0;
}

int io_looper_post_callback(io_looper_t looper, io_callback cb, void* udata){
	int ret;
	BOOL bret = PostQueuedCompletionStatus(looper, IO_RUN_KEY, (ULONG_PTR)cb, (LPOVERLAPPED)udata);
	if(!bret)return_error(GetLastError(), "PostQueuedCompletionStatus");
	return 0;	
}


int io_async_init(io_ctx* outctx, io_looper_t looper, fd_t fd, int flags){
	int ret;
	if (NULL == CreateIoCompletionPort((HANDLE)fd, looper, (ULONG_PTR)outctx, 0)) {
		return_error(GetLastError(), "CreateIoCompletionPort");
	}
	outctx->fd = fd;
	outctx->flags = flags;
	outctx->offset = 0;
	return 0;
}


#define return_inval() return ERROR_INVALID_PARAMETER

static void after_async_read_write(DWORD len, void* comkey, void* arg){
	io_ctx* ctx = (io_ctx*)comkey;
	async_add_ctx_pos(ctx, len);
}

int io_async_read(io_ctx* ioctx, uint64_t offset, char* buf, size_t buflen, io_future* pctx) {	
	BOOL bret;
	int ret;
	io_future_impl* ctx;
	if(!ioctx || !buf || !pctx || buflen<0)return_inval();

	ctx = (io_future_impl*)pctx;
	if (ioctx->flags & IO_FLAG_RANDOM_ACCESS) {
		async_set_future_pos(ctx, offset);
		ctx->precb = NULL;
	}else{
		async_set_future_pos(ctx, ioctx->offset);
		ctx->precb = after_async_read_write;
	}
	ctx->ready = 0;	
	//IO_DEBUG("[IO] async ReadFile (%d, %d)\n", (int)fd, dwbuflen);
	bret = ReadFile((HANDLE)ioctx->fd, buf, (DWORD)buflen, NULL, &ctx->overlapped);
	if (bret || ERROR_IO_PENDING == GetLastError()) return 0;	
	return_error(GetLastError(), "ReadFile");
}

int io_async_write(io_ctx* ioctx, uint64_t offset, const char* buf, size_t buflen, io_future* pctx) {
	BOOL bret;
	int ret;
	io_future_impl* ctx;
	if(!ioctx || !buf || !pctx || buflen<0)return_inval();

	ctx = (io_future_impl*)pctx;
	if (ioctx->flags & IO_FLAG_RANDOM_ACCESS) {
		async_set_future_pos(ctx, offset);
		ctx->precb = NULL;
	}else{
		async_set_future_pos(ctx, ioctx->offset);
		ctx->precb = after_async_read_write;
	}
	ctx->ready = 0;	
	//IO_DEBUG("[IO] async WriteFile (%d, %d)\n", (int)fd, dwbuflen);
	bret = WriteFile((HANDLE)ioctx->fd, buf, (DWORD)buflen, NULL, &ctx->overlapped);
	if (bret || ERROR_IO_PENDING == GetLastError()) return 0;	
	return_error(GetLastError(), "WriteFile");
}


int io_async_connect(io_ctx* ioctx, const struct sockaddr* addr, int addrlen, io_future* pctx) {
	BOOL bret;
	int ret;
	io_future_impl* ctx;
	struct sockaddr_storage dummyaddr; //dummy addr for bind

	if(!ioctx || !addr || !pctx || addrlen<0)return_inval();

	ctx = (io_future_impl*)pctx;
		
	//must bind befor ConnectEx
	ZeroMemory(&dummyaddr, (addrlen < sizeof(dummyaddr)) ? addrlen : sizeof(dummyaddr));
	dummyaddr.ss_family = addr->sa_family;
	if (bind((SOCKET)ioctx->fd, (const struct sockaddr*)&dummyaddr, addrlen)) {
		error_print(WSAGetLastError(), "bind");
		//IO_DEBUG("[IO] io_async_connect() bind error(%d)\n", WSAGetLastError());
	}

	//IO_DEBUG("[IO] ConnectEx (%d)\n", (int)fd);
	ctx->ready = 0;
	ctx->precb = NULL;
	bret = ConnectEx_((SOCKET)ioctx->fd, addr, addrlen, NULL, 0, NULL, &ctx->overlapped);
	if (bret || ERROR_IO_PENDING == GetLastError()) return 0;	
	return_error(WSAGetLastError(), "ConnectEx");
}


static void after_async_accept(DWORD len, void* comkey, void* arg) {
	io_future_impl* ctx = (io_future_impl*)arg;
	struct sockaddr* localaddr = NULL;
	struct sockaddr* remoteaddr = NULL;
	int locallen = sizeof(struct sockaddr_storage);
	int remotelen = sizeof(struct sockaddr_storage);
	int buflen = *ctx->paddrlen;	
	
	GetAcceptExSockaddrs_(ctx->paddrbuf, 0, 0, buflen, &localaddr, &locallen, &remoteaddr, &remotelen);
	if (remoteaddr && remotelen && remotelen != sizeof(struct sockaddr_storage)) {
		if (remotelen > buflen)remotelen = buflen;
		memmove(ctx->paddrbuf, remoteaddr, remotelen);
		*ctx->paddrlen = remotelen;
	}
}

int io_async_accept(io_ctx* ioctx, fd_t* outfd, struct sockaddr* addrbuf, int* addrlen, io_future* pctx) {
	SOCKET client = INVALID_SOCKET;
	io_future_impl* ctx;
	BOOL bret;
	int ret;
	struct sockaddr_storage addrstg;
	int buflen;

	if(!ioctx || !outfd || !pctx)return_inval();

	ctx = (io_future_impl*)pctx;

	//must create client socket befor AcceptEx		
	buflen = sizeof(addrstg);
	ret = getsockname((SOCKET)ioctx->fd, (struct sockaddr*) &addrstg, &buflen);
	if(ret)goto_error(WSAGetLastError(), "getsockname");
	client = socket(addrstg.ss_family, SOCK_STREAM, 0);
	if (client == INVALID_SOCKET)goto_error(WSAGetLastError(), "socket");

	ctx->ready = 0;	
	ctx->paddrbuf = (char*)addrbuf;
	ctx->paddrlen = addrlen;
	ctx->precb = after_async_accept;
	bret = AcceptEx_((SOCKET)ioctx->fd, client, addrbuf, 0, 0, *addrlen, NULL, &ctx->overlapped);
	if (bret || ERROR_IO_PENDING == GetLastError()) {
		*outfd = client;
		return 0;
	}
	goto_error(WSAGetLastError(), "AcceptEx");
error:
	if(client != INVALID_SOCKET)closesocket(client);
	return ret?ret:-1;
}

int io_async_recv(io_ctx* ioctx, char* buf, size_t buflen, io_future* pctx) {	
	io_future_impl* ctx;
	BOOL bret;
	int ret;
	if(!ioctx || !buf || !pctx || buflen<0)return_inval();

	ctx = (io_future_impl*)pctx;
	ctx->wsabuf.buf = (char*)buf;
	ctx->wsabuf.len = (ULONG)buflen;
	ctx->wsaflags = 0;
	ctx->ready = 0;
	ctx->precb = NULL;
	//IO_DEBUG("[IO] WSARecv (%d, %d)\n", (int)fd, buflen);
	bret = (0 == WSARecv((SOCKET)ioctx->fd, &ctx->wsabuf, 1, NULL, &ctx->wsaflags, &ctx->overlapped, NULL));
	if (bret || ERROR_IO_PENDING == GetLastError()) return 0;	
	return_error(WSAGetLastError(), "WSARecv");
}

int io_async_send(io_ctx* ioctx, const char* buf, size_t buflen, io_future* pctx) {
	io_future_impl* ctx;
	BOOL bret;
	int ret;
	if(!ioctx || !buf || !pctx || buflen<0)return_inval();

	ctx = (io_future_impl*)pctx;
	ctx->wsabuf.buf = (char*)buf;
	ctx->wsabuf.len = (ULONG)buflen;
	ctx->wsaflags = 0;
	ctx->ready = 0;
	ctx->precb = NULL;
	//IO_DEBUG("[IO] WSASend (%d, %d)\n", (int)fd, buflen);
	bret = (0 == WSASend((SOCKET)ioctx->fd, &ctx->wsabuf, 1, NULL, ctx->wsaflags, &ctx->overlapped, NULL));
	if (bret || ERROR_IO_PENDING == GetLastError()) return 0;	
	return_error(WSAGetLastError(), "WSASend");
}



int io_async_recvfrom(io_ctx* ioctx, char* buf, size_t buflen, struct sockaddr* addrbuf, int* addrlen, io_future* pctx) {
	BOOL bret;
	int ret;
	io_future_impl* ctx;
	if(!ioctx || !buf || !pctx || buflen<0)return_inval();

	ctx = (io_future_impl*)pctx;
	ctx->wsabuf.buf = (char*)buf;
	ctx->wsabuf.len = (ULONG)buflen;
	ctx->wsaflags = MSG_PARTIAL;
	ctx->ready = 0;
	ctx->precb = NULL;
	IO_DEBUG("[IO] WSARecvFrom (%d, %d)\n", (int)fd, buflen);
	bret = (0 == WSARecvFrom((SOCKET)ioctx->fd, &ctx->wsabuf, 1, NULL, &ctx->wsaflags, addrbuf, addrlen, &ctx->overlapped, NULL));
	if (bret || ERROR_IO_PENDING == GetLastError()) return 0;	
	return_error(WSAGetLastError(), "WSARecvFrom");
}

int io_async_sendto(io_ctx* ioctx, const char* buf, size_t buflen, const struct sockaddr* addr, int addrlen, io_future* pctx) {
	BOOL bret;
	int ret;
	io_future_impl* ctx;
	if(!ioctx || !buf || !pctx || buflen<0)return_inval();

	ctx = (io_future_impl*)pctx;
	ctx->wsabuf.buf = (char*)buf;
	ctx->wsabuf.len = (ULONG)buflen;
	ctx->wsaflags = 0;
	ctx->ready = 0;
	ctx->precb = NULL;
	IO_DEBUG("[IO] WSASendTo (%d, %d)\n", (int)fd, buflen);
	bret = (0 == WSASendTo((SOCKET)ioctx->fd, &ctx->wsabuf, 1, NULL, ctx->wsaflags, addr, addrlen, &ctx->overlapped, NULL));
	if (bret || ERROR_IO_PENDING == GetLastError()) return 0;	
	return_error(WSAGetLastError(), "WSASendTo");
}



#endif



