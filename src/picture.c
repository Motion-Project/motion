/*    picture.c
 *
 *    Various funtions for saving/loading pictures.
 *    Copyright 2002 by Jeroen Vreeken (pe1rxq@amsat.org)
 *    Portions of this file are Copyright by Lionnel Maugis
 *    Portions of this file are Copyright 2010 by Wim Lewis (wiml@hhhh.org)
 *    This software is distributed under the GNU public license version 2
 *    See also the file 'COPYING'.
 *
 */
#include "motion.h"
#include "logger.h"
#include "util.h"
#include "picture.h"
#include "jpegutils.h"
#include "event.h"
#include "exif.h"
#include "draw.h"

#ifdef HAVE_WEBP
    #include <webp/encode.h>
    #include <webp/mux.h>
#endif /* HAVE_WEBP */



#ifdef HAVE_WEBP
/*
 * pic_webp_exif writes the EXIF APP1 chunk to the webp file.
 * It must be called after WebPEncode() and the result
 * can then be written out to webp a file
 */
static void pic_webp_exif(WebPMux* webp_mux,
              const struct ctx_cam *cam,
              const struct timespec *ts1,
              const struct ctx_coord *box)
{
    unsigned char *exif = NULL;
    unsigned exif_len = exif_prepare(&exif, cam, ts1, box);

    if(exif_len > 0) {
        WebPData webp_exif;
        /* EXIF in WEBP does not need the EXIF marker signature (6 bytes) that are needed by jpeg */
        webp_exif.bytes = exif + 6;
        webp_exif.size = exif_len - 6;

        WebPMuxError err = WebPMuxSetChunk(webp_mux, "EXIF", &webp_exif, 1);
        if (err != WEBP_MUX_OK) {
            MOTION_LOG(ERR, TYPE_CORE, NO_ERRNO
                , _("Unable to set set EXIF to webp chunk"));
        }
        free(exif);
    }
}
#endif /* HAVE_WEBP */



/** Save image as webp to file */
static void pic_save_webp(FILE *fp, unsigned char *image, int width, int height,
                  int quality, struct ctx_cam *cam, struct timespec *ts1, struct ctx_coord *box)
{
    #ifdef HAVE_WEBP
        /* Create a config present and check for compatible library version */
        WebPConfig webp_config;
        if (!WebPConfigPreset(&webp_config, WEBP_PRESET_DEFAULT, (float) quality)){
            MOTION_LOG(ERR, TYPE_CORE, NO_ERRNO, _("libwebp version error"));
            return;
        }

        /* Create the input data structure and check for compatible library version */
        WebPPicture webp_image;
        if (!WebPPictureInit(&webp_image)){
            MOTION_LOG(ERR, TYPE_CORE, NO_ERRNO,_("libwebp version error"));
            return;
        }

        /* Allocate the image buffer based on image width and height */
        webp_image.width = width;
        webp_image.height = height;
        if (!WebPPictureAlloc(&webp_image)){
            MOTION_LOG(ERR, TYPE_CORE, NO_ERRNO,_("libwebp image buffer allocation error"));
            return;
        }

        /* Map the input YUV420P buffer as individual Y, U and V pointers */
        webp_image.y = image;
        webp_image.u = image + width * height;
        webp_image.v = webp_image.u + (width * height) / 4;

        /* Setup the memory writting method */
        WebPMemoryWriter webp_writer;
        WebPMemoryWriterInit(&webp_writer);
        webp_image.writer = WebPMemoryWrite;
        webp_image.custom_ptr = (void*) &webp_writer;

        /* Encode the YUV image as webp */
        if (!WebPEncode(&webp_config, &webp_image))
            MOTION_LOG(WRN, TYPE_CORE, NO_ERRNO,_("libwebp image compression error"));

        /* A bitstream object is needed for the muxing proces */
        WebPData webp_bitstream;
        webp_bitstream.bytes = webp_writer.mem;
        webp_bitstream.size = webp_writer.size;

        /* Create a mux from the prepared image data */
        WebPMux* webp_mux = WebPMuxCreate(&webp_bitstream, 1);
        pic_webp_exif(webp_mux, cam, ts1, box);

        /* Add Exif data to the webp image data */
        WebPData webp_output;
        WebPMuxError err = WebPMuxAssemble(webp_mux, &webp_output);
        if (err != WEBP_MUX_OK) {
            MOTION_LOG(ERR, TYPE_CORE, NO_ERRNO,_("unable to assemble webp image"));
        }

        /* Write the webp final bitstream to the file */
        if (fwrite(webp_output.bytes, sizeof(uint8_t), webp_output.size, fp) != webp_output.size)
            MOTION_LOG(ERR, TYPE_CORE, NO_ERRNO,_("unable to save webp image to file"));

        #if WEBP_ENCODER_ABI_VERSION > 0x0202
            /* writer.mem must be freed by calling WebPMemoryWriterClear */
            WebPMemoryWriterClear(&webp_writer);
        #else
            /* writer.mem must be freed by calling 'free(writer.mem)' */
            free(webp_writer.mem);
        #endif /* WEBP_ENCODER_ABI_VERSION */

        /* free the memory used by webp for image data */
        WebPPictureFree(&webp_image);
        /* free the memory used by webp mux object */
        WebPMuxDelete(webp_mux);
        /* free the memory used by webp for output data */
        WebPDataClear(&webp_output);
    #else
        (void)fp;
        (void)image;
        (void)width;
        (void)height;
        (void) quality;
        (void)cam;
        (void)ts1;
        (void)box;
    #endif /* HAVE_WEBP */
}


/** Save image as yuv420p jpeg to file */
static void pic_save_yuv420p(FILE *fp, unsigned char *image, int width, int height,
                  int quality, struct ctx_cam *cam, struct timespec *ts1, struct ctx_coord *box) {

    int sz = 0;
    int image_size = cam->imgs.size_norm;
    unsigned char *buf = mymalloc(image_size);

    sz = jpgutl_put_yuv420p(buf, image_size, image, width, height, quality, cam ,ts1, box);
    fwrite(buf, sz, 1, fp);

    free(buf);

}

/** Save image as grey jpeg to file */
static void pic_save_grey(FILE *picture, unsigned char *image, int width, int height,
                  int quality, struct ctx_cam *cam, struct timespec *ts1, struct ctx_coord *box) {

    int sz = 0;
    int image_size = cam->imgs.size_norm;
    unsigned char *buf = mymalloc(image_size);

    sz = jpgutl_put_grey(buf, image_size, image, width, height, quality, cam ,ts1, box);
    fwrite(buf, sz, 1, picture);

    free(buf);
}

/** Save image as greyscale ppm image to file */
static void pic_save_ppm(FILE *picture, unsigned char *image, int width, int height) {
    int x, y;
    unsigned char *l = image;
    unsigned char *u = image + width * height;
    unsigned char *v = u + (width * height) / 4;
    int r, g, b;
    unsigned char rgb[3];

    /*
     *  ppm header
     *  width height
     *  maxval
     */
    fprintf(picture, "P6\n");
    fprintf(picture, "%d %d\n", width, height);
    fprintf(picture, "%d\n", 255);
    for (y = 0; y < height; y++) {

        for (x = 0; x < width; x++) {
            r = 76283 * (((int)*l) - 16)+104595*(((int)*u) - 128);
            g = 76283 * (((int)*l) - 16)- 53281*(((int)*u) - 128) - 25625 * (((int)*v) - 128);
            b = 76283 * (((int)*l) - 16) + 132252 * (((int)*v) - 128);
            r = r >> 16;
            g = g >> 16;
            b = b >> 16;
            if (r < 0)
                r = 0;
            else if (r > 255)
                r = 255;
            if (g < 0)
                g = 0;
            else if (g > 255)
                g = 255;
            if (b < 0)
                b = 0;
            else if (b > 255)
                b = 255;

            rgb[0] = b;
            rgb[1] = g;
            rgb[2] = r;

            l++;
            if (x%2 != 0) {
                u++;
                v++;
            }
            /* ppm is rgb not bgr */
            fwrite(rgb, 1, 3, picture);
        }
        if (y%2 == 0) {
            u -= width / 2;
            v -= width / 2;
        }
    }
}


/** Put picture into memory as jpg */
int pic_put_memory(struct ctx_cam *cam, unsigned char* dest_image, int image_size, unsigned char *image,
        int quality, int width, int height) {

    struct timespec ts1;

    clock_gettime(CLOCK_REALTIME, &ts1);
    if (!cam->conf.stream_grey){
        return jpgutl_put_yuv420p(dest_image, image_size, image,
                                width, height, quality, cam ,&ts1, NULL);
    } else {
        return jpgutl_put_grey(dest_image, image_size, image,
                                width, height, quality, cam,&ts1, NULL);
    }

    return 0;
}

/* Write the picture to a file */
static void pic_write(struct ctx_cam *cam, FILE *picture, unsigned char *image, int quality, int ftype){

    int width, height;
    int passthrough;

    passthrough = mycheck_passthrough(cam);
    if ((ftype == FTYPE_IMAGE) && (cam->imgs.size_high > 0) && (!passthrough)) {
        width = cam->imgs.width_high;
        height = cam->imgs.height_high;
    } else {
        width = cam->imgs.width;
        height = cam->imgs.height;
    }

    if (mystreq(cam->conf.picture_type, "ppm")) {
        pic_save_ppm(picture, image, width, height);
    } else if (mystreq(cam->conf.picture_type, "webp")) {
        pic_save_webp(picture, image, width, height, quality, cam, &(cam->current_image->imgts), &(cam->current_image->location));
    } else if (mystreq(cam->conf.picture_type, "grey")) {
        pic_save_grey(picture, image, width, height, quality, cam, &(cam->current_image->imgts), &(cam->current_image->location));
    } else {
        pic_save_yuv420p(picture, image, width, height, quality, cam, &(cam->current_image->imgts), &(cam->current_image->location));
    }
}

/* Saves image to a file in format requested */
void pic_save_norm(struct ctx_cam *cam, char *file, unsigned char *image, int ftype) {
    FILE *picture;

    picture = myfopen(file, "w");
    if (!picture) {
        /* Report to syslog - suggest solution if the problem is access rights to target dir. */
        if (errno ==  EACCES) {
            MOTION_LOG(ERR, TYPE_ALL, SHOW_ERRNO
                ,_("Can't write picture to file %s - check access rights to target directory\n"
                "Thread is going to finish due to this fatal error"), file);
            cam->finish_cam = 1;
            cam->restart_cam = 0;
            return;
        } else {
            /* If target dir is temporarily unavailable we may survive. */
            MOTION_LOG(ERR, TYPE_ALL, SHOW_ERRNO
                ,_("Can't write picture to file %s"), file);
            return;
        }
    }

    pic_write(cam, picture, image, cam->conf.picture_quality, ftype);

    myfclose(picture);
}

/** Get the pgm file used as fixed mask */
unsigned char *pic_load_pgm(FILE *picture, int width, int height) {

    int x, y, mask_width, mask_height, maxval;
    char line[256];
    unsigned char *image, *resized_image;

    line[255] = 0;

    if (!fgets(line, 255, picture)) {
        MOTION_LOG(ERR, TYPE_ALL, SHOW_ERRNO,_("Could not read from pgm file"));
        return NULL;
    }

    if (strncmp(line, "P5", 2)) {
        MOTION_LOG(ERR, TYPE_ALL, SHOW_ERRNO
            ,_("This is not a pgm file, starts with '%s'"), line);
        return NULL;
    }

    /* Skip comment */
    line[0] = '#';
    while (line[0] == '#')
        if (!fgets(line, 255, picture))
            return NULL;

    /* Read image size */
    if (sscanf(line, "%d %d", &mask_width, &mask_height) != 2) {
        MOTION_LOG(ERR, TYPE_ALL, SHOW_ERRNO
            ,_("Failed reading size in pgm file"));
        return NULL;
    }

    /* Maximum value */
    line[0] = '#';
    while (line[0] == '#')
        if (!fgets(line, 255, picture))
            return NULL;

    if (sscanf(line, "%d", &maxval) != 1) {
        MOTION_LOG(ERR, TYPE_ALL, SHOW_ERRNO
            ,_("Failed reading maximum value in pgm file"));
        return NULL;
    }

    /* Read data */
    /* We allocate the size for a 420P since we will use
    ** this image for masking privacy which needs the space for
    ** the cr / cb components
    */
    image = mymalloc((mask_width * mask_height * 3) / 2);

    for (y = 0; y < mask_height; y++) {
        if ((int)fread(&image[y * mask_width], 1, mask_width, picture) != mask_width)
            MOTION_LOG(ERR, TYPE_ALL, SHOW_ERRNO, "Failed reading image data from pgm file");

        for (x = 0; x < mask_width; x++)
            image[y * mask_width + x] = (int)image[y * mask_width + x] * 255 / maxval;

    }

    /* Resize mask if required */
    if (mask_width != width || mask_height != height) {
        MOTION_LOG(WRN, TYPE_ALL, NO_ERRNO
            ,_("The mask file specified is not the same size as image from camera."));
        MOTION_LOG(WRN, TYPE_ALL, NO_ERRNO
            ,_("Attempting to resize mask image from %dx%d to %dx%d")
            ,mask_width, mask_height, width, height);

        resized_image = mymalloc((width * height * 3) / 2);

        for (y = 0; y < height; y++) {
            for (x = 0; x < width; x++) {
                resized_image[y * width + x] = image[
                        (mask_height - 1) * y / (height - 1) * mask_width +
                        (mask_width  - 1) * x / (width  - 1)];
            }
        }

        free(image);
        image = resized_image;
    }

    return image;
}

/** Write out a base mask file if needed */
static void pic_write_mask(struct ctx_cam *cam, const char *file) {
    FILE *picture;

    picture = myfopen(file, "w");
    if (!picture) {
        /* Report to syslog - suggest solution if the problem is access rights to target dir. */
        if (errno ==  EACCES) {
            MOTION_LOG(ERR, TYPE_ALL, SHOW_ERRNO
                ,_("can't write mask file %s - check access rights to target directory")
                ,file);
        } else {
            /* If target dir is temporarily unavailable we may survive. */
            MOTION_LOG(ERR, TYPE_ALL, SHOW_ERRNO
                ,_("can't write mask file %s"), file);
        }
        return;
    }
    memset(cam->imgs.image_motion.image_norm, 255, cam->imgs.motionsize); /* Initialize to unset */

    /* Write pgm-header. */
    fprintf(picture, "P5\n");
    fprintf(picture, "%d %d\n", cam->conf.width, cam->conf.height);
    fprintf(picture, "%d\n", 255);

    /* Write pgm image data at once. */
    if ((int)fwrite(cam->imgs.image_motion.image_norm, cam->conf.width, cam->conf.height, picture) != cam->conf.height) {
        MOTION_LOG(ERR, TYPE_ALL, SHOW_ERRNO
            ,_("Failed writing default mask as pgm file"));
        return;
    }

    myfclose(picture);

    MOTION_LOG(ERR, TYPE_ALL, NO_ERRNO
        ,_("Creating empty mask %s\nPlease edit this file and "
        "re-run motion to enable mask feature"), cam->conf.mask_file);
}

void pic_scale_img(int width_src, int height_src, unsigned char *img_src, unsigned char *img_dst){

    int i = 0, x, y;
    for (y = 0; y < height_src; y+=2)
        for (x = 0; x < width_src; x+=2)
            img_dst[i++] = img_src[y * width_src + x];

    for (y = 0; y < height_src / 2; y+=2)
       for (x = 0; x < width_src; x += 4)
       {
          img_dst[i++] = img_src[(width_src * height_src) + (y * width_src) + x];
          img_dst[i++] = img_src[(width_src * height_src) + (y * width_src) + (x + 1)];
       }

    return;
}

void pic_save_preview(struct ctx_cam *cam, struct ctx_image_data *img) {
    void *image_norm, *image_high;

    /* Save our pointers to our memory locations for images*/
    image_norm = cam->imgs.image_preview.image_norm;
    image_high = cam->imgs.image_preview.image_high;

    /* Copy over the meta data from the img into preview */
    memcpy(&cam->imgs.image_preview, img, sizeof(struct ctx_image_data));

    /* Restore the pointers to the memory locations for images*/
    cam->imgs.image_preview.image_norm = image_norm;
    cam->imgs.image_preview.image_high = image_high;

    /* Copy the actual images for norm and high */
    memcpy(cam->imgs.image_preview.image_norm, img->image_norm, cam->imgs.size_norm);
    if (cam->imgs.size_high > 0){
        memcpy(cam->imgs.image_preview.image_high, img->image_high, cam->imgs.size_high);
    }

    /*
     * If we set output_all to yes and during the event
     * there is no image with motion, diffs is 0, we are not going to save the preview event
     */
    if (cam->imgs.image_preview.diffs == 0)
        cam->imgs.image_preview.diffs = 1;

    draw_locate_preview(cam, img);

}

void pic_init_privacy(struct ctx_cam *cam){

    int indxrow, indxcol;
    int start_cr, offset_cb, start_cb;
    int y_index, uv_index;
    int indx_img, indx_max;         /* Counter and max for norm/high */
    int indx_width, indx_height;
    unsigned char *img_temp, *img_temp_uv;


    FILE *picture;

    /* Load the privacy file if any */
    cam->imgs.mask_privacy = NULL;
    cam->imgs.mask_privacy_uv = NULL;
    cam->imgs.mask_privacy_high = NULL;
    cam->imgs.mask_privacy_high_uv = NULL;

    if (cam->conf.mask_privacy) {
        if ((picture = myfopen(cam->conf.mask_privacy, "r"))) {
            MOTION_LOG(INF, TYPE_ALL, NO_ERRNO, _("Opening privacy mask file"));
            /*
             * NOTE: The mask is expected to have the output dimensions. I.e., the mask
             * applies to the already rotated image, not the capture image. Thus, use
             * width and height from imgs.
             */
            cam->imgs.mask_privacy = pic_load_pgm(picture, cam->imgs.width, cam->imgs.height);

            /* We only need the "or" mask for the U & V chrominance area.  */
            cam->imgs.mask_privacy_uv = mymalloc((cam->imgs.height * cam->imgs.width) / 2);
            if (cam->imgs.size_high > 0){
                MOTION_LOG(INF, TYPE_ALL, NO_ERRNO
                    ,_("Opening high resolution privacy mask file"));
                rewind(picture);
                cam->imgs.mask_privacy_high = pic_load_pgm(picture, cam->imgs.width_high, cam->imgs.height_high);
                cam->imgs.mask_privacy_high_uv = mymalloc((cam->imgs.height_high * cam->imgs.width_high) / 2);
            }

            myfclose(picture);
        } else {
            MOTION_LOG(ERR, TYPE_ALL, SHOW_ERRNO
                ,_("Error opening mask file %s"), cam->conf.mask_privacy);
            /* Try to write an empty mask file to make it easier for the user to edit it */
            pic_write_mask(cam, cam->conf.mask_privacy);
        }

        if (!cam->imgs.mask_privacy) {
            MOTION_LOG(ERR, TYPE_ALL, NO_ERRNO
                ,_("Failed to read mask privacy image. Mask privacy feature disabled."));
        } else {
            MOTION_LOG(INF, TYPE_ALL, NO_ERRNO
            ,_("Mask privacy file \"%s\" loaded."), cam->conf.mask_privacy);

            indx_img = 1;
            indx_max = 1;
            if (cam->imgs.size_high > 0) indx_max = 2;

            while (indx_img <= indx_max){
                if (indx_img == 1){
                    start_cr = (cam->imgs.height * cam->imgs.width);
                    offset_cb = ((cam->imgs.height * cam->imgs.width)/4);
                    start_cb = start_cr + offset_cb;
                    indx_width = cam->imgs.width;
                    indx_height = cam->imgs.height;
                    img_temp = cam->imgs.mask_privacy;
                    img_temp_uv = cam->imgs.mask_privacy_uv;
                } else {
                    start_cr = (cam->imgs.height_high * cam->imgs.width_high);
                    offset_cb = ((cam->imgs.height_high * cam->imgs.width_high)/4);
                    start_cb = start_cr + offset_cb;
                    indx_width = cam->imgs.width_high;
                    indx_height = cam->imgs.height_high;
                    img_temp = cam->imgs.mask_privacy_high;
                    img_temp_uv = cam->imgs.mask_privacy_high_uv;
                }

                for (indxrow = 0; indxrow < indx_height; indxrow++) {
                    for (indxcol = 0; indxcol < indx_width; indxcol++) {
                        y_index = indxcol + (indxrow * indx_width);
                        if (img_temp[y_index] == 0xff) {
                            if ((indxcol % 2 == 0) && (indxrow % 2 == 0) ){
                                uv_index = (indxcol/2) + ((indxrow * indx_width)/4);
                                img_temp[start_cr + uv_index] = 0xff;
                                img_temp[start_cb + uv_index] = 0xff;
                                img_temp_uv[uv_index] = 0x00;
                                img_temp_uv[offset_cb + uv_index] = 0x00;
                            }
                        } else {
                            img_temp[y_index] = 0x00;
                            if ((indxcol % 2 == 0) && (indxrow % 2 == 0) ){
                                uv_index = (indxcol/2) + ((indxrow * indx_width)/4);
                                img_temp[start_cr + uv_index] = 0x00;
                                img_temp[start_cb + uv_index] = 0x00;
                                img_temp_uv[uv_index] = 0x80;
                                img_temp_uv[offset_cb + uv_index] = 0x80;
                            }
                        }
                    }
                }
                indx_img++;
            }
        }
    }

}

void pic_init_mask(struct ctx_cam *cam){

    FILE *picture;

    /* Load the mask file if any */
    if (cam->conf.mask_file) {
        if ((picture = myfopen(cam->conf.mask_file, "r"))) {
            /*
             * NOTE: The mask is expected to have the output dimensions. I.e., the mask
             * applies to the already rotated image, not the capture image. Thus, use
             * width and height from imgs.
             */
            cam->imgs.mask = pic_load_pgm(picture, cam->imgs.width, cam->imgs.height);
            myfclose(picture);
        } else {
            MOTION_LOG(ERR, TYPE_ALL, SHOW_ERRNO
                ,_("Error opening mask file %s")
                ,cam->conf.mask_file);
            /*
             * Try to write an empty mask file to make it easier
             * for the user to edit it
             */
            pic_write_mask(cam, cam->conf.mask_file);
        }

        if (!cam->imgs.mask) {
            MOTION_LOG(ERR, TYPE_ALL, NO_ERRNO
                ,_("Failed to read mask image. Mask feature disabled."));
        } else {
            MOTION_LOG(INF, TYPE_ALL, NO_ERRNO
                ,_("Maskfile \"%s\" loaded.")
                ,cam->conf.mask_file);
        }
    } else {
        cam->imgs.mask = NULL;
    }
}
