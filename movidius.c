/*
 * movidius.c
 *
 *    Neural network based person detection using Movidius.
 *
 *    Copyright 2018 by Joo Aun Saw
 *    This software is distributed under the GNU public license version 2
 *    See also the file 'COPYING'.
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <libavutil/imgutils.h>
#include <libavutil/parseutils.h>
#include <libswscale/swscale.h>
#include <mvnc.h>

#include "translate.h"
#include "motion.h"
#include "movidius.h"


static const char *MobileNet_labels[] = {"background",
                                         "aeroplane", "bicycle", "bird", "boat",
                                         "bottle", "bus", "car", "cat", "chair",
                                         "cow", "diningtable", "dog", "horse",
                                         "motorbike", "person", "pottedplant",
                                         "sheep", "sofa", "train", "tvmonitor"};

// TODO: move this into a struct
static struct ncDeviceHandle_t* deviceHandle = NULL;
static struct ncGraphHandle_t* graphHandle = NULL;
static struct ncFifoHandle_t* inputFIFO = NULL;
static struct ncFifoHandle_t* outputFIFO = NULL;


static float *scale_image(unsigned char *src_img, int width, int height, unsigned int *processed_img_len)
{
    uint8_t *src_data[4] = {0};
    uint8_t *dst_data[4] = {0};
    int src_linesize[4] = {0};
    int dst_linesize[4] = {0};
    int src_w = width;
    int src_h = height;
    int dst_w = 300;
    int dst_h = 300;
    enum AVPixelFormat src_pix_fmt = AV_PIX_FMT_YUV420P;
    enum AVPixelFormat dst_pix_fmt = AV_PIX_FMT_BGR24;
    struct SwsContext *sws_ctx = NULL;
    float *processed_img = NULL;
    int i;

    // linesize is size in bytes for each picture line.
    // It contains stride(Image Stride) for the i-th plane
    // For example, for frame with 640*480 which has format is YUV420P (planar).
    // It contains pointer to Y plane, data[1] and data[2] contains pointers to
    // U and V plans.
    // In this case linesize[0] = 640, linesize[1] = linesize[2] = 640:2 = 320
    // (because the U and V plane equal a half of Y � what is YUV?).
    // With RGB24, there is only one plane
    // data[0] = linesize[0] = width*channels = 640*3(R, G, B).

    // create scaling context
    sws_ctx = sws_getContext(src_w, src_h, src_pix_fmt,
                             dst_w, dst_h, dst_pix_fmt,
                             SWS_BICUBIC, NULL, NULL, NULL);
    if (!sws_ctx)
    {
        MOTION_LOG(ERR, TYPE_VIDEO, NO_ERRNO
            ,_("Impossible to create scale context for image conversion fmt:%s s:%dx%d -> fmt:%s s:%dx%d"),
            av_get_pix_fmt_name(src_pix_fmt), src_w, src_h,
            av_get_pix_fmt_name(dst_pix_fmt), dst_w, dst_h);
        goto end;
    }
    
    int srcNumBytes = av_image_fill_arrays(src_data, src_linesize, src_img,
                                           src_pix_fmt, src_w, src_h, 1);
    if (srcNumBytes < 0)
    {
        MOTION_LOG(ERR, TYPE_VIDEO, NO_ERRNO
            ,_("Failed to fill image arrays: code %d"),
            srcNumBytes);
        goto end;
    }

    int dst_bufsize;
    // buffer is float format, align to float size
    if ((dst_bufsize = av_image_alloc(dst_data, dst_linesize,
                       dst_w, dst_h, dst_pix_fmt, 1)) < 0)
    {
        MOTION_LOG(ERR, TYPE_VIDEO, NO_ERRNO,_("Failed to allocate dst image"));
        goto end;
    }

    // convert to destination format
    sws_scale(sws_ctx, (const uint8_t * const*)src_data,
              src_linesize, 0, src_h, dst_data, dst_linesize);

    *processed_img_len = dst_w * dst_h * 3 * sizeof(float);
    processed_img = malloc(*processed_img_len);
    if (processed_img == NULL)
    {
        MOTION_LOG(ERR, TYPE_VIDEO, NO_ERRNO,_("Failed to allocate memory"));
        goto end;
    }
    // scale BGR range from 0-255 to -1.0 to 1.0
    for (i = 0; i < dst_w * dst_h * 3; i++)
    {
        float value = dst_data[0][i];
        processed_img[i] = (value - 127.5) * 0.007843;
    }


    //FILE *wrfile;

    //wrfile = fopen("/tmp/yuv420p.raw", "wb");
    //fwrite(src_img, 1, srcNumBytes, wrfile);
    //fclose(wrfile);

    // use gnuplot to show scaled image
    // gnuplot> plot 'scaled_bgr24.raw' binary array=(300,300) flipy format='%uchar%uchar%uchar' using 3:2:1 with rgbimage
    //wrfile = fopen("/tmp/scaled_bgr24.raw", "wb");
    //fwrite(dst_data[0], 1, dst_bufsize, wrfile);
    //fclose(wrfile);

    // gnuplot> plot 'scaled_bgr_float.raw' binary array=(300,300) flipy format='%float%float%float' using ($3*127.5+127.5):($2*127.5+127.5):($1*127.5+127.5) with rgbimage
    //wrfile = fopen("/tmp/scaled_bgr_float.raw", "wb");
    //fwrite(processed_img, *processed_img_len, 1, wrfile);
    //fclose(wrfile);

end:
    av_freep(&dst_data[0]);
    if (sws_ctx)
        sws_freeContext(sws_ctx);
    return processed_img;
}


void movidius_infer_image(unsigned char *image, int width, int height)
{
    float *processed_img = NULL;
    unsigned int processed_img_len = 0;
    ncStatus_t retCode;

    // check write fifo level == 0 before writing new image
    int writefifolevel = 0;
    unsigned int optionSize = sizeof(writefifolevel);
    retCode = ncFifoGetOption(inputFIFO,  NC_RO_FIFO_WRITE_FILL_LEVEL,
                              &writefifolevel, &optionSize);
    if (retCode != NC_OK) {
        MOTION_LOG(ERR, TYPE_VIDEO, NO_ERRNO,
                   _("Error [%d]: Could not get the input FIFO level"),
                   retCode);
        return;
    }

    if (writefifolevel > 0)
        return;

    processed_img = scale_image(image, width, height, &processed_img_len);

    // Write the tensor to the input FIFO and queue an inference
    retCode = ncGraphQueueInferenceWithFifoElem(
                graphHandle, inputFIFO, outputFIFO, processed_img,
                &processed_img_len, NULL);
    if (retCode != NC_OK) {
        MOTION_LOG(ERR, TYPE_VIDEO, NO_ERRNO,
                   _("Error [%d]: Could not write to the input FIFO and queue an inference"),
                   retCode);
    }

    if (processed_img)
        free(processed_img);
}


static unsigned class_id_valid(int class_id)
{
    return ((class_id >= 0) && (class_id < sizeof(MobileNet_labels)/sizeof(char *)));
}


static inline void clip_box_location(float *box_loc)
{
    if (*box_loc < 0.0)
        *box_loc = 0.0;
    if (*box_loc > 1.0)
        *box_loc = 1.0;
}


// returns zero if result is available, otherwise negative number.
// free resultData after use
int movidius_get_results(movidius_output **resultData)
{
    // check read fifo level > 0 first before reading so we don't block
    float *tensor_output = NULL;
    ncStatus_t retCode;
    int readfifolevel = 0;
    unsigned int optionSize = sizeof(readfifolevel);
    retCode = ncFifoGetOption(outputFIFO,  NC_RO_FIFO_READ_FILL_LEVEL,
                              &readfifolevel, &optionSize);
    if (retCode != NC_OK) {
        MOTION_LOG(ERR, TYPE_VIDEO, NO_ERRNO,
                   _("Error [%d]: Could not get the output FIFO level"),
                   retCode);
        return -1;
    }

    if (readfifolevel > 0)
    {
        movidius_free_results(resultData);

        // Get the size of the output tensor
        unsigned int outFifoElemSize = 0;
        optionSize = sizeof(outFifoElemSize);
        retCode = ncFifoGetOption(outputFIFO,  NC_RO_FIFO_ELEMENT_DATA_SIZE,
                                  &outFifoElemSize, &optionSize);
        if (retCode != NC_OK) {
            MOTION_LOG(ERR, TYPE_VIDEO, NO_ERRNO,
                       _("Error [%d]: Could not get the output FIFO element data size"),
                       retCode);
            return -1;
        }

        // Get the output tensor
        tensor_output = (float *)malloc(outFifoElemSize);
        if (tensor_output == NULL) {
            MOTION_LOG(ERR, TYPE_VIDEO, NO_ERRNO,
                       _("Could not allocate memory for tensor output"));
            return -1;
        }
        void *userParam;  // this will be set to point to the user-defined data that you passed into ncGraphQueueInferenceWithFifoElem() with this tensor
        retCode = ncFifoReadElem(outputFIFO, tensor_output, &outFifoElemSize, &userParam);
        if (retCode != NC_OK) {
            MOTION_LOG(ERR, TYPE_VIDEO, NO_ERRNO,
                       _("Error [%d]: Could not read the result from the ouput FIFO"),
                       retCode);
            free(tensor_output);
            return -1;
        }

        //   a.	First fp16 value holds the number of valid detections = num_valid.
        //   b.	The next 6 values are unused.
        //   c.	The next (7 * num_valid) values contain the valid detections data
        //       Each group of 7 values will describe an object/box These 7 values in order.
        //       The values are:
        //         0: image_id (always 0)
        //         1: class_id (this is an index into labels)
        //         2: score (this is the probability for the class)
        //         3: box left location within image as number between 0.0 and 1.0
        //         4: box top location within image as number between 0.0 and 1.0
        //         5: box right location within image as number between 0.0 and 1.0
        //         6: box bottom location within image as number between 0.0 and 1.0

        if ((outFifoElemSize > 1) && (tensor_output))
        {
            int num_detections = (int)tensor_output[0];
            int num_valid_detections = 0;
            int i;

            if (outFifoElemSize >= num_detections*7*sizeof(float)+7*sizeof(float))
            {
                for (i = 0; i < num_detections; i++)
                {
                    int base_index = 7 + i*7;
                    int class_id = (int)tensor_output[base_index + 1];
                    if (class_id_valid(class_id))
                        num_valid_detections++;
                }
                if (num_valid_detections > 0)
                {
                    *resultData = (struct movidius_output *)malloc(sizeof(struct movidius_output));
                    if (*resultData == NULL)
                    {
                        MOTION_LOG(ERR, TYPE_VIDEO, NO_ERRNO,
                                   _("Could not allocate memory for result"));
                        free(tensor_output);
                        return -1;
                    }
                    struct movidius_result *obj = (struct movidius_result *)malloc(sizeof(struct movidius_result)*num_valid_detections);
                    if (obj == NULL)
                    {
                        MOTION_LOG(ERR, TYPE_VIDEO, NO_ERRNO,
                                   _("Could not allocate memory for result"));
                        free(*resultData);
                        *resultData = NULL;
                        free(tensor_output);
                        return -1;
                    }
                    (*resultData)->object = obj;
                    (*resultData)->num_objects = num_valid_detections;

                    int obj_index = 0;
                    for (i = 0; i < num_detections; i++)
                    {
                        int base_index = 7 + i*7;
                        int class_id = (int)tensor_output[base_index + 1];
                        if (class_id_valid(class_id))
                        {
                            obj[obj_index].class_id = class_id;
                            obj[obj_index].score = tensor_output[base_index + 2]*100;
                            obj[obj_index].box_left = tensor_output[base_index + 3];
                            obj[obj_index].box_top = tensor_output[base_index + 4];
                            obj[obj_index].box_right = tensor_output[base_index + 5];
                            obj[obj_index].box_bottom = tensor_output[base_index + 6];
                            clip_box_location(&obj[obj_index].box_left);
                            clip_box_location(&obj[obj_index].box_top);
                            clip_box_location(&obj[obj_index].box_right);
                            clip_box_location(&obj[obj_index].box_bottom);

                            MOTION_LOG(NTC, TYPE_VIDEO, NO_ERRNO,
                                _("%s : %f%%, (%f, %f, %f, %f)"),
                                movidius_get_class_label(obj[obj_index].class_id), obj[obj_index].score,
                                obj[obj_index].box_left, obj[obj_index].box_right,
                                obj[obj_index].box_bottom, obj[obj_index].box_top);

                            //printf("%s : %f%%, (%f, %f, %f, %f)\n",
                            //       movidius_get_class_label(obj[obj_index].class_id), obj[obj_index].score,
                            //       obj[obj_index].box_left, obj[obj_index].box_right,
                            //       obj[obj_index].box_bottom, obj[obj_index].box_top);

                            obj_index++;
                        }
                    }
                }
            }
        }

        free(tensor_output);

        return 0;
    }
    return -1;
}


const char *movidius_get_class_label(int class_id)
{
    if (class_id_valid(class_id))
        return MobileNet_labels[class_id];
    return "";
}


unsigned movidius_person_detected(movidius_output *resultData, float score_threshold)
{
    int person_class_id = 15;
    int i;

    if (resultData)
    {
        for (i = 0; i < resultData->num_objects; i++)
        {
            if (resultData->object[i].class_id == person_class_id)
            {
                if (resultData->object[i].score > score_threshold)
                    return 1;
            }
        }
    }
    return 0;
}


void movidius_free_results(movidius_output **resultData)
{
    if (*resultData)
    {
        if ((*resultData)->object)
        {
            free((*resultData)->object);
            (*resultData)->object = NULL;
        }
        free(*resultData);
        *resultData = NULL;
    }
}


static int movidius_read_graph_file(char *graph_path, char **graph_buffer, unsigned int *graph_length)
{
    FILE *file;
    *graph_length = 0;

    //Open file
    file = fopen(graph_path, "rb");
    if (!file)
    {
        MOTION_LOG(ERR, TYPE_VIDEO, NO_ERRNO,
                   _("Unable to open graph file %s"), graph_path);
        return -1;
    }

    //Get file length
    fseek(file, 0, SEEK_END);
    *graph_length = ftell(file);
    fseek(file, 0, SEEK_SET);

    //Allocate memory
    *graph_buffer = (char *)malloc(*graph_length + 1);
    if (*graph_buffer == NULL)
    {
        MOTION_LOG(ERR, TYPE_VIDEO, NO_ERRNO,
                   _("Unable to allocate memory"));
        fclose(file);
        return -1;
    }

    //Read file contents into graph_buffer
    fread(*graph_buffer, *graph_length, 1, file);
    fclose(file);
    return 0;
}


int movidius_init(void)
{
    char *graph_path = "/home/pi/motion/MobileNetSSD.graph";
    char *graph_buffer = NULL;
    unsigned int graph_length = 0;
    int ret = 0;
    ncStatus_t retCode;

    // Create a device handle for the first device found (index 0)
    retCode = ncDeviceCreate(0, &deviceHandle);
    if (retCode != NC_OK) {
        MOTION_LOG(ERR, TYPE_VIDEO, NO_ERRNO,
                   _("Error [%d]: Could not create a neural compute device handle"),
                   retCode);
        ret = -1;
        goto cleanup;
    }
    // Boot the device and open communication
    retCode = ncDeviceOpen(deviceHandle);
    if (retCode != NC_OK) {
        MOTION_LOG(ERR, TYPE_VIDEO, NO_ERRNO,
                   _("Error [%d]: Could not open the neural compute device"),
                   retCode);
        ret = -1;
        goto cleanup;
    }
    // Load a graph from file
    if (movidius_read_graph_file(graph_path, &graph_buffer, &graph_length))
    {
        MOTION_LOG(ERR, TYPE_VIDEO, NO_ERRNO,
                   _("Failed to load graph"));
        ret = -1;
        goto cleanup;
    }
    // Initialize a graph handle
    retCode = ncGraphCreate("MobileNetSSD", &graphHandle);
    if (retCode != NC_OK) {
        MOTION_LOG(ERR, TYPE_VIDEO, NO_ERRNO,
                   _("Error [%d]: Could not create a graph handle"),
                   retCode);
        ret = -1;
        goto cleanup;
    }

    //printf("*** deviceHandle 0x%x, graphHandle 0x%x, graph_buffer 0x%x, graph_length %d\n",
    //       (unsigned int)deviceHandle, (unsigned int)graphHandle,
    //       (unsigned int)graph_buffer, graph_length);

    // Allocate the graph to the device and create input and output FIFOs with default options
    retCode = ncGraphAllocateWithFifos(deviceHandle, graphHandle, graph_buffer,
                                       graph_length, &inputFIFO, &outputFIFO);
    if (retCode != NC_OK) {
        MOTION_LOG(ERR, TYPE_VIDEO, NO_ERRNO,
                   _("Error [%d]: Could not allocate graph with FIFOs"),
                   retCode);
        ret = -1;
        goto cleanup;
    }

    // Success !
    if (graph_buffer)
    {
        free(graph_buffer);
        graph_buffer = NULL;
    }
    return ret;


cleanup:
    movidius_close();
    if (graph_buffer)
    {
        free(graph_buffer);
        graph_buffer = NULL;
    }
    return ret;
}


void movidius_close(void)
{
    if (inputFIFO)
    {
        ncFifoDestroy(&inputFIFO);
        inputFIFO = NULL;
    }
    if (outputFIFO)
    {
        ncFifoDestroy(&outputFIFO);
        outputFIFO = NULL;
    }
    if (graphHandle)
    {
        ncGraphDestroy(&graphHandle);
        graphHandle = NULL;
    }
    if (deviceHandle)
    {
        ncDeviceClose(deviceHandle);
        ncDeviceDestroy(&deviceHandle);
        deviceHandle = NULL;
    }
}
