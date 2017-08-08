#include <stdlib.h>

#include "motion.h"
#include "alg.h"

#if __APPLE__

#include <mach/mach_time.h>

static mach_timebase_info_data_t sTimebaseInfo;

#define TS_INIT() \
    do { \
        if (!sTimebaseInfo.numer) mach_timebase_info(&sTimebaseInfo); \
    } while(0)

#define TS_MARK(cycles) \
    do { \
        cycles = mach_absolute_time(); \
    } while(0)

#define TS_CONVERT(cycles, us_time) \
    do { \
        us_time = (cycles) * sTimebaseInfo.numer / sTimebaseInfo.denom / 1000;\
    } while(0)

#else

#include <time.h>

#define TS_INIT() \
    do { \
    } while(0)

#define TS_MARK(cycles) \
    do { \
        struct timeval tv; \
        gettimeofday(&tv, NULL); \
        cycles = (uint64_t)tv.tv_sec * 1000000 + tv.tv_usec; \
    } while(0)

#define TS_CONVERT(cycles, us_time) \
    do { \
        us_time = cycles; \
    } while(0)

#endif


void alg_locate_center_size(struct images *imgs, int width, int height, struct coord *cent);
void alg_locate_center_size_c(struct images *imgs, int width, int height, struct coord *cent);

void motion_log(int level, unsigned int type, int errno_flag, const char *fmt, ...) {
}

int draw_text(unsigned char *image, unsigned int startx, unsigned int starty, unsigned int width, const char *text, unsigned int factor) {
    return 0;
}

void test_locate_center_size(int width, int height, unsigned char * out, int *lables) {
    width -= 4;
    height -= 2;
    struct images img;
    img.width = width;
    img.height = height;
    img.labelsize_max = 1;
    img.out = out;
    img.labels = lables;

    struct coord cent, cent_ref;
    alg_locate_center_size_c(&img, width, height, &cent_ref);
    alg_locate_center_size(&img, width, height, &cent);
    uint64_t start, first, second;
    TS_MARK(start);
    alg_locate_center_size_c(&img, width, height, &cent_ref);
    TS_MARK(first);
    alg_locate_center_size(&img, width, height, &cent);
    TS_MARK(second);

    assert(cent.x == cent_ref.x);
    assert(cent.y == cent_ref.y);
    assert(cent.maxx == cent_ref.maxx);
    assert(cent.maxy == cent_ref.maxy);
    assert(cent.minx == cent_ref.minx);
    assert(cent.miny == cent_ref.miny);

    int64_t time1, time2;
    TS_CONVERT(first - start, time1);
    TS_CONVERT(second - first, time2);
    printf("Test %s passed, %lld vs %lld (%lld) %lf%%\n", __FUNCTION__, time1, time2, time1 - time2, (double)time2/(double)time1*100.0);

    {
        img.labelsize_max = 0;
        alg_locate_center_size_c(&img, width, height, &cent_ref);
        alg_locate_center_size(&img, width, height, &cent);
        TS_MARK(start);
        alg_locate_center_size_c(&img, width, height, &cent_ref);
        TS_MARK(first);
        alg_locate_center_size(&img, width, height, &cent);
        TS_MARK(second);

        assert(cent.x == cent_ref.x);
        assert(cent.y == cent_ref.y);
        assert(cent.maxx == cent_ref.maxx);
        assert(cent.maxy == cent_ref.maxy);
        assert(cent.minx == cent_ref.minx);
        assert(cent.miny == cent_ref.miny);

        TS_CONVERT(first - start, time1);
        TS_CONVERT(second - first, time2);
        printf("Test %s passed, %lld vs %lld (%lld) %lf%%\n", __FUNCTION__, time1, time2, time1 - time2, (double)time2/(double)time1*100.0);
    }
}

#define test_width 1280
#define test_height 720
static unsigned char test_img_data[test_width * test_height];
static int test_lables[test_width * test_height];

int main(int argc, const char* argv[]) {
    TS_INIT();

    // filling test data
    for (int i = 0; i < test_height * test_width; i++) {
        test_img_data[i] = rand();
        test_lables[i] = rand();
    }

    test_locate_center_size(test_width, test_height, test_img_data, test_lables);
    test_locate_center_size(test_width - 2, test_height - 2, test_img_data, test_lables);

    return 0;
}
