#define _POSIX_C_SOURCE 199309L

#include <time.h>

/* This is not threadsafe at all, but that's fine for our purposes. */

static struct timespec start;
static struct timespec end;

void
timer_start ()
{
	clock_gettime(CLOCK_MONOTONIC, &start);
}

void
timer_stop ()
{
	clock_gettime(CLOCK_MONOTONIC, &end);
}

float
timer_sec ()
{
	struct timespec temp;

	if ((end.tv_nsec - start.tv_nsec) < 0) {
		temp.tv_sec = end.tv_sec - start.tv_sec - 1;
		temp.tv_nsec = 1000000000 + end.tv_nsec - start.tv_nsec;
	} else {
		temp.tv_sec = end.tv_sec - start.tv_sec;
		temp.tv_nsec = end.tv_nsec - start.tv_nsec;
	}
	return (float)(temp.tv_sec + ((float)temp.tv_nsec / 1000000000.0));
}
