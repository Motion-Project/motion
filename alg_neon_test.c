#include <stdlib.h>

#include "motion.h"
#include "alg.h"
#include <inttypes.h>

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


#define test_width 1280
#define test_height 720

void alg_locate_center_size(struct images *imgs, int width, int height, struct coord *cent);
void alg_locate_center_size_c(struct images *imgs, int width, int height, struct coord *cent);

int alg_diff_standard(struct context *cnt, unsigned char *new);
int alg_diff_standard_c(struct context *cnt, unsigned char *new);

void alg_update_reference_frame(struct context *cnt, int action);
void alg_update_reference_frame_c(struct context *cnt, int action);

void motion_log(int level, unsigned int type, int errno_flag, const char *fmt, ...) {
}

int draw_text(unsigned char *image, unsigned int startx, unsigned int starty, unsigned int width, const char *text, unsigned int factor) {
    return 0;
}

void test_locate_center_size(int width, int height, unsigned char * out, int * lables) {
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
    printf("Test %s passed, %" PRIu64 " vs %" PRIu64 " (%" PRIu64 ") %lf%%\n", __FUNCTION__, time1, time2, time1 - time2, (double)time2/(double)time1*100.0);

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
        printf("Test %s passed, %" PRIu64 " vs %" PRIu64 " (%" PRId64 ") %lf%%\n", __FUNCTION__, time1, time2, time1 - time2, (double)time2/(double)time1*100.0);
    }
}

static void test_alg_diff_standart_one_case(struct context *cnt, unsigned char *new, int N)
{
    static unsigned char out[test_width * test_height * 3 / 2];
    static unsigned char out_ref[test_width * test_height * 3 / 2];

    static int smartmask_buffer[test_width * test_height];
    static int smartmask_buffer_ref[test_width * test_height];

    int diffs, diffs_ref;

    uint64_t first_start, first_end, second_start, second_end;

    struct images *imgs = &cnt->imgs;
    imgs->out = memset(out_ref, 0XD7, sizeof(out_ref));
    imgs->smartmask_buffer = memset(smartmask_buffer_ref, 0xCA, sizeof(smartmask_buffer_ref));
    TS_MARK(first_start);
    diffs_ref = alg_diff_standard_c(cnt, new);
    TS_MARK(first_end);

    imgs->out = memset(out, 0XD7, sizeof(out));;
    imgs->smartmask_buffer = memset(smartmask_buffer, 0xCA, sizeof(smartmask_buffer));
    TS_MARK(second_start);
    diffs = alg_diff_standard(cnt, new);
    TS_MARK(second_end);

    assert(diffs == diffs_ref);
    for (int i = 0; i < sizeof(smartmask_buffer)/sizeof(smartmask_buffer[0]); i++) {
        assert(smartmask_buffer_ref[i] == smartmask_buffer[i]);
    }
    for (int i = 0; i < sizeof(out); i++) {
        assert(out_ref[i] == out[i]);
    }

    int64_t time1, time2;
    TS_CONVERT(first_end - first_start, time1);
    TS_CONVERT(second_end - second_start, time2);

    if (N) {
        printf("Test %s:%d passed, %" PRIu64 " vs %" PRIu64 " (%" PRId64 ") %lf%%\n",
               __FUNCTION__, N, time1, time2, time1 - time2,
               (double)time2/(double)time1*100.0);
    }

}

void test_alg_diff_standart(int width, int height, unsigned char noise,
                            unsigned char * ref, unsigned char * mask, unsigned char * new,
                            unsigned char * smartmask_final)
{
    struct context cnt;
    struct images *imgs = &cnt.imgs;
    imgs->motionsize = width * height;
    cnt.noise = noise;
    imgs->ref = ref;
    imgs->smartmask_final = smartmask_final;


    // interlal part of the loop depends on
    // N  mask, smartmask_speed and, cnt->event_nr != cnt->prev_event,
    // 1   0           0                           0
    // 2   0           1                           0
    // 3   0           1                           1
    // 4   1           0                           0
    // 5   1           1                           0
    // 6   1           1                           1
    // There is 6 possible loops variants without conditions

    // 0 Warmup
    imgs->mask = mask;
    cnt.smartmask_speed = 1;
    cnt.event_nr = 0;
    cnt.prev_event = 1;
    test_alg_diff_standart_one_case(&cnt, new, 0);

    // 1
    imgs->mask = NULL;
    cnt.smartmask_speed = 0;
    cnt.event_nr = cnt.prev_event = 1;
    test_alg_diff_standart_one_case(&cnt, new, 1);

    // 2
    imgs->mask = NULL;
    cnt.smartmask_speed = 1;
    cnt.event_nr = cnt.prev_event = 1;
    test_alg_diff_standart_one_case(&cnt, new, 2);

    // 3
    imgs->mask = NULL;
    cnt.smartmask_speed = 1;
    cnt.event_nr = 0;
    cnt.prev_event = 1;
    test_alg_diff_standart_one_case(&cnt, new, 3);

    // 4
    imgs->mask = mask;
    cnt.smartmask_speed = 0;
    cnt.event_nr = cnt.prev_event = 1;
    test_alg_diff_standart_one_case(&cnt, new, 4);

    // 5
    imgs->mask = mask;
    cnt.smartmask_speed = 1;
    cnt.event_nr = cnt.prev_event = 1;
    test_alg_diff_standart_one_case(&cnt, new, 5);

    // 6
    imgs->mask = mask;
    cnt.smartmask_speed = 1;
    cnt.event_nr = 0;
    cnt.prev_event = 1;
    test_alg_diff_standart_one_case(&cnt, new, 6);
}

static void test_alg_update_reference_frame_one_case(struct context *cnt,
                                                     unsigned char *ref_buf,
                                                     unsigned char *ref_buf_ref,
                                                     int *ref_dyn_buf,
                                                     int *ref_dyn_buf_ref,
                                                     int action,
                                                     int len, int N)
{
    uint64_t first_start, first_end, second_start, second_end;

    cnt->imgs.ref = ref_buf_ref;
    cnt->imgs.ref_dyn = ref_dyn_buf_ref;

    TS_MARK(first_start);
    alg_update_reference_frame_c(cnt, action);
    TS_MARK(first_end);

    cnt->imgs.ref = ref_buf;
    cnt->imgs.ref_dyn = ref_dyn_buf;

    TS_MARK(second_start);
    alg_update_reference_frame(cnt, action);
    TS_MARK(second_end);

    for (int i = 0; i < len; i++) {
        assert(ref_buf_ref[i] == ref_buf[i]);
    }
    for (int i = 0; i < len; i++) {
        assert(ref_dyn_buf_ref[i] == ref_dyn_buf[i]);
    }

    int64_t time1, time2;
    TS_CONVERT(first_end - first_start, time1);
    TS_CONVERT(second_end - second_start, time2);
    printf("Test %s:%d passed, %" PRIu64 " vs %" PRIu64 " (%" PRId64 ") %lf%%\n",
           __FUNCTION__, N, time1, time2, time1 - time2,
           (double)time2/(double)time1*100.0);
}

void test_alg_update_reference_frame(int width, int height, unsigned char noise,
                                     unsigned char * image_virgin, unsigned char * ref,
                                     unsigned char * smartmask, unsigned char * out,
                                     int *ref_dyn)
{
    static unsigned char ref_buf[test_width * test_height];
    static unsigned char ref_buf_ref[test_width * test_height];

    static int ref_dyn_buf[test_width * test_height];
    static int ref_dyn_buf_ref[test_width * test_height];

    // Prepeare output buffers
    memcpy(ref_buf, ref, sizeof(ref_buf));
    memcpy(ref_buf_ref, ref, sizeof(ref_buf_ref));
    memcpy(ref_dyn_buf, ref_dyn, sizeof(ref_dyn_buf));
    memcpy(ref_dyn_buf_ref, ref_dyn, sizeof(ref_dyn_buf_ref));

    struct context cnt;
    cnt.lastrate = 3;
    cnt.noise = noise;
    cnt.imgs.image_virgin = image_virgin;
    cnt.imgs.smartmask_final = smartmask;
    cnt.imgs.out = out;
    cnt.imgs.motionsize = width * height;
    cnt.imgs.size = width * height;

    test_alg_update_reference_frame_one_case(&cnt, ref_buf, ref_buf_ref,
                                             ref_dyn_buf, ref_dyn_buf_ref,
                                             UPDATE_REF_FRAME,
                                             width * height, 1);

    test_alg_update_reference_frame_one_case(&cnt, ref_buf, ref_buf_ref,
                                             ref_dyn_buf, ref_dyn_buf_ref,
                                             RESET_REF_FRAME,
                                             width * height, 2);

    test_alg_update_reference_frame_one_case(&cnt, ref_buf, ref_buf_ref,
                                             ref_dyn_buf, ref_dyn_buf_ref,
                                             UPDATE_REF_FRAME,
                                             width * height, 3);
}

static unsigned char test_img_data[test_width * test_height];
static unsigned char test_img_data_new[test_width * test_height];
static unsigned char test_img_data_mask[test_width * test_height];
static int test_lables[test_width * test_height];
static unsigned char test_smartmask_final[test_width * test_height];

int main(int argc, const char* argv[]) {
    TS_INIT();

    for (int j = 0; j < 2; j++) {
        // filling test data
        for (int i = 0; i < test_height * test_width; i++) {
            uint32_t t = rand();
            test_img_data[i] = t;
            test_img_data_new[i] = t >> 8;
            test_img_data_mask[i] = t >> 13;
            test_lables[i] = rand();
            test_smartmask_final[i] = (t >> 16) % 2 ? 0xff : 0;
        }
        unsigned char noise = rand();

        test_locate_center_size(test_width, test_height, test_img_data, test_lables);
        test_locate_center_size(test_width - 2, test_height - 2, test_img_data, test_lables);

        test_alg_diff_standart(test_width, test_height, noise,
                               test_img_data, test_img_data_mask, test_img_data_new,
                               test_smartmask_final);
        test_alg_diff_standart(test_width - 1, test_height, noise,
                               test_img_data, test_img_data_mask, test_img_data_new,
                               test_smartmask_final);

        //Make labbels value fit in uint16_t
        for (int i = 0; i < test_height * test_width; i++) {
            test_lables[i] = test_lables[i] & 0xFFFF;
        }

        test_alg_update_reference_frame(test_width, test_height, noise,
                                        test_img_data, test_img_data_new,
                                        test_img_data_mask, test_smartmask_final,
                                        test_lables);
        test_alg_update_reference_frame(test_width - 1, test_height, noise,
                                        test_img_data, test_img_data_new,
                                        test_img_data_mask, test_smartmask_final,
                                        test_lables);
    }

    return 0;
}
