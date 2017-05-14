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

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <sys/stat.h>
#endif


#ifdef __cplusplus
extern "C" {
#endif 

#ifndef _FD_T_DEFINED_
#define _FD_T_DEFINED_
#ifdef _WIN32
	typedef SOCKET fd_t;
#else
	typedef int fd_t;
#endif
#endif


#ifdef _WIN32
	
//test error code
#define io_is_in_progress(code) ((code) == WSAEINPROGRESS || (code) == WSAEWOULDBLOCK)
#define io_is_would_block(code) ((code) == WSAEWOULDBLOCK || (code) == ERROR_IO_PENDING) 

#define is_valid_file(fd) (fd)
#define is_valid_socket(fd) (fd!=INVALID_SOCKET)

#else
#define io_is_in_progress(code) ((code) == EINPROGRESS)
#define io_is_would_block(code) ((code) == EWOULDBLOCK) 

#define closesocket close

#define is_valid_file(fd) (fd>=0)
#define is_valid_socket(fd) (fd>=0)
#endif

/** set nonblocking */
int io_set_nonblocking(fd_t fd);


#ifdef _WIN32
#ifdef IO_ASYNC_USE_IOCP
#define CREATE_FILE_FLAG_DEFAULT  FILE_FLAG_OVERLAPPED 
#else
#define CREATE_FILE_FLAG_DEFAULT  0
#endif
#define open_for_read(filename) (fd_t)CreateFileA(filename, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, CREATE_FILE_FLAG_DEFAULT, NULL)
#define open_for_write(filename) (fd_t)CreateFileA(filename, GENERIC_WRITE, FILE_SHARE_READ, NULL, CREATE_ALWAYS, CREATE_FILE_FLAG_DEFAULT, NULL)
#define closefile(fd) CloseHandle((HANDLE)fd)
#else

#define open_for_read(filename) open(filename,O_RDONLY)
#define open_for_write(filename) open(filename,O_WRONLY|O_CREAT|O_TRUNC,S_IRWXU|S_IRWXG|S_IRWXO);
#define closefile(fd) close(fd)
#endif


#ifdef __cplusplus
}
#endif 



