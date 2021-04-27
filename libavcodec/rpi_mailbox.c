/*
Copyright (c) 2012, Broadcom Europe Ltd.
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
*/

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <assert.h>
#include <stdint.h>
#include <sys/ioctl.h>

#include <linux/ioctl.h>

#define MAJOR_NUM 100
#define IOCTL_MBOX_PROPERTY _IOWR(MAJOR_NUM, 0, char *)
#define DEVICE_FILE_NAME "/dev/vcio"

#include "rpi_mailbox.h"
//#include <interface/vctypes/vc_image_structs.h>

/*
 * use ioctl to send mbox property message
 */

static int mbox_property(int file_desc, void *buf)
{
   int ret_val = ioctl(file_desc, IOCTL_MBOX_PROPERTY, buf);

   if (ret_val < 0) {
      printf("ioctl_set_msg failed:%d\n", ret_val);
   }

#ifdef DEBUG
   unsigned *p = buf; int i; unsigned size = *(unsigned *)buf;
   for (i=0; i<size/4; i++)
      printf("%04x: 0x%08x\n", i*sizeof *p, p[i]);
#endif
   return ret_val;
}

#define GET_VCIMAGE_PARAMS 0x30044

int mbox_get_image_params(int fd, VC_IMAGE_T * img)
{
    uint32_t buf[sizeof(*img) / sizeof(uint32_t) + 32];
    uint32_t * p = buf;
    void * rimg;
    int rv;

    *p++ = 0; // size
    *p++ = 0; // process request
    *p++ = GET_VCIMAGE_PARAMS;
    *p++ = sizeof(*img);
    *p++ = sizeof(*img);
    rimg = p;
    memcpy(p, img, sizeof(*img));
    p += sizeof(*img) / sizeof(*p);
    *p++ = 0;  // End tag
    buf[0] = (p - buf) * sizeof(*p);

    rv = mbox_property(fd, buf);
    memcpy(img, rimg, sizeof(*img));

    return rv;
}


#define SET_CLOCK_RATE 0x00038002
#define GET_MAX_CLOCK 0x00030004
#define CLOCK_HEVC 11

static int mbox_property_generic(int fd, unsigned command, unsigned *word0, unsigned *word1)
{
    uint32_t buf[32];
    uint32_t * p = buf;
    int rv;

    *p++ = 0; // size
    *p++ = 0; // process request
    *p++ = command;
    *p++ = 8;
    *p++ = 8;
    *p++ = *word0;
    *p++ = *word1;
    *p++ = 0;  // End tag
    buf[0] = (p - buf) * sizeof(*p);

    rv = mbox_property(fd, buf);
    *word0 = buf[6];
    *word1 = buf[7];
    return rv;
}

int mbox_open() {
   int file_desc;

   // open a char device file used for communicating with kernel mbox driver
   file_desc = open(DEVICE_FILE_NAME, 0);
   if (file_desc < 0) {
      printf("Can't open device file: %s\n", DEVICE_FILE_NAME);
      printf("Try creating a device file with: sudo mknod %s c %d 0\n", DEVICE_FILE_NAME, MAJOR_NUM);
   }
   return file_desc;
}

void mbox_close(int file_desc) {
  close(file_desc);
}

int mbox_request_clock(int fd) {
   int rv;
   unsigned word0, word1 = 0;
   word0 = CLOCK_HEVC;
   rv = mbox_property_generic(fd, GET_MAX_CLOCK, &word0, &word1);
   if (rv != 0)
      return rv;
   word1 = word0;
   word0 = CLOCK_HEVC;
   rv = mbox_property_generic(fd, SET_CLOCK_RATE, &word0, &word1);
   return rv;
}

int mbox_release_clock(int fd) {
  int rv;
  unsigned word0, word1 = 0;
  word0 = CLOCK_HEVC;
  word1 = 0;
  rv = mbox_property_generic(fd, SET_CLOCK_RATE, &word0, &word1);
  return rv;
}
