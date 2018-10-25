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


typedef struct movidius_result {
    int class_id;       // index into MobileNet_labels
    float score;        // probability in percentage
    float box_left;     // object location within image, range 0.0 to 1.0
    float box_top;      // object location within image, range 0.0 to 1.0
    float box_right;    // object location within image, range 0.0 to 1.0
    float box_bottom;   // object location within image, range 0.0 to 1.0
} movidius_result;


typedef struct movidius_output {
    struct movidius_result *object;
    int num_objects;
} movidius_output;



void movidius_infer_image(unsigned char *image, int width, int height);
int movidius_get_results(movidius_output **resultData);
unsigned movidius_person_detected(movidius_output *resultData, float score_threshold);
int movidius_get_max_person_index(movidius_output *resultData, float score_threshold);
const char *movidius_get_class_label(int class_id);
void movidius_free_results(movidius_output **resultData);

int movidius_init(void);
void movidius_close(void);


#endif /* MOVIDIUS_H_ */
