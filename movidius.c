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

#include "translate.h"
#include "motion.h"
#include "movidius.h"


static const char *MobileNet_labels[] = {"background",
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

    // create scaling context
    sws_ctx = sws_getContext(src_w, src_h, src_pix_fmt,
                             dst_w, dst_h, dst_pix_fmt,
                             SWS_BICUBIC, NULL, NULL, NULL);
    if (!sws_ctx)
    {
        MOTION_LOG(ERR, TYPE_ALL, NO_ERRNO
            ,_("Impossible to create scale context for image conversion fmt:%s s:%dx%d -> fmt:%s s:%dx%d"),
            av_get_pix_fmt_name(src_pix_fmt), src_w, src_h,
            av_get_pix_fmt_name(dst_pix_fmt), dst_w, dst_h);
        goto end;
    }
    
    int srcNumBytes = av_image_fill_arrays(src_data, src_linesize, src_img,
                                           src_pix_fmt, src_w, src_h, 1);
    if (srcNumBytes < 0)
    {
        MOTION_LOG(ERR, TYPE_ALL, NO_ERRNO
            ,_("Failed to fill image arrays: code %d"),
            srcNumBytes);
        goto end;
    }

    int dst_bufsize;
    if ((dst_bufsize = av_image_alloc(dst_data, dst_linesize,
                       dst_w, dst_h, dst_pix_fmt, 1)) < 0)
    {
        MOTION_LOG(ERR, TYPE_ALL, NO_ERRNO,_("Failed to allocate dst image"));
        goto end;
    }

    // convert to destination format
    sws_scale(sws_ctx, (const uint8_t * const*)src_data,
              src_linesize, 0, src_h, dst_data, dst_linesize);

    *processed_img_len = dst_w * dst_h * 3 * sizeof(float);
    processed_img = malloc(*processed_img_len);
    if (processed_img == NULL)
    {
        MOTION_LOG(ERR, TYPE_ALL, NO_ERRNO,_("Failed to allocate memory"));
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


void mvnc_infer_image(struct mvnc_device_t *dev, unsigned char *image, int width, int height)
{
    // Setting min_fifo_level to zero feeds the FIFO only when it is empty,
    // giving us 5.5 fps throughput.
    // Setting min_fifo_level to one ensures the NC stick is not starved from
    // data, giving us 11 fps throughput, but NC stick overheats quite quickly.
    // The hardware has thermal throttling that kicks in at 70 degrees Celsius.

    // Set to zero for now as 5.5 fps inference is enough for detecting objects.
    const int min_fifo_level = 0;
    float *processed_img = NULL;
    unsigned int processed_img_len = 0;
    ncStatus_t retCode;

    int writefifolevel = 0;
    unsigned int optionSize = sizeof(writefifolevel);
    retCode = ncFifoGetOption(dev->inputFIFO,  NC_RO_FIFO_WRITE_FILL_LEVEL,
                              &writefifolevel, &optionSize);
    if (retCode != NC_OK) {
        MOTION_LOG(ERR, TYPE_ALL, NO_ERRNO,
                   _("Error [%d]: Could not get the input FIFO level"),
                   retCode);
        return;
    }

    if (writefifolevel > min_fifo_level)
        return;

    processed_img = scale_image(image, width, height, &processed_img_len);

    // Write the tensor to the input FIFO and queue an inference
    retCode = ncGraphQueueInferenceWithFifoElem(
                dev->graphHandle, dev->inputFIFO, dev->outputFIFO, processed_img,
                &processed_img_len, NULL);
    if (retCode != NC_OK) {
        MOTION_LOG(ERR, TYPE_ALL, NO_ERRNO,
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


#ifdef HAVE_MVNC_PROFILE
static void mvnc_profile_record_ts(struct mvnc_device_t *dev)
{
    if (clock_gettime(CLOCK_MONOTONIC, &dev->profile_ts[dev->profile_ts_index]) == 0)
    {
        dev->profile_ts_index++;
        if (dev->profile_ts_index >= MVNC_PROFILE_AVERAGE_LENGTH)
            dev->profile_ts_index = 0;
    }
}


// Gets the throughput of the device in frame per second
// This function is not thread safe
double mvnc_profile_get_fps(struct mvnc_device_t *dev)
{
    struct timespec diff;
    unsigned int oldest_ts_index = dev->profile_ts_index;
    unsigned int newest_ts_index = dev->profile_ts_index - 1;
    if (newest_ts_index >= MVNC_PROFILE_AVERAGE_LENGTH)
        newest_ts_index = MVNC_PROFILE_AVERAGE_LENGTH - 1;
    if ((dev->profile_ts[newest_ts_index].tv_sec > 0) && (dev->profile_ts[oldest_ts_index].tv_sec > 0))
    {
        diff.tv_sec = dev->profile_ts[newest_ts_index].tv_sec - dev->profile_ts[oldest_ts_index].tv_sec;
        diff.tv_nsec = dev->profile_ts[newest_ts_index].tv_nsec - dev->profile_ts[oldest_ts_index].tv_nsec;
        return MVNC_PROFILE_AVERAGE_LENGTH / (diff.tv_sec + (double)diff.tv_nsec/1.0e9);
    }
    return 0;
}
#endif


// returns number of results, otherwise negative number if error.
// free results after use by calling mvnc_free_results
int mvnc_get_results(struct mvnc_device_t *dev)
{
    // check read fifo level > 0 first before reading so we don't block
    int num_valid_detections = 0;
    float *tensor_output = NULL;
    ncStatus_t retCode;
    int readfifolevel = 0;
    unsigned int optionSize = sizeof(readfifolevel);
    retCode = ncFifoGetOption(dev->outputFIFO,  NC_RO_FIFO_READ_FILL_LEVEL,
                              &readfifolevel, &optionSize);
    if (retCode != NC_OK) {
        MOTION_LOG(ERR, TYPE_ALL, NO_ERRNO,
                   _("Error [%d]: Could not get the output FIFO level"),
                   retCode);
        return -1;
    }

    if (readfifolevel > 0)
    {
#ifdef HAVE_MVNC_PROFILE
        mvnc_profile_record_ts(dev);
#endif

        mvnc_free_results(dev);

        // Get the size of the output tensor
        unsigned int outFifoElemSize = 0;
        optionSize = sizeof(outFifoElemSize);
        retCode = ncFifoGetOption(dev->outputFIFO,  NC_RO_FIFO_ELEMENT_DATA_SIZE,
                                  &outFifoElemSize, &optionSize);
        if (retCode != NC_OK) {
            MOTION_LOG(ERR, TYPE_ALL, NO_ERRNO,
                       _("Error [%d]: Could not get the output FIFO element data size"),
                       retCode);
            return -1;
        }

        // Get the output tensor
        tensor_output = (float *)malloc(outFifoElemSize);
        if (tensor_output == NULL) {
            MOTION_LOG(ERR, TYPE_ALL, NO_ERRNO,
                       _("Could not allocate memory for tensor output"));
            return -1;
        }
        void *userParam;
        retCode = ncFifoReadElem(dev->outputFIFO, tensor_output, &outFifoElemSize, &userParam);
        if (retCode != NC_OK) {
            MOTION_LOG(ERR, TYPE_ALL, NO_ERRNO,
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
                    dev->results = (struct mvnc_result *)malloc(sizeof(struct mvnc_result)*num_valid_detections);
                    if (dev->results == NULL)
                    {
                        MOTION_LOG(ERR, TYPE_ALL, NO_ERRNO,
                                   _("Could not allocate memory for result"));
                        free(tensor_output);
                        return -1;
                    }
                    dev->num_results = num_valid_detections;

                    int obj_index = 0;
                    for (i = 0; i < num_detections; i++)
                    {
                        int base_index = 7 + i*7;
                        int class_id = (int)tensor_output[base_index + 1];
                        if (class_id_valid(class_id))
                        {
                            dev->results[obj_index].class_id = class_id;
                            dev->results[obj_index].score = tensor_output[base_index + 2]*100;
                            dev->results[obj_index].box_left = tensor_output[base_index + 3];
                            dev->results[obj_index].box_top = tensor_output[base_index + 4];
                            dev->results[obj_index].box_right = tensor_output[base_index + 5];
                            dev->results[obj_index].box_bottom = tensor_output[base_index + 6];
                            clip_box_location(&dev->results[obj_index].box_left);
                            clip_box_location(&dev->results[obj_index].box_top);
                            clip_box_location(&dev->results[obj_index].box_right);
                            clip_box_location(&dev->results[obj_index].box_bottom);

                            MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,
                                _("%s : %f%%, (%f, %f, %f, %f)"),
                                mvnc_get_class_label(dev->results[obj_index].class_id),
                                dev->results[obj_index].score,
                                dev->results[obj_index].box_left,
                                dev->results[obj_index].box_right,
                                dev->results[obj_index].box_bottom,
                                dev->results[obj_index].box_top);

                            obj_index++;
                        }
                    }
                }
            }
        }

        free(tensor_output);

        return num_valid_detections;
    }
    return -1;
}


// Gets the last 25 seconds worth of temperature reading.
// returns 0 if success
// caller must free temperature_log after use
int mvnc_get_temperature_log(struct mvnc_device_t *dev, float **temperature_log,
                             unsigned *temperature_log_length)
{
    int ret = -1;
    ncStatus_t retCode;

    *temperature_log_length = 0;
    *temperature_log = malloc(dev->thermal_buffer_size);
    if (*temperature_log)
    {
        retCode = ncDeviceGetOption(dev->deviceHandle, NC_RO_DEVICE_THERMAL_STATS,
                                    *temperature_log, &dev->thermal_buffer_size);
        if (retCode != NC_OK)
        {
            MOTION_LOG(ERR, TYPE_ALL, NO_ERRNO,
                       _("Error [%d]: Failed to get thermal log"),
                       retCode);
            free(*temperature_log);
            *temperature_log = NULL;
        }
        else
        {
            *temperature_log_length = dev->thermal_buffer_size/sizeof(float);
            ret = 0;
        }
    }
    return ret;
}


// Gets the maximum temperature over the last 25 seconds.
float mvnc_get_max_temperature(struct mvnc_device_t *dev)
{
    float max_temp = -99.9;
    float *temperature_log = NULL;
    unsigned temperature_log_length = 0;
    if (mvnc_get_temperature_log(dev, &temperature_log,
                                 &temperature_log_length) == 0)
    {
        int i;
        for (i = 0; i < temperature_log_length; i++)
        {
            if (temperature_log[i] > max_temp)
                max_temp = temperature_log[i];
        }
    }
    if (temperature_log) {
        free(temperature_log);
        temperature_log = NULL;
    }
    return max_temp;
}


// Gets the thermal throttle level.
// return -1 on error, otherwise return the below possible values:
// 0: No limit reached.
// 1: Lower guard temperature threshold of chip sensor reached; short throttling
//    time is in action between inferences to protect the device.
// 2: Upper guard temperature of chip sensor reached; long throttling time is in
//    action between inferences to protect the device.
int mvnc_get_thermal_throttle_level(struct mvnc_device_t *dev)
{
    int level = -1;
    ncStatus_t retCode;
    unsigned int throttle_size = sizeof(int);

    retCode = ncDeviceGetOption(dev->deviceHandle, NC_RO_DEVICE_THERMAL_THROTTLING_LEVEL,
                                &level, &throttle_size);
    if (retCode != NC_OK)
    {
        MOTION_LOG(ERR, TYPE_ALL, NO_ERRNO,
                   _("Error [%d]: Failed to get thermal throttle level"),
                   retCode);
    }
    return level;
}


const char *mvnc_get_class_label(int class_id)
{
    if (class_id_valid(class_id))
        return MobileNet_labels[class_id];
    return "";
}


// return -1 if not found
int mvnc_get_class_id_from_string(const char *label_string)
{
    int class_id = -1;
    int i;

    for (i = 0; i < sizeof(MobileNet_labels)/sizeof(char *); i++)
    {
        if (strcmp(label_string, MobileNet_labels[i]) == 0)
        {
            class_id = i;
            break;
        }
    }

    return class_id;
}


unsigned mvnc_objects_detected(struct mvnc_device_t *dev, int *class_ids,
                               int num_class_ids, float score_threshold)
{
    int i;
    int j;

    if (dev->results)
    {
        for (i = 0; i < dev->num_results; i++)
        {
            for (j = 0; j < num_class_ids; j++)
            {
                if (dev->results[i].class_id == class_ids[j])
                {
                    if (dev->results[i].score > score_threshold)
                        return 1;
                }
            }
        }
    }
    return 0;
}


// returns -1 if no result found
int mvnc_get_max_score_index(struct mvnc_device_t *dev, int *class_ids,
                             int num_class_ids, float score_threshold)
{
    float max_score = 0;
    int max_score_index = -1;
    int i;
    int j;

    if (dev->results)
    {
        for (i = 0; i < dev->num_results; i++)
        {
            for (j = 0; j < num_class_ids; j++)
            {
                if (dev->results[i].class_id == class_ids[j])
                {
                    if ((dev->results[i].score > score_threshold) &&
                        (dev->results[i].score > max_score))
                    {
                        max_score = dev->results[i].score;
                        max_score_index = i;
                    }
                }
            }
        }
    }
    return max_score_index;
}


void mvnc_free_results(struct mvnc_device_t *dev)
{
    if (dev->results)
    {
        free(dev->results);
        dev->results = NULL;
    }
    dev->num_results = 0;
}


static int mvnc_read_graph_file(const char *graph_path, char **graph_buffer, unsigned int *graph_length)
{
    FILE *file;
    *graph_length = 0;

    //Open file
    file = fopen(graph_path, "rb");
    if (!file)
    {
        MOTION_LOG(ERR, TYPE_ALL, NO_ERRNO,
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
        MOTION_LOG(ERR, TYPE_ALL, NO_ERRNO,
                   _("Unable to allocate memory"));
        fclose(file);
        return -1;
    }

    //Read file contents into graph_buffer
    fread(*graph_buffer, *graph_length, 1, file);
    fclose(file);
    return 0;
}


int mvnc_init(struct mvnc_device_t *dev, int dev_index, const char *graph_path)
{
    char *graph_buffer = NULL;
    unsigned int graph_length = 0;
    int ret = 0;
    ncStatus_t retCode;

#ifdef HAVE_MVNC_PROFILE
    memset(dev->profile_ts, 0, sizeof(struct timespec) * MVNC_PROFILE_AVERAGE_LENGTH);
    dev->profile_ts_index = 0;
#endif

    MOTION_LOG(NTC, TYPE_ALL, NO_ERRNO,
        _("Initializing mvnc device at index %d ..."),
        dev_index);

    // Create a device handle for the device located at dev_index
    retCode = ncDeviceCreate(dev_index, &dev->deviceHandle);
    if (retCode != NC_OK) {
        MOTION_LOG(ERR, TYPE_ALL, NO_ERRNO,
                   _("Error [%d]: Could not create mvnc device handle for device index %d"),
                   retCode, dev_index);
        ret = -1;
        goto cleanup;
    }
    // Boot the device and open communication
    retCode = ncDeviceOpen(dev->deviceHandle);
    if (retCode != NC_OK) {
        MOTION_LOG(ERR, TYPE_ALL, NO_ERRNO,
                   _("Error [%d]: Could not open mvnc device at index %d"),
                   retCode, dev_index);
        ret = -1;
        goto cleanup;
    }
    // Load a graph from file
    if (mvnc_read_graph_file(graph_path, &graph_buffer, &graph_length))
    {
        MOTION_LOG(ERR, TYPE_ALL, NO_ERRNO,
                   _("Failed to read graph %s"), graph_path);
        ret = -1;
        goto cleanup;
    }
    // Initialize a graph handle
    retCode = ncGraphCreate("MobileNetSSD", &dev->graphHandle);
    if (retCode != NC_OK) {
        MOTION_LOG(ERR, TYPE_ALL, NO_ERRNO,
                   _("Error [%d]: Could not create graph handle for device index %d"),
                   retCode, dev_index);
        ret = -1;
        goto cleanup;
    }
    // Allocate the graph to the device and create input and output FIFOs with default options
    retCode = ncGraphAllocateWithFifos(dev->deviceHandle, dev->graphHandle,
                                       graph_buffer, graph_length,
                                       &dev->inputFIFO, &dev->outputFIFO);
    if (retCode != NC_OK) {
        MOTION_LOG(ERR, TYPE_ALL, NO_ERRNO,
                   _("Error [%d]: Could not allocate graph with FIFOs for device index %d"),
                   retCode, dev_index);
        ret = -1;
        goto cleanup;
    }

    dev->thermal_buffer_size = 0;
    retCode = ncDeviceGetOption(dev->deviceHandle, NC_RO_DEVICE_THERMAL_STATS,
                                NULL, &dev->thermal_buffer_size);
    if (retCode != NC_INVALID_DATA_LENGTH)
    {
        MOTION_LOG(ERR, TYPE_ALL, NO_ERRNO,
                   _("Error [%d]: Failed to get thermal buffer size"),
                   retCode);
    }
    // thermal_buffer_size should now be set to correct size
    MOTION_LOG(NTC, TYPE_ALL, NO_ERRNO,
               _("thermal_buffer_size: %d"), dev->thermal_buffer_size);

    // Success !
    MOTION_LOG(NTC, TYPE_ALL, NO_ERRNO,
        _("Initializing mvnc device at index %d: success"),
        dev_index);
    if (graph_buffer)
    {
        free(graph_buffer);
        graph_buffer = NULL;
    }
    return ret;


cleanup:
    mvnc_close(dev);
    if (graph_buffer)
    {
        free(graph_buffer);
        graph_buffer = NULL;
    }
    return ret;
}


void mvnc_close(struct mvnc_device_t *dev)
{
    if (dev->inputFIFO)
    {
        ncFifoDestroy(&dev->inputFIFO);
        dev->inputFIFO = NULL;
    }
    if (dev->outputFIFO)
    {
        ncFifoDestroy(&dev->outputFIFO);
        dev->outputFIFO = NULL;
    }
    if (dev->graphHandle)
    {
        ncGraphDestroy(&dev->graphHandle);
        dev->graphHandle = NULL;
    }
    if (dev->deviceHandle)
    {
        MOTION_LOG(NTC, TYPE_ALL, NO_ERRNO, _("Closing mvnc device ..."));
        ncDeviceClose(dev->deviceHandle);
        ncDeviceDestroy(&dev->deviceHandle);
        dev->deviceHandle = NULL;
        MOTION_LOG(NTC, TYPE_ALL, NO_ERRNO, _("Closing mvnc device: success"));
    }
}
