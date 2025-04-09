// Copyright(c) 2021, Intel Corporation
//
// Redistribution  and  use  in source  and  binary  forms,  with  or  without
// modification, are permitted provided that the following conditions are met:
//
// * Redistributions of  source code  must retain the  above copyright notice,
//   this list of conditions and the following disclaimer.
// * Redistributions in binary form must reproduce the above copyright notice,
//   this list of conditions and the following disclaimer in the documentation
//   and/or other materials provided with the distribution.
// * Neither the name  of Intel Corporation  nor the names of its contributors
//   may be used to  endorse or promote  products derived  from this  software
//   without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING,  BUT NOT LIMITED TO,  THE
// IMPLIED WARRANTIES OF  MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
// ARE DISCLAIMED.  IN NO EVENT  SHALL THE COPYRIGHT OWNER  OR CONTRIBUTORS BE
// LIABLE  FOR  ANY  DIRECT,  INDIRECT,  INCIDENTAL,  SPECIAL,  EXEMPLARY,  OR
// CONSEQUENTIAL  DAMAGES  (INCLUDING,  BUT  NOT LIMITED  TO,  PROCUREMENT  OF
// SUBSTITUTE GOODS OR SERVICES;  LOSS OF USE,  DATA, OR PROFITS;  OR BUSINESS
// INTERRUPTION)  HOWEVER CAUSED  AND ON ANY THEORY  OF LIABILITY,  WHETHER IN
// CONTRACT,  STRICT LIABILITY,  OR TORT  (INCLUDING NEGLIGENCE  OR OTHERWISE)
// ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,  EVEN IF ADVISED OF THE
// POSSIBILITY OF SUCH DAMAGE.

#include "intel_st_debug_if_sockets.h"
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>

#include "intel_st_debug_if_st_dbg_ip_driver.h"
#include "intel_st_debug_if_common.h"
#include "intel_st_debug_if_constants.h"

#define PACKET_HEADER_SIZE 64

const struct timeval ZERO_TIMEOUT = {0, 0};

static char* g_socket_recv_buff = NULL;
static char* g_socket_send_buff = NULL;

RETURN_CODE alloc_tcpip_recv_send_buffer(size_t sz)
{
    g_socket_recv_buff = (char*) malloc((sz + PACKET_HEADER_SIZE) * sizeof(char));
    g_socket_send_buff = (char*) malloc((sz + PACKET_HEADER_SIZE) * sizeof(char));

    if (g_socket_recv_buff == NULL || g_socket_send_buff == NULL)
    {
        free_tcpip_recv_send_buffer();
        return FAILURE;
    }
    return OK;
}

void free_tcpip_recv_send_buffer()
{
    if (g_socket_recv_buff != NULL)
    {
        free(g_socket_recv_buff);
        g_socket_recv_buff = NULL;
    }

    if (g_socket_send_buff != NULL)
    {
        free(g_socket_send_buff);
        g_socket_send_buff = NULL;
    }
}

SOCKET max_of(SOCKET* array, int size)
{
    SOCKET result = 0;
    int i;
    for (i = 0; i < size; ++i)
    {
        result = MAX_MACRO(array[i], result);
    }
    return result;
}

RETURN_CODE socket_send_all(
    SOCKET fd, const char* buff, const size_t len, int flags, ssize_t* bytes_sent)
{
    ssize_t curr_bytes_sent;
    size_t bytes_remaining = len;

    while (bytes_remaining > 0)
    {
        if ((curr_bytes_sent = send(fd, buff + (len - bytes_remaining), bytes_remaining, flags)) <=
            0)
        {
            if (bytes_sent != NULL)
            {
                *bytes_sent = curr_bytes_sent;
            }
            return FAILURE;
        }
        bytes_remaining -= curr_bytes_sent;
    }
    if (bytes_sent != NULL)
    {
        *bytes_sent = len;
    }
    return OK;
}

RETURN_CODE socket_send_all_t2h_or_mgmt_rsp_data(
    SOCKET fd, uint64_t buff, const size_t len, int flags, ssize_t* bytes_sent)
{
    // First copy the mmio ptr into local memory domain
    memcpy64_fpga2host(buff, (uint64_t*) g_socket_send_buff, len);

    RETURN_CODE ret = socket_send_all(fd, g_socket_send_buff, len, flags, bytes_sent);

    return ret;
}

RETURN_CODE socket_recv_until_null_reached(
    SOCKET sock_fd, char* buff, const size_t max_len, int flags, ssize_t* bytes_recvd)
{
    ssize_t curr_bytes_recvd;
    size_t bytes_remaining = max_len;

    while (bytes_remaining > 0)
    {
        if ((curr_bytes_recvd = recv(sock_fd, buff, bytes_remaining, flags)) <= 0)
        {
            if (bytes_recvd != NULL)
            {
                *bytes_recvd = curr_bytes_recvd;  // Return the error
            }
            return FAILURE;
        }

        bytes_remaining -= curr_bytes_recvd;
        void* pos = memchr(buff, 0, curr_bytes_recvd);
        if (pos != NULL)
        {
            if (bytes_recvd != NULL)
            {
                *bytes_recvd = max_len - bytes_remaining;
            }
            return OK;
        }
        buff += curr_bytes_recvd;
    }

    // No null character was found within the bounded max length
    if (bytes_recvd != NULL)
    {
        *bytes_recvd = max_len;
    }
    return FAILURE;
}

RETURN_CODE socket_recv_accumulate(
    SOCKET sock_fd, char* buff, const size_t len, int flags, ssize_t* bytes_recvd)
{
    ssize_t curr_bytes_recvd;
    size_t bytes_remaining = len;

    while (bytes_remaining > 0)
    {
        if ((curr_bytes_recvd = recv(sock_fd, buff, bytes_remaining, flags)) <= 0)
        {
            if (bytes_recvd != NULL)
            {
                *bytes_recvd = curr_bytes_recvd;  // Return the error
            }
            return FAILURE;
        }
        bytes_remaining -= curr_bytes_recvd;
        buff += curr_bytes_recvd;
    }

    if (bytes_recvd != NULL)
    {
        *bytes_recvd = len;
    }

    return OK;
}

RETURN_CODE socket_recv_accumulate_h2t_or_mgmt_data(
    SOCKET sock_fd, uint64_t buff, const size_t len, int flags, ssize_t* bytes_recvd)
{
    RETURN_CODE rc = socket_recv_accumulate(sock_fd, g_socket_recv_buff, len, flags, bytes_recvd);

    if (rc != FAILURE)
    {
        // Copy the local memory ptr into the mmio domain
        memcpy64_host2fpga((uint64_t*) g_socket_recv_buff, buff, len);
    }

    return OK;
}

RETURN_CODE initialize_sockets_library()
{
#if STI_NOSYS_PROT_PLATFORM == STI_PLATFORM_WINDOWS
    WORD wVersionRequested;
    WSADATA wsaData;
    int err;

    wVersionRequested = MAKEWORD(1, 1);
    err = WSAStartup(wVersionRequested, &wsaData);
    if (err != 0)
    {
        // We couldn't find a useable winsock.dll
        printf("Failed to initialize Windows socket library component \"winsock.dll\"\n");
        return FAILURE;
    }
#endif
    return OK;
}

int set_boolean_socket_option(SOCKET socket_fd, int option, int option_val)
{
#if STI_NOSYS_PROT_PLATFORM == STI_PLATFORM_WINDOWS
    return setsockopt(socket_fd, SOL_SOCKET, option, (const char*) &option_val, sizeof(option_val));
#else
    return setsockopt(socket_fd, SOL_SOCKET, option, &option_val, sizeof(option_val));
#endif
}

int set_tcp_no_delay(SOCKET socket_fd, int no_delay)
{
#if STI_NOSYS_PROT_PLATFORM == STI_PLATFORM_WINDOWS
    return setsockopt(
        socket_fd, IPPROTO_TCP, TCP_NODELAY, (const char*) &no_delay, sizeof(no_delay));
#elif STI_NOSYS_PROT_PLATFORM == STI_PLATFORM_NIOS_UC_TCPIP
    // For no delay, uC/TCP-IP *requires* a CPU_BOOLEAN.
    CPU_BOOLEAN no_delay_bool = (no_delay != 0);
    return setsockopt(socket_fd, IPPROTO_TCP, TCP_NODELAY, &no_delay_bool, sizeof(no_delay_bool));
#else
    return setsockopt(socket_fd, IPPROTO_TCP, TCP_NODELAY, &no_delay, sizeof(no_delay));
#endif
}

int set_linger_socket_option(SOCKET socket_fd, int l_onoff, int l_linger)
{
// uC/TCP-IP does not support linger
#if STI_NOSYS_PROT_PLATFORM != STI_PLATFORM_NIOS_UC_TCPIP
    struct linger linger_opt_val;
#if STI_NOSYS_PROT_PLATFORM == STI_PLATFORM_WINDOWS
    linger_opt_val.l_linger = (u_short) l_linger;
    linger_opt_val.l_onoff = (u_short) l_onoff;
    return setsockopt(
        socket_fd, SOL_SOCKET, SO_LINGER, (const char*) &linger_opt_val, sizeof(linger_opt_val));
#else
    linger_opt_val.l_linger = l_linger;
    linger_opt_val.l_onoff = l_onoff;
    return setsockopt(socket_fd, SOL_SOCKET, SO_LINGER, &linger_opt_val, sizeof(linger_opt_val));
#endif  // end if STI_NOSYS_PROT_PLATFORM==STI_PLATFORM_WINDOWS
#endif  // end if STI_NOSYS_PROT_PLATFORM != STI_PLATFORM_NIOS_UC_TCPIP
}

char is_last_socket_error_would_block()
{
#if STI_NOSYS_PROT_PLATFORM == STI_PLATFORM_WINDOWS
    return (get_last_socket_error() == WSAEWOULDBLOCK) ? 1 : 0;
#else
    return (get_last_socket_error() == EAGAIN) ? 1 : 0;
#endif
}

int close_socket_fd(SOCKET socket_fd)
{
#if STI_NOSYS_PROT_PLATFORM == STI_PLATFORM_WINDOWS
    return closesocket(socket_fd);
#else
    return close(socket_fd);
#endif
}

void wait_for_read_event(SOCKET socket_fd, long seconds, long useconds)
{
    struct timeval timeout;
    timeout.tv_sec = seconds;
    timeout.tv_usec = useconds;
    fd_set readfds;
    FD_ZERO(&readfds);
    FD_SET(socket_fd, &readfds);
    select((int) (socket_fd + 1), &readfds, NULL, NULL, &timeout);
}

int get_last_socket_error()
{
#if STI_NOSYS_PROT_PLATFORM == STI_PLATFORM_WINDOWS
    return WSAGetLastError();
#else
    return errno;
#endif
}

const char* get_last_socket_error_msg(char* buff, size_t buff_sz)
{
#if STI_NOSYS_PROT_PLATFORM == STI_PLATFORM_WINDOWS
    int err = WSAGetLastError();
    if (err > 0)
    {
        FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
                      NULL,
                      err,
                      MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),  // Default language
                      (LPTSTR) buff,
                      buff_sz,
                      NULL);
        return buff;
    }
    else
    {
        return NULL;
    }
#else
    if (errno > 0)
    {
        return strerror(errno);
    }
    else
    {
        return NULL;
    }
#endif
}
