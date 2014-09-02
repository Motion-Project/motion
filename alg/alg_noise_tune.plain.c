#ifndef ABS
#define ABS(x)             ((x) < 0 ? -(x) : (x))
#endif

/**
 * alg_noise_tune_plain
 *
 */
static void alg_noise_tune_plain(struct context *cnt, unsigned char *new)
{
    struct images *imgs = &cnt->imgs;
    int i;
    unsigned char *ref = imgs->ref;
    int diff, sum = 0, count = 0;
    unsigned char *mask = imgs->mask;
    unsigned char *smartmask = imgs->smartmask_final;

    i = imgs->motionsize;

    for (; i > 0; i--) {
        diff = ABS(*ref - *new);

        if (mask)
            diff = ((diff * *mask++) / 255);

        if (*smartmask) {
            sum += diff + 1;
            count++;
        }

        ref++;
        new++;
        smartmask++;
    }

    if (count > 3)  /* Avoid divide by zero. */
        sum /= count / 3;

    /* 5: safe, 4: regular, 3: more sensitive */
    cnt->noise = 4 + (cnt->noise + sum) / 2;
}
