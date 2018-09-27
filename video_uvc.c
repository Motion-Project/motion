/*
 * Copyright (c) 2018 Hiroki Mori
 * Copyright (c) 2012-2014 SAITOU Toshihide
 * All rights reserved.
 *
 * This code based on ex41.c.
 * A sample program for LibUSB isochronous transfer using UVC cam.
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
 *
 * Reference
 *
 *   [PL_UnComp]
 *
 *     (USB_Video_Payload_Uncompressed_1.5.pdf),
 *     <http://www.usb.org/developers/devclass_docs>.
 *
 *   [UVC1.5]
 *
 *     Universal Serial Bus Device Class Definition for Video Devices, (UVC 1.5
 *     Class specification.pdf), <http://www.usb.org/developers/devclass_docs>.
 *
 *   [USB2.0]
 *
 *     USB 2.0 Specification Universal Serial Bus Revision 2.0 specification,
 *     (usb_20.pdf), <http://www.USB.org/developers/>.
 *
 */

#include "translate.h"
#include "rotate.h"     /* Already includes motion.h */
#include "video_common.h"

#include "video_uvc.h"

#if HAVE_UVC

#include <libusb.h>

typedef struct uvc_device
{
        uint16_t VID;
        uint16_t PID;
        uint8_t  ConfIdx;       /* configuration index */
        uint8_t  ConfVal;       /* configuration value */
        uint8_t  IfNum;         /* interface number */
        uint8_t  AltSetting;    /* alternate setting of the interface */
        uint8_t  Endpoint;      /* endpoint */
        uint8_t  FrameIndex;    /* FrameIndex for 640x480 */
        uint16_t PuId;          /* VC_PROCESSING_UNIT, bUnitID<<8 */
        uint16_t TermId;        /* VC_INPUT_TERMINAL, bTerminalID<<8 */
	int FrameBufferSize;    /* FrameSize * BitPerPixel */
        uint32_t PktLen;        /* dwMaxPayloadTransferSize 0xc00, 0xa80,... */
} uvc_device;

uvc_device uvc_device_list[] =
{
	/* Isochronous */

        /* MSK-1425: Microsoft, Microsoft LifeCam StudioTM  */
        { 0x045e, 0x0772, 0, 1, 1,  0, 0x81, 2, 0x0400, 0x0100 },

        /* BSW20K07HWH: iBUFFALO, BSW20K07HWH */
        { 0x0458, 0x7081, 0, 1, 1,  0, 0x82, 7, 0x0200, 0x0100 },

        /* UCAM-DLY300TA: Etron Technology, Inc., UCAM-DLY300TA */
        { 0x056e, 0x7008, 0, 1, 1,  0, 0x82, 1, 0x0200, 0x0100 },

        /* C920: Logitech Inc., LOGICOOL HD Webcam C920 */
        { 0x046d, 0x082d, 0, 1, 1, 10, 0x81, 1, 0x0300, 0x0100 },

	/* Logitech HD Webcam C270 */
        { 0x046d, 0x0825, 0, 1, 1, 10, 0x81, 1, 0x0300, 0x0100 },

        /* UCAM-MS130: Etron Technology, Inc., UCAM-MS130SV */
        { 0x056e, 0x7012, 0, 1, 1,  0, 0x81, 2, 0x0300, 0x0100 },

        /* KBCR-S01MU */
        { 0x05ca, 0x18d0, 0, 1, 1,  0, 0x82, 1, 0x0200, 0x0400 },

        /* Bulk */

        /* ESCH021: e-con System Pvt. Ltd., See3CAM_10CUG_CH */
        { 0x2560, 0xc111, 0, 1, 1,  0, 0x83, 1, 0x0200, 0x0100 },

        /* ESMH156: e-con System Pvt. Ltd., See3CAM_10CUG_MH */
        { 0x2560, 0xc110, 0, 1, 1,  0, 0x83, 1, 0x0200, 0x0100 },

        { 0, 0, 0, 0, 0, 0, 0 }
};


#define PKTS_PER_XFER  0x40
#define NUM_TRANSFER   2

#define TIMEOUT 500             /* 500 ms */

/* boolean */
#define TRUE  1
#define FALSE 0

uvc_device uvc;

uint8_t *padding;               /* padding data */
int finish;
int CaptStat;                   /* Capture 0 = no, 1 = next, 2 = now, 3 = done, 4 = end */
pthread_t thread;

libusb_context *ctx = NULL;
libusb_device_handle *handle;
struct libusb_transfer *xfers[NUM_TRANSFER];
int total;

static void
uvc_ctrl()
{
        uint8_t buf[2];
        int16_t brightness;

        /* 
         * BSW20K07HWH
         *   min: 0xff81, max: 0x0080, res: 0x0001
         * UCAM-DLY300TA
         *   min: 0x000a, max: 0xfff6, res: 0x0001
         */

        /* 
         * PU_BRIGHTNESS_CONTROL(0x02), GET_MIN(0x82) [UVC1.5, p. 160,
         * 158, 96]
         */
        libusb_control_transfer(
                handle, 0xa1, 0x82, 0x0200, uvc.PuId, buf, 2, TIMEOUT);
        MOTION_LOG(NTC, TYPE_VIDEO, NO_ERRNO,
                "brightness min: %02x%02x\n", buf[1], buf[0]);

        /*
         * PU_BRIGHTNESS_CONTROL(0x02), GET_MAX(0x83) [UVC1.5, p. 160,
         * 158, 96]
         */
        libusb_control_transfer(
                handle, 0xa1, 0x83, 0x0200, uvc.PuId, buf, 2, TIMEOUT);
        MOTION_LOG(NTC, TYPE_VIDEO, NO_ERRNO,
                "brightness max: %02x%02x\n", buf[1], buf[0]);

        /*
         * PU_BRIGHTNESS_CONTROL(0x02), GET_RES(0x84) [UVC1.5, p. 160,
         * 158, 96]
         */
        libusb_control_transfer(
                handle, 0xa1, 0x84, 0x0200, uvc.PuId, buf, 2, TIMEOUT);
        MOTION_LOG(NTC, TYPE_VIDEO, NO_ERRNO,
                "brightness res: %02x%02x\n", buf[1], buf[0]);

        /*
         * PU_BRIGHTNESS_CONTROL(0x02), GET_CUR(0x81) [UVC1.5, p. 160,
         * 158, 96]
         */
        libusb_control_transfer(
                handle, 0xa1, 0x81, 0x0200, uvc.PuId, buf, 2, TIMEOUT);
        MOTION_LOG(NTC, TYPE_VIDEO, NO_ERRNO,
                "brightness cur: %02x%02x\n", buf[1], buf[0]);

        /* change brightness */
        brightness = buf[1]<<8 | buf[0];
//      brightness += 30;
        brightness += 1;
        buf[1] = brightness<<8;
        buf[0] = brightness & 0xff;

        /*
         * PU_BRIGHTNESS_CONTROL(0x02), SET_CUR(0x01) [UVC1.5, p. 160,
         * 158, 96]
         */
        libusb_control_transfer(
                handle, 0x21, 0x01, 0x0200, uvc.PuId, buf, 2, TIMEOUT);

        /*
         * PU_BRIGHTNESS_CONTROL(0x02), GET_CUR(0x81) [UVC1.5, p. 160,
         * 158, 96]
         */
        libusb_control_transfer(
                handle, 0xa1, 0x81, 0x0200, uvc.PuId, buf, 2, TIMEOUT);
        MOTION_LOG(NTC, TYPE_VIDEO, NO_ERRNO,
                "brightness: %02x%02x\n", buf[1], buf[0]);
}

static void
uvc_forcus()
{
        uint8_t buf[2];

        /*
         * CT_FOCUS_AUTO_CONTROL(0x08), GET_CUR(0x81) [UVC1.5, p. 160,
         * 159, 86]
         */
        libusb_control_transfer(
                handle, 0xa1, 0x81, 0x0800, uvc.TermId, buf, 1, TIMEOUT);
        buf[0] = !buf[0];

        /*
         * CT_FOCUS_AUTO_CONTROL(0x08), SET_CUR(0x01) [UVC1.5, p. 160,
         * 159, 86]
         */
        libusb_control_transfer(
                handle, 0x21, 0x01, 0x0800, uvc.TermId, buf, 1, TIMEOUT);
        MOTION_LOG(NTC, TYPE_VIDEO, NO_ERRNO,
                "auto focus control: %02x\n", buf[0]);

}

static void cb(struct libusb_transfer *xfer)
{
        uint8_t *p;
        int plen;
        int i;

        p = xfer->buffer;

        for (i = 0; i < xfer->num_iso_packets; i++, p += uvc.PktLen)
        {
                if (xfer->iso_packet_desc[i].status != LIBUSB_TRANSFER_COMPLETED)
                        continue;

                plen = xfer->iso_packet_desc[i].actual_length;

                /* packet only contains an acknowledge? */
                if (plen < 2)
                        continue;
                if (finish == 1)
                        break;

                /* error packet */
                if (p[1] & UVC_STREAM_ERR) /* bmHeaderInfo */
                        continue;

                /* subtract the header size */
                plen -= p[0];

                /* check the data size before write */
                if (plen + total > uvc.FrameBufferSize)
                {
#if DEBUG
                        MOTION_LOG(NTC, TYPE_VIDEO, NO_ERRNO,
                                "truncate the excess payload length.\n");
#endif
                        plen = uvc.FrameBufferSize - total;
                }
//                write(fd, p + p[0], plen);

                /* update padding data */
		if (CaptStat == 2)
                        memcpy(padding + total, p + p[0], plen);

                total += plen;

                /* this is the EOF data. */
                if (p[1] & UVC_STREAM_EOF)
                {
                        if (total < uvc.FrameBufferSize)
                        {
                                /*
                                 * insufficient frame data, so pad with the
                                 * previous frame data.
                                 */
//                                write(fd, padding + total,
//                                      uvc.FrameBufferSize - total);
                                MOTION_LOG(NTC, TYPE_VIDEO, NO_ERRNO,
                                        "insufficient frame data.\n");
                        }

			if (CaptStat == 1) {
                                CaptStat = 2;
			} else if(CaptStat == 2) {
			        if(total == uvc.FrameBufferSize)
                                        CaptStat = 3;
			}

                        total = 0;
                }
        }

        /* re-submit a transfer before returning. */
        if (libusb_submit_transfer(xfer) != 0)
        {
                MOTION_LOG(NTC, TYPE_VIDEO, NO_ERRNO,
                        "submit transfer failed.\n");
        }
}

void *thread_func(void *param)
{
        while (CaptStat != 4)
                libusb_handle_events(ctx);
}

#endif

void uvc_cleanup(struct context *cnt)
{
#if HAVE_UVC
        int i;
        for (i=0; i<NUM_TRANSFER; i++) {
                libusb_free_transfer(xfers[i]);
	}

	CaptStat = 4;
	pthread_join(thread, NULL);

        libusb_set_interface_alt_setting(handle, 1, 0);

        libusb_exit(ctx);
#else
        if (!cnt) MOTION_LOG(DBG, TYPE_VIDEO, NO_ERRNO,_("UVC is not enabled."));
#endif
}

int uvc_start(struct context *cnt)
{
#if HAVE_UVC
        libusb_device **devs;
        libusb_device *dev;
        const struct libusb_interface *intf;
        struct libusb_device_descriptor desc;
        struct libusb_config_descriptor *confDesc;
        struct libusb_interface_descriptor *intfDesc;
        struct libusb_endpoint_descriptor *ep;

        int i;
        int j;
        int k;
        int foundIt;
        int maxPktSize;
        int altSetting;
        uint8_t *buf = (void *)malloc(1024);
        int frameIndex;
        int width;
        int height;
        int XferType;                   /* isochronous / bulk */
        int BitPerPixel;                /* 8: 1 byte/pixel, 16: 2 byte/pxel */
        int FrameSize;

        /*
         * get cam device.
         */

        libusb_init(&ctx);
#if DEBUG
        libusb_set_debug(ctx, 1);
#endif
        libusb_get_device_list(ctx, &devs);

        foundIt = FALSE;
        i = 0;
        while ((dev = devs[i++]) != NULL)
        {
                libusb_get_device_descriptor(dev, &desc);
                for (j = 0; uvc_device_list[j].VID != 0; j++)
                {
                        uvc = uvc_device_list[j];
                        if (uvc.VID == desc.idVendor &&
                            uvc.PID == desc.idProduct)
                        {
                                foundIt = TRUE;
                                goto FOUND;
                        }
                }
        }
FOUND:
        if (!foundIt)
        {
                MOTION_LOG(ERR, TYPE_VIDEO, SHOW_ERRNO,
                        "device not found.\n");
                return -1;
        }

        libusb_open(dev, &handle);
        libusb_free_device_list(devs, 1);


        /*
         * if the kernel driver is active, detach it.
         */

        libusb_get_config_descriptor(dev, uvc.ConfIdx, &confDesc);
        for (i=0; i<confDesc->bNumInterfaces; i++)
        {
                if (libusb_kernel_driver_active(handle, i) == 1)
                {
                        MOTION_LOG(ERR, TYPE_VIDEO, SHOW_ERRNO,
                                "detaching kernel driver for interface %d.\n", i);

                        if (libusb_detach_kernel_driver(handle, i) != 0)
                        {
                                MOTION_LOG(ERR, TYPE_VIDEO, SHOW_ERRNO,
                                        "detach failed.\n");
                        }
                }
        }


        /*
         * search user-specified bFrameIndex which usually located at interface 1.
         */

        foundIt = FALSE;
        for (i = 1; i < confDesc->bNumInterfaces; i++) /* seek interfaces */
        {
                intf = (void *) &confDesc->interface[i];
                intfDesc = (void *) &intf->altsetting[0]; /* interface i alt 0 */
                /* CC_VIDEO && SC_VIDEOSTREAMING */
                if (intfDesc->bInterfaceClass == 0x0e
                    && intfDesc->bInterfaceSubClass == 0x02)
                {
                        uvc.IfNum = i;
                        buf = (void *) intfDesc->extra;
                        foundIt = TRUE;
                        break;
                }
        }

        if (!foundIt)
        {
                MOTION_LOG(ERR, TYPE_VIDEO, SHOW_ERRNO,
                        "no SC_VIDEOSTREAMING.\n");
                return -1;
        }

        for (i = 0; i < intfDesc->extra_length;)
        {
                /* CS_INTERFACE && VS_FRAME_UNCOMPRESSED */
                if (buf[i+1] == 0x24 && buf[i+2] == 0x05)
                {
                        width = (buf[i+6]<<8) | buf[i+5];
                        height = (buf[i+8]<<8) | buf[i+7];
                        MOTION_LOG(NTC, TYPE_VIDEO, NO_ERRNO,
                                "%d: %dx%d\n", buf[i+3], width, height);
                        if (cnt->conf.width == width && cnt->conf.height)
                        {
				frameIndex = buf[i+3];
                                foundIt = TRUE;
                                break;
                        }
                }
                i += buf[i+0];
        }

        if (!foundIt)
        {
                MOTION_LOG(ERR, TYPE_VIDEO, SHOW_ERRNO,
                        "Can't find the frame index.\n");
                return -1;
        }


        /*
         * search VS_FORMAT_UNCOMPRESSED and see what bBitsPerPixel is.
         */

        foundIt = FALSE;
        for (i = 0; i < intfDesc->extra_length;)
        {
                /* CS_INTERFACE && VS_FORMAT_UNCOMPRESSED */
                if (buf[i+1] == 0x24 && buf[i+2] == 0x04)
                {
                        BitPerPixel = buf[i+11];
                        foundIt = TRUE;
                        break;
                }
                i += buf[i+0];
        }

        if (!foundIt)
        { 
                MOTION_LOG(ERR, TYPE_VIDEO, SHOW_ERRNO,
                        "no VS_FORMAT_UNCOMPRESSED.\n");
		return -1;
        }

        if (BitPerPixel != 16)
        { 
                MOTION_LOG(ERR, TYPE_VIDEO, SHOW_ERRNO,
                        "only support 16 bit yuv \n");
		return -1;
        }


        /*
         * search payload transfer endpoint.
         */

        foundIt = FALSE;
        maxPktSize = 0;
        altSetting = 0;
        for (i = 1; i < confDesc->bNumInterfaces; i++) /* seek interfaces */
        {
                intf = (void *) &confDesc->interface[i];
                for (j = 0; j < intf->num_altsetting; j++) /* seek alt settings */
                {
                        intfDesc = (void *) &intf->altsetting[j];
                        /* CC_VIDEO && SC_VIDEOSTREAMING */
                        if (intfDesc->bInterfaceClass == 0x0e
                            && intfDesc->bInterfaceSubClass == 0x02
                            && intfDesc->bNumEndpoints != 0)
                        {
                                for (k = 0; k < intfDesc->bNumEndpoints; k++)
                                {
                                        ep = (void *) &intfDesc->endpoint[k];
                                        if (ep->wMaxPacketSize > maxPktSize)
                                        {
                                                maxPktSize = ep->wMaxPacketSize;
                                                altSetting = j;
                                                foundIt = TRUE;
                                        }
                                }
                        }
                }
        }

        if (!foundIt)
        {
                MOTION_LOG(ERR, TYPE_VIDEO, SHOW_ERRNO,
                        "Can't find the appropriate endpoint.\n");
		return -1;
        }

        if (uvc.AltSetting == 0)
                uvc.AltSetting = altSetting;

        if (ep->bmAttributes == 0x05) /* isochronous ? */
                XferType = LIBUSB_TRANSFER_TYPE_ISOCHRONOUS;
        else
                XferType = LIBUSB_TRANSFER_TYPE_BULK;
        MOTION_LOG(NTC, TYPE_VIDEO, NO_ERRNO,
                "XferType: %x\n", XferType);

        uvc.FrameIndex = frameIndex;
        FrameSize = width * height;

        uvc.FrameBufferSize = 2*FrameSize;

        padding = (void *) malloc(uvc.FrameBufferSize); 

        libusb_free_config_descriptor(confDesc);


        /* set the active configuration */
        if (libusb_set_configuration(handle, uvc.ConfVal) != 0)
                MOTION_LOG(ERR, TYPE_VIDEO, SHOW_ERRNO,
                        "set configuration failed.\n");

        /* claim an interface in a given libusb_handle. */
        if (libusb_claim_interface(handle, 0) != 0)
                MOTION_LOG(ERR, TYPE_VIDEO, SHOW_ERRNO,
                        "claim interface failed.\n");

        cnt->imgs.width = width;
        cnt->imgs.height = height;

        cnt->vdev = mymalloc(sizeof(struct vdev_context));
        memset(cnt->vdev, 0, sizeof(struct vdev_context));
        cnt->vdev->usrctrl_array = NULL;
	cnt->vdev->usrctrl_count = 0;
	cnt->vdev->update_parms = TRUE;     /*Set trigger that we have updated user parameters */

        cnt->imgs.size_norm = (width * height * 3) / 2;
        cnt->imgs.motionsize = width * height;


        /*
         * negotiate the streaming parameters
         *
         * Device State Transition [UVC1.5, p. 107]
         * Video Probe and Commit Controls [UVC1.5, p. 134]
         *
         *  [0] what fields shall be kept fixed
         *  [1]
         *  [2] Video format index (Uncompressed)
         *  [3] Video frame index
         *  [4] Frame interval in 100ns
         *  [5]
         *  [6]
         *  [7]
         *  [8] Key frame rate in key-frame per videoframe units 
         *  [9]
         * [10] PFrame rate in PFrame/key frame units. 
         * [11] 
         * [12] Compression quality control in abstract units 1 to 10000. 
         * [13]
         * [14] Window size for average bit rate control. 
         * [15]
         * [16] Internal video streaming interface latency in ms. 
         * [17]
         * [18] Max. video frame or codec-specific segment size in bytes (ro) 
         * [19]  (dwMaxVideoFrameSize) 
         * [20]
         * [21]
         * [22] Specifies the maximum number of bytes (ro) 
         * [23] (dwMaxPayloadTransferSize) 
         * [24] 
         * [25]
         */
        buf[0] = 0x01;
        buf[1] = 0x00;
        buf[2] = 0x01;
        buf[3] = uvc.FrameIndex;
        buf[4] = 0x15;          /* propose:   0x00051615 (33ms) */
        buf[5] = 0x16;
        buf[6] = 0x05;
        buf[7] = 0x00;
        buf[8] = 0x00;
        buf[9] = 0x00;
        buf[10] = 0x00;
        buf[11] = 0x00;
        buf[12] = 0x00;
        buf[13] = 0x00;
        buf[14] = 0x00;
        buf[15] = 0x00;
        buf[16] = 0x00;
        buf[17] = 0x00;
        buf[18] = 0x00;
        buf[19] = 0x00;
        buf[20] = 0x00;
        buf[21] = 0x00;
        buf[22] = 0x00;
        buf[23] = 0x00;
        buf[24] = 0x00;
        buf[25] = 0x00;

        /* VS_PROBE_CONTROL(0x01), SET_CUR(0x01)  [UVC1.5, p. 161, 158] */
        libusb_control_transfer(
                handle, 0x21, 0x01, 0x0100, 0x0001, buf, 26, TIMEOUT);

        /* VS_PROBE_CONTROL(0x01), GET_MIN(0x82)  [UVC1.5, p. 161, 158] */
        libusb_control_transfer(
                handle, 0xa1, 0x82, 0x0100, 0x0001, buf, 26, TIMEOUT);

        /* VS_COMMIT_CONTROL(0x02), SET_CUR(0x01) [UVC1.5, p. 161, 158] */
        libusb_control_transfer(
                handle, 0x21, 0x01, 0x0200, 0x0001, buf, 26, TIMEOUT);

#if DEBUG
        for (i = 0; i < 26; i++)
                fprintf(stderr, "%02x ", buf[i]);
        fprintf(stderr, "\n");
#endif

        /* uvc.PktLen */
        uvc.PktLen = (buf[25]<<24 | buf[24]<<16 | buf[23]<<8 | buf[22]);
//        fprintf(stderr, "dwMaxPayloadTransferSize: %08x\n", uvc.PktLen);

        /*
         * set interface, set alt interface [USB2.0, p. 250]
         */

        /* claim an interface in a given libusb_handle. */
        if (libusb_claim_interface(handle, uvc.IfNum) != 0)
                MOTION_LOG(ERR, TYPE_VIDEO, SHOW_ERRNO,
                        "claim interface failed.\n");

        /* activate an alternate setting for an interface. */
        if (uvc.AltSetting != 0)
                if (libusb_set_interface_alt_setting(
                        handle, uvc.IfNum, uvc.AltSetting) != 0)
                        MOTION_LOG(ERR, TYPE_VIDEO, SHOW_ERRNO,
                                "activate an alternate setting failed.\n");


        /*
         * do an isochronous / bulk transfer
         */

	uint8_t *data[NUM_TRANSFER];
        for (i=0; i<NUM_TRANSFER; i++)
        {
                xfers[i] = libusb_alloc_transfer(PKTS_PER_XFER);
                data[i] = malloc(uvc.PktLen*PKTS_PER_XFER);

                libusb_fill_iso_transfer(
                        xfers[i], handle, uvc.Endpoint,
                        data[i], uvc.PktLen*PKTS_PER_XFER, PKTS_PER_XFER,
                        cb, NULL, 0);

                libusb_set_iso_packet_lengths(xfers[i], uvc.PktLen);
        }

        for (i=0; i<NUM_TRANSFER; i++)
                if (libusb_submit_transfer(xfers[i]) != 0)
                        MOTION_LOG(ERR, TYPE_VIDEO, SHOW_ERRNO,
                                "submit xfer failed.\n");

        CaptStat = 0;

	total = 0;

        pthread_create(&thread, NULL, thread_func, NULL);

#else
        if (!cnt) MOTION_LOG(DBG, TYPE_VIDEO, NO_ERRNO,_("UVC is not enabled."));
#endif
	return 1;
}

int uvc_next(struct context *cnt,  struct image_data *img_data)
{
#if HAVE_UVC

	CaptStat = 1;
        while (CaptStat != 3) 
                SLEEP(0,500000000L);

	CaptStat = 0;
	vid_yuv422to420p(img_data->image_norm, padding,
                cnt->imgs.width, cnt->imgs.height);

#else
        if (!cnt || !img_data) MOTION_LOG(DBG, TYPE_VIDEO, NO_ERRNO,_("UVC is not enabled."));
#endif

	return 0;
}
