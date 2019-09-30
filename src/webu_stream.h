#ifndef _INCLUDE_WEBU_STREAM_H_
#define _INCLUDE_WEBU_STREAM_H_

    struct webui_ctx;

    void webu_stream_init(struct ctx_cam *cam);
    void webu_stream_deinit(struct ctx_cam *cam);
    void webu_stream_getimg(struct ctx_cam *cam, struct ctx_image_data *img_data);

    int webu_stream_mjpeg(struct webui_ctx *webui);
    int webu_stream_static(struct webui_ctx *webui);

#endif
