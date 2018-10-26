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

#include <mvnc.h>


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
} mvnc_device_t;


void mvnc_infer_image(struct mvnc_device_t *dev, unsigned char *image, int width, int height);
int mvnc_get_results(struct mvnc_device_t *dev);
unsigned mvnc_objects_detected(struct mvnc_device_t *dev, int *class_ids,
                               int num_class_ids, float score_threshold);
int mvnc_get_max_score_index(struct mvnc_device_t *dev, int *class_ids,
                             int num_class_ids, float score_threshold);
int mvnc_get_class_id_from_string(const char *label_string);
const char *mvnc_get_class_label(int class_id);
void mvnc_free_results(struct mvnc_device_t *dev);

int mvnc_init(struct mvnc_device_t *dev, int dev_index, const char *graph_path);
void mvnc_close(struct mvnc_device_t *dev);


#endif /* MOVIDIUS_H_ */
