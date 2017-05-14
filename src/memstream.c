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

#include "memstream.h"



#define MEMORY_STREAM_DEFAULT_SIZE 4096

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#ifdef _WIN32
#include <windows.h>
//#define ERRNO_NOMEM ERROR_OUTOFMEMORY
#else
#define ERROR_OUTOFMEMORY ENOMEM

#endif

struct _memory_stream{
	char* mem;
	char* memend;	
	char* posread;
	char* poswrite;	
	size_t datalen;	
};


int memory_stream_open(memory_stream_t* mems, size_t sz){	
	memory_stream_t result;
	void* buf;
	*mems = NULL;
	if(sz==0)sz = MEMORY_STREAM_DEFAULT_SIZE;
	result = (memory_stream_t)malloc(sizeof(struct _memory_stream));
	if(!result){ return ERROR_OUTOFMEMORY;}
	buf = malloc(sz);
	if(!buf){ free(result); return ERROR_OUTOFMEMORY;}
	memset(result, 0, sizeof(struct _memory_stream));	
	result->mem = (char*)buf;
	result->memend = result->mem + sz;	
	result->poswrite = result->mem;
	result->posread = result->mem;
	*mems = result;
	return 0;
}



int memory_stream_close(memory_stream_t mems){
	free(mems->mem);
	free(mems);
	return 0;
}



size_t memory_stream_write(memory_stream_t mems, const void* buf, size_t buflen){
	size_t writelen = buflen;
	size_t mremainlenl, mremainlenr, remainlen;
	if(!buf)return 0;
	if(writelen==0)return 0;
	if(mems->posread > mems->poswrite){  //---wp---rp------
		mremainlenr = mems->posread - mems->poswrite;
		mremainlenl = 0;
	}else{ //---rp--wp---------
		mremainlenr = mems->memend - mems->poswrite;
		mremainlenl = mems->posread - mems->mem;
	}
	if(writelen > (mremainlenl + mremainlenr)){ //no enough memory		
		return 0;
	}
	if(writelen <= mremainlenr){
		memcpy(mems->poswrite, buf, writelen);
		mems->poswrite += writelen;		
	}else{
		memcpy(mems->poswrite, buf, mremainlenr);
		remainlen = writelen - mremainlenr;
		if(remainlen)memcpy(mems->mem, (char*)buf + mremainlenr, remainlen);
		mems->poswrite = mems->mem + remainlen;
	}
	mems->datalen += writelen;
	return buflen;
}



size_t memory_stream_read(memory_stream_t mems, void* buf, size_t buflen){
	size_t rdlen,remainlen;
	size_t mremainlenl, mremainlenr;
	if(!buf || !buflen)return 0;
	rdlen = buflen;
	if(rdlen<=0)return 0;
	if(mems->datalen==0){		
		return 0;
	}
	if(mems->posread >= mems->poswrite){  //---wp---rp------
		mremainlenr = mems->memend - mems->posread;
		mremainlenl = mems->poswrite - mems->mem;
	}else{ //---rp--wp---------
		mremainlenr = mems->poswrite - mems->posread;
		mremainlenl = 0;
	}
	if((size_t)rdlen > mems->datalen)rdlen = mems->datalen;
	if(rdlen <= mremainlenr){
		memcpy(buf, mems->posread, rdlen);		
		mems->posread += rdlen;
	}else{
		memcpy(buf, mems->posread, mremainlenr);
		remainlen = rdlen - mremainlenr;
		if(remainlen > mremainlenl) remainlen = mremainlenl;
		if(remainlen)memcpy((char*)buf + mremainlenr, mems->mem, remainlen);
		mems->posread = mems->mem + remainlen;		
	}
	mems->datalen -= rdlen;
	return rdlen;
}
