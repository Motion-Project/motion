/*
 * Copyright (c) 2018 Hiroki Mori
 * Copyright (c) 2012-2014 SAITOU Toshihide
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
*/

#ifndef VIDEO_FORMAT_DESCRIPTOR
#define VIDEO_FORMAT_DESCRIPTOR



typedef struct payloadHeader {
        uint8_t bHeaderLength;
        uint8_t bmHeaderInfo;
#define UVC_STREAM_EOH  (1 << 7)
#define UVC_STREAM_ERR  (1 << 6)
#define UVC_STREAM_STI  (1 << 5)
#define UVC_STREAM_RES  (1 << 4)
#define UVC_STREAM_SCR  (1 << 3)
#define UVC_STREAM_PTS  (1 << 2)
#define UVC_STREAM_EOF  (1 << 1)
#define UVC_STREAM_FID  (1 << 0)
} payloadHeader __attribute__((aligned(sizeof(void *))));




typedef struct videoFormatDescriptor {
        uint8_t bLength;
        uint8_t bDescriptorType;
        uint8_t bDescriptorSubtype;
        uint8_t bFormatIndex;
        uint8_t bNumFrameDescriptors;
        uint8_t guidFormat[16];
        uint8_t bBitsPerPixel;
        uint8_t bDefaultFrameIndex;
        uint8_t bAspectRatioX;
        uint8_t bAspectRatioY;
        uint8_t bmInterlaceFlags;
        uint8_t bCopyProtect;
} videoFormatDescriptor __attribute__((aligned(sizeof(void *))));


typedef struct videoFrameDescriptor {
        uint8_t bLength;
        uint8_t bDescriptorType;
        uint8_t bDescriptorSubtype;
        uint8_t bFrameIndex;
        uint8_t bmCapabilities;
        uint16_t wWidth;
        uint16_t wHeight;
        uint32_t dwMinBitRate;
        uint32_t dwMaxBitRate;
        uint32_t dwMaxVideoFrameBufferSize;
        uint32_t dwDefaultFrameInterval;
        uint8_t bFrameIntervalType;
} videoFrameDescriptor __attribute__((aligned(sizeof(void *))));

int uvc_start(struct context *cnt);
int uvc_next(struct context *cnt,  struct image_data *img_data);
void uvc_cleanup(struct context *cnt);

#endif
