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

#pragma once

#include <stdlib.h>
#include "packet.h"
#include "platform.h"

#ifdef __cplusplus
extern "C" {
#endif

#define ALIGN_TO(n) (((n) >> ST_DBG_IP_BUFF_ALIGN_POW_2) << ST_DBG_IP_BUFF_ALIGN_POW_2)
#define GET_ALIGNED_SZ(n) (ALIGN_TO(((n) + (1 << ST_DBG_IP_BUFF_ALIGN_POW_2) - 1)))

#define SUPPORTED_TYPE 0x5244444D
#define SUPPORTED_VERSION 0x0
#define INIT_ERROR_CODE_MISSING_INFO -1
#define INIT_ERROR_CODE_INCOMPATIBLE_IP -2

// Customize here from FPGA design
#define ST_DBG_IF_BASE 0x0000
#define JOP_MEM_BASE 0x1000

typedef struct {
    uint32_t ST_DBG_IP_CSR_BASE_ADDR;

    uint32_t H2T_MEM_BASE_ADDR;
    size_t H2T_MEM_SZ;

    uint32_t T2H_MEM_BASE_ADDR;
    size_t T2H_MEM_SZ;

    uint32_t MGMT_MEM_BASE_ADDR;
    size_t MGMT_MEM_SZ;

    uint32_t MGMT_RSP_MEM_BASE_ADDR;
    size_t MGMT_RSP_MEM_SZ;
} ST_DBG_IP_DESIGN_INFO;

extern ST_DBG_IP_DESIGN_INFO g_std_dbg_ip_info;

#define HW_LOOPBACK_PARAM "#HW_LOOPBACK"
#define HW_LOOPBACK_PARAM_LEN 13

// The ST Debug IP allows these to be queried dynamically, but since we are not using malloc,
// I will reserve enough space for the upperlimit of how many descriptors the IP supports.
#define MAX_H2T_DESCRIPTOR_DEPTH 128
#define MAX_MGMT_DESCRIPTOR_DEPTH 128

// This is used to keep addresses passed to the H2T / MGMT CSR aligned to the native word size
// of the ST Debug IP's DMA masters.
#define ST_DBG_IP_BUFF_ALIGN_POW_2 3 // Aligned to 64-bit boundaries

// Config CSR
#define ST_DBG_IP_CONFIG_TYPE 0x0
#define ST_DBG_IP_CONFIG_VERSION 0x4
#define ST_DBG_IP_CONFIG_VERSION_MASK 0xF

#define ST_DBG_IP_CONFIG_RESET_AND_LOOPBACK 0x20
#define ST_DBG_IP_CONFIG_H2T_T2H_RESET_FIELD 0x1
#define ST_DBG_IP_CONFIG_LOOPBACK_FIELD 0x2
#define ST_DBG_IP_CONFIG_ENABLE_INT_FIELD 0x4

#define ST_DBG_IP_CONFIG_H2T_T2H_MEM 0x24
#define ST_DBG_IP_CONFIG_MGMT_MGMT_RSP_MEM 0x28
#define ST_DBG_IP_CONFIG_H2T_T2H_DESC_DEPTH 0x2C
#define ST_DBG_IP_CONFIG_MGMT_MGMT_RSP_DESC_DEPTH 0x30

#define ST_DBG_IP_CONFIG_INTERRUPTS 0x48
#define ST_DBG_IP_CONFIG_MASK_H2T_FIELD 0x1
#define ST_DBG_IP_CONFIG_MASK_T2H_FIELD 0x2
#define ST_DBG_IP_CONFIG_MASK_MGMT_FIELD 0x4
#define ST_DBG_IP_CONFIG_MASK_MGMT_RSP_FIELD 0x8

// H2T CSR
#define ST_DBG_IP_H2T_AVAILABLE_SLOTS 0x100
#define ST_DBG_IP_H2T_HOW_LONG 0x108
#define ST_DBG_IP_H2T_WHERE 0x10C
#define ST_DBG_IP_H2T_CONNECTION_ID 0x110
#define ST_DBG_IP_H2T_CHANNEL_ID_PUSH 0x114

// T2H CSR
#define ST_DBG_IP_T2H_HOW_LONG 0x208
#define ST_DBG_IP_T2H_WHERE 0x20C
#define ST_DBG_IP_T2H_CONNECTION_ID 0x210
#define ST_DBG_IP_T2H_CHANNEL_ID_ADVANCE 0x214
#define ST_DBG_IP_T2H_DESCRIPTORS_DONE 0x218

// MGMT CSR
#define ST_DBG_IP_MGMT_AVAILABLE_SLOTS 0x1100
#define ST_DBG_IP_MGMT_HOW_LONG 0x1108
#define ST_DBG_IP_MGMT_WHERE 0x110C
#define ST_DBG_IP_MGMT_CHANNEL_ID_PUSH 0x1114

// MGMT_RSP CSR
#define ST_DBG_IP_MGMT_RSP_HOW_LONG 0x1208
#define ST_DBG_IP_MGMT_RSP_WHERE 0x120C
#define ST_DBG_IP_MGMT_RSP_CHANNEL_ID_ADVANCE 0x1214
#define ST_DBG_IP_MGMT_RSP_DESCRIPTORS_DONE 0x1218

// Common masks
#define ST_DBG_IP_LAST_DESCRIPTOR_MASK 0x80000000
#define ST_DBG_IP_HOW_LONG_MASK 0x7FFFFFFF

// Driver init
int init_driver();
void set_design_info(ST_DBG_IP_DESIGN_INFO info);

// H2T
uint32_t get_h2t_buffer(size_t sz);
int push_h2t_data(H2T_PACKET_HEADER *header, uint32_t payload);

// MGMT
uint32_t get_mgmt_buffer(size_t sz);
int push_mgmt_data(MGMT_PACKET_HEADER *header, uint32_t payload);

// T2H
int get_t2h_data(H2T_PACKET_HEADER *header, uint32_t *payload);
void t2h_data_complete();

// MGMT RSP
int get_mgmt_rsp_data(MGMT_PACKET_HEADER *header, uint32_t *payload);
void mgmt_rsp_data_complete();

// Config CSR
void set_loopback_mode(int val);
int get_loopback_mode();
void enable_interrupts(int val);
int get_mgmt_support();
int check_version_and_type(); // A non-zero return value indicates the IP is incompatible
void assert_h2t_t2h_reset();

// buffer data exchange
void memcpy64_fpga2host(int32_t fpga_buff, uint64_t *host_buff, size_t len);
void memcpy64_host2fpga(uint64_t *host_buff, int32_t fpga_buff, size_t len);

// Misc settings
int set_driver_param(const char *param, const char *val);
char *get_driver_param(const char *param);

#ifdef __cplusplus    
}
#endif
