
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

#endif
