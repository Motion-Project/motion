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

void movidius_infer_image(unsigned char *image, int width, int height);
int movidius_get_results(float **resultData, int *numResults);
float movidius_get_person_probability(float *resultData, int numResults);
void movidius_free_results(float **resultData);

int movidius_init(void);
void movidius_close(void);


#endif /* MOVIDIUS_H_ */
