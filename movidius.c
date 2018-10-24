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

#include "motion.h"
#include "movidius.h"


// TODO: move this into a struct
static struct ncDeviceHandle_t* deviceHandle = NULL;
static struct ncGraphHandle_t* graphHandle = NULL;
static struct ncFifoHandle_t* inputFIFO = NULL;
static struct ncFifoHandle_t* outputFIFO = NULL;
static const char *labels[] = {"background",
                               "aeroplane", "bicycle", "bird", "boat",
                               "bottle", "bus", "car", "cat", "chair",
                               "cow", "diningtable", "dog", "horse",
                               "motorbike", "person", "pottedplant",
                               "sheep", "sofa", "train", "tvmonitor"};


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
    // (because the U and V plane equal a half of Y – what is YUV?).
    // With RGB24, there is only one plane
    // data[0] = linesize[0] = width*channels = 640*3(R, G, B).

    // create scaling context
    sws_ctx = sws_getContext(src_w, src_h, src_pix_fmt,
                             dst_w, dst_h, dst_pix_fmt,
                             SWS_BICUBIC, NULL, NULL, NULL);
    if (!sws_ctx)
    {
        fprintf(stderr,
                "Impossible to create scale context for the conversion "
                "fmt:%s s:%dx%d -> fmt:%s s:%dx%d\n",
                av_get_pix_fmt_name(src_pix_fmt), src_w, src_h,
                av_get_pix_fmt_name(dst_pix_fmt), dst_w, dst_h);
        goto end;
    }
    
    int srcNumBytes = av_image_fill_arrays(src_data, src_linesize, src_img,
                                           src_pix_fmt, src_w, src_h, 1);
    // TODO: check av_image_fill_arrays return code

    int dst_bufsize;

    // buffer is float format, align to float size
    if ((dst_bufsize = av_image_alloc(dst_data, dst_linesize,
                       dst_w, dst_h, dst_pix_fmt, 1)) < 0)
    {
        fprintf(stderr, "Could not allocate destination image\n");
        goto end;
    }

    // convert to destination format
    sws_scale(sws_ctx, (const uint8_t * const*)src_data,
              src_linesize, 0, src_h, dst_data, dst_linesize);

    *processed_img_len = dst_w * dst_h * 3 * sizeof(float);
    processed_img = malloc(*processed_img_len);
    if (processed_img == NULL)
    {
        fprintf(stderr, "Failed to allocate memory\n");
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
        printf("Error [%d]: Could not get the input FIFO level.\n", retCode);
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
        printf("Error [%d]: Could not write to the input FIFO and queue an inference.\n", retCode);
    }

    if (processed_img)
        free(processed_img);

    //printf("Queued image for inference\n");
}


// returns zero if result is available, otherwise negative number.
// free resultData after use
int movidius_get_results(float **resultData, int *resultDataLength)
{
    // check read fifo level > 0 first before reading so we don't block
    ncStatus_t retCode;
    int readfifolevel = 0;
    unsigned int optionSize = sizeof(readfifolevel);
    retCode = ncFifoGetOption(outputFIFO,  NC_RO_FIFO_READ_FILL_LEVEL,
                              &readfifolevel, &optionSize);
    if (retCode != NC_OK) {
        printf("Error [%d]: Could not get the output FIFO level.\n", retCode);
        return -1;
    }

    if (readfifolevel > 0)
    {
        //printf("readfifolevel: %d\n", readfifolevel);
        // Get the size of the output tensor
        unsigned int outFifoElemSize = 0;
        optionSize = sizeof(outFifoElemSize);
        retCode = ncFifoGetOption(outputFIFO,  NC_RO_FIFO_ELEMENT_DATA_SIZE,
                                  &outFifoElemSize, &optionSize);
        if (retCode != NC_OK) {
            printf("Error [%d]: Could not get the output FIFO element data size.\n", retCode);
            return -1;
        }

        //printf("outFifoElemSize: %d\n", outFifoElemSize);

        // Get the output tensor
        *resultData = (float *)malloc(outFifoElemSize);
        if (*resultData == NULL) {
            printf("Error: Could not allocate memory for result.\n");
            return -1;
        }
        void *userParam;  // this will be set to point to the user-defined data that you passed into ncGraphQueueInferenceWithFifoElem() with this tensor
        retCode = ncFifoReadElem(outputFIFO, *resultData, &outFifoElemSize, &userParam);
        if (retCode != NC_OK) {
            printf("Error [%d]: Could not read the result from the ouput FIFO.\n", retCode);
            movidius_free_results(resultData);
            return -1;
        }

        *resultDataLength = outFifoElemSize;

        //printf("*numResults: %d\n", *numResults);

        return 0;
    }
    return -1;
}


float movidius_get_person_probability(float *resultData, int resultDataLength)
{
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

    if ((resultDataLength > 1) && (resultData))
    {
        int num_valid_detections = (int)resultData[0];
        int i;

        if (resultDataLength >= num_valid_detections*7*sizeof(float)+7*sizeof(float))
        {
            printf("Num valid detections: %d\n", num_valid_detections);
            for (i = 0; i < num_valid_detections; i++)
            {
                int base_index = 7 + i*7;
                int class_id = (int)resultData[base_index + 1];
                float score = resultData[base_index + 2]*100;
                float box_left = resultData[base_index + 3];
                float box_top = resultData[base_index + 4];
                float box_right = resultData[base_index + 5];
                float box_bottom = resultData[base_index + 6];
                printf("\tclass ID: %d\n", class_id);
                if (class_id < sizeof(labels)/sizeof(char *))
                    printf("\t%s : %f%%, (%f, %f, %f, %f)\n", labels[class_id], score, box_left, box_right, box_bottom, box_top);
            }
        }
    }

    //float maxResult = 0.0;
    //int maxIndex = -1;
    //int index;
    //
    //for (index = 0; index < numResults; index++)
    //{
    //    if (resultData[index] > maxResult)
    //    {
    //        maxResult = resultData[index];
    //        maxIndex = index;
    //    }
    //}
    //printf("Index %d: probability %f\n", maxIndex, resultData[maxIndex]);
    return 0; // FIXME: return person's probability
}


void movidius_free_results(float **resultData)
{
    if (*resultData)
    {
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
        fprintf(stderr, "Unable to open file %s", graph_path);
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
        fprintf(stderr, "Unable to allocate memory");
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
        printf("Error [%d]: Could not create a neural compute device handle.\n", retCode);
        ret = -1;
        goto cleanup;
    }
    // Boot the device and open communication
    retCode = ncDeviceOpen(deviceHandle);
    if (retCode != NC_OK) {
        printf("Error [%d]: Could not open the neural compute device.\n", retCode);
        ret = -1;
        goto cleanup;
    }
    // Load a graph from file
    if (movidius_read_graph_file(graph_path, &graph_buffer, &graph_length))
    {
        printf("Error: Failed to load graph.\n");
        ret = -1;
        goto cleanup;
    }
    // Initialize a graph handle
    retCode = ncGraphCreate("MobileNetSSD", &graphHandle);
    if (retCode != NC_OK) {
        printf("Error [%d]: Could not create a graph handle.\n", retCode);
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
        printf("Error [%d]: Could not allocate graph with FIFOs.\n", retCode);
        ret = -1;
        goto cleanup;
    }
    //ncStatus_t ncDeviceSetOption(deviceHandle,
    //                         int option, const void* data,
    //                         unsigned int dataLength);

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
