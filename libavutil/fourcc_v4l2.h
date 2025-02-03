/*
 * V4L2 format helper functions
 *
 * Copyright (C) 2017 Alexis Ballier <aballier@gentoo.org>
 * Copyright (C) 2017 Jorge Ramirez <jorge.ramirez-ortiz@linaro.org>
 *
 * This file is part of FFmpeg.
 *
 * FFmpeg is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * FFmpeg is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with FFmpeg; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

#ifndef AVUTIL_FOURCC_V4L2
#define AVUTIL_FOURCC_V4L2

#include <linux/videodev2.h>

// V4L2_PIX_FMT_NV12_10_COL128 and V4L2_PIX_FMT_NV12_COL128 should be defined
// in drm_fourcc.h hopefully will be sometime in the future but until then...
#ifndef V4L2_PIX_FMT_NV12_10_COL128
#define V4L2_PIX_FMT_NV12_10_COL128 v4l2_fourcc('N', 'C', '3', '0')
#endif

#ifndef V4L2_PIX_FMT_NV12_COL128
#define V4L2_PIX_FMT_NV12_COL128 v4l2_fourcc('N', 'C', '1', '2') /* 12  Y/CbCr 4:2:0 128 pixel wide column */
#endif

#ifndef V4L2_PIX_FMT_NV12_COL128M
#define V4L2_PIX_FMT_NV12_COL128M v4l2_fourcc('N', 'c', '1', '2') /* 12  Y/CbCr 4:2:0 128 pixel wide column */
/* Y/CbCr 4:2:0 10bpc, 3x10 packed as 4 bytes in
 * a 128 bytes / 96 pixel wide column */
#define V4L2_PIX_FMT_NV12_10_COL128M v4l2_fourcc('N', 'c', '3', '0')
#endif

#ifndef V4L2_PIX_FMT_P210
#define V4L2_PIX_FMT_P210 v4l2_fourcc('P', '2', '1', '0')
#endif
#ifndef V4L2_PIX_FMT_P212
#define V4L2_PIX_FMT_P212 v4l2_fourcc('P', '2', '1', '2')
#endif
#ifndef V4L2_PIX_FMT_P410
#define V4L2_PIX_FMT_P410 v4l2_fourcc('P', '4', '1', '0')
#endif
#ifndef V4L2_PIX_FMT_P412
#define V4L2_PIX_FMT_P412 v4l2_fourcc('P', '4', '1', '2')
#endif

#endif /* AVUTIL_FOURCC_V4L2 */
