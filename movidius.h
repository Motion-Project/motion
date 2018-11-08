/*
 * movidius.h
 *
 *    Include file for movidius.c
 *
 *    Copyright 2013 by Nicholas Tuckett
 *    This software is distributed under the GNU public license version 2
 *    See also the file 'COPYING'.
 */

#ifndef MOVIDIUS_H_
#define MOVIDIUS_H_

#include <time.h>
#include <mvnc.h>


#define HAVE_MVNC_PROFILE

#define MVNC_PROFILE_AVERAGE_LENGTH     16


typedef struct mvnc_result {
    int class_id;       // index into MobileNet_labels
    float score;        // probability in percentage
    float box_left;     // object location within image, range 0.0 to 1.0
    float box_top;      // object location within image, range 0.0 to 1.0
    float box_right;    // object location within image, range 0.0 to 1.0
    float box_bottom;   // object location within image, range 0.0 to 1.0
} mvnc_result;


typedef struct mvnc_device_t {
    struct ncDeviceHandle_t* deviceHandle;
    struct ncGraphHandle_t* graphHandle;
    struct ncFifoHandle_t* inputFIFO;
    struct ncFifoHandle_t* outputFIFO;
    struct mvnc_result *results;
    int num_results;
    unsigned int thermal_buffer_size;
#ifdef HAVE_MVNC_PROFILE
    struct timespec profile_ts[MVNC_PROFILE_AVERAGE_LENGTH];
    unsigned int profile_ts_index;
#endif
} mvnc_device_t;


void mvnc_infer_image(struct mvnc_device_t *dev, unsigned char *image, int width, int height);
int mvnc_get_results(struct mvnc_device_t *dev);
unsigned mvnc_objects_detected(struct mvnc_device_t *dev, int *class_ids,
                               int num_class_ids, float score_threshold);
int mvnc_get_max_score_index(struct mvnc_device_t *dev, int *class_ids,
                             int num_class_ids, float score_threshold);
int mvnc_get_class_id_from_string(const char *label_string);
int mvnc_get_temperature_log(struct mvnc_device_t *dev, float **temperature_log,
                             unsigned *temperature_log_length);
float mvnc_get_max_temperature(struct mvnc_device_t *dev);
int mvnc_get_thermal_throttle_level(struct mvnc_device_t *dev);
const char *mvnc_get_class_label(int class_id);
void mvnc_free_results(struct mvnc_device_t *dev);

#ifdef HAVE_MVNC_PROFILE
double mvnc_profile_get_fps(struct mvnc_device_t *dev);
#endif

int mvnc_init(struct mvnc_device_t *dev, int dev_index, const char *graph_path);
void mvnc_close(struct mvnc_device_t *dev);


#endif /* MOVIDIUS_H_ */
