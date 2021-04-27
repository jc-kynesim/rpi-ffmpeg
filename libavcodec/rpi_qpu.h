/*
Copyright (c) 2018 Raspberry Pi (Trading) Ltd.
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:
    * Redistributions of source code must retain the above copyright
      notice, this list of conditions and the following disclaimer.
    * Redistributions in binary form must reproduce the above copyright
      notice, this list of conditions and the following disclaimer in the
      documentation and/or other materials provided with the distribution.
    * Neither the name of the copyright holder nor the
      names of its contributors may be used to endorse or promote products
      derived from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY
DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

Authors: John Cox, Ben Avison
*/

#ifndef RPI_QPU_H
#define RPI_QPU_H

#include "rpi_mem.h"
#include "rpi_zc_frames.h"

#pragma GCC diagnostic push
// Many many redundant decls in the header files
#pragma GCC diagnostic ignored "-Wredundant-decls"
#pragma GCC diagnostic ignored "-Wstrict-prototypes"
#include "interface/vmcs_host/vc_vchi_gpuserv.h"  // for gpu_job_s
#pragma GCC diagnostic pop

// QPU specific functions

typedef struct HEVCRpiQpu {
    uint32_t c_pxx;
    uint32_t c_pxx_l1;
    uint32_t c_bxx;
    uint32_t y_pxx;
    uint32_t y_bxx;
    uint32_t y_p00;
    uint32_t y_b00;
} HEVCRpiQpu;

int rpi_hevc_qpu_init_fn(HEVCRpiQpu * const qf, const unsigned int bit_depth);

uint32_t qpu_fn(const int * const mc_fn);
uint32_t qpu_dummy(void);

#define QPU_N_GRP    4
#define QPU_N_MAX    12

#define QPU_MAIL_EL_VALS  2

struct vpu_qpu_wait_s;
typedef struct vq_wait_s * vpu_qpu_wait_h;

// VPU specific functions

struct vpu_qpu_job_env_s;
typedef struct vpu_qpu_job_env_s * vpu_qpu_job_h;

#define VPU_QPU_JOB_MAX 4
struct vpu_qpu_job_env_s
{
  unsigned int n;
  unsigned int mask;
  struct gpu_job_s j[VPU_QPU_JOB_MAX];
};
typedef struct vpu_qpu_job_env_s vpu_qpu_job_env_t;

vpu_qpu_job_h vpu_qpu_job_init(vpu_qpu_job_env_t * const buf);
void vpu_qpu_job_delete(const vpu_qpu_job_h vqj);
void vpu_qpu_job_add_vpu(const vpu_qpu_job_h vqj, const uint32_t vpu_code,
  const unsigned r0, const unsigned r1, const unsigned r2, const unsigned r3, const unsigned r4, const unsigned r5);
void vpu_qpu_job_add_qpu(const vpu_qpu_job_h vqj, const unsigned int n, const uint32_t * const mail);
void vpu_qpu_job_add_sync_this(const vpu_qpu_job_h vqj, vpu_qpu_wait_h * const wait_h);
int vpu_qpu_job_add_sync_sem(vpu_qpu_job_env_t * const vqj, sem_t * const sem);
int vpu_qpu_job_start(const vpu_qpu_job_h vqj);
int vpu_qpu_job_finish(const vpu_qpu_job_h vqj);

extern unsigned int vpu_get_fn(const unsigned int bit_depth);
extern unsigned int vpu_get_constants(void);

// Waits for previous post_codee to complete and Will null out *wait_h after use
void vpu_qpu_wait(vpu_qpu_wait_h * const wait_h);
int vpu_qpu_init(void);
void vpu_qpu_term(void);

void gpu_ref(void);
void gpu_unref(void);

#endif
