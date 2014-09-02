/**
 * alg_noise_tune_plain
 *
 */
static void alg_noise_tune_plain(struct context *cnt, unsigned char *new)
{
    struct images *imgs = &cnt->imgs;
    unsigned char *ref = imgs->ref;
    unsigned int sum = 0, count = 0;
    unsigned char *mask = imgs->mask;
    unsigned char *smartmask = imgs->smartmask_final;

    int i = imgs->motionsize;

    for (; i > 0; i--) {
        unsigned char absdiff = (*ref > *new) ? (*ref - *new) : (*new - *ref);

        if (mask)
            absdiff = ((absdiff * *mask++) / 255);

        if (*smartmask) {
            sum += absdiff + 1;
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
