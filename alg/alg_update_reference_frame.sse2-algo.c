/* This file is not meant to be included into the main program; it's intended
 * to showcase, benchmark and test the algorithm used in the SSE2 version of
 * this routine, in simple, non-vectorized code.
 * The idea is to replace all conditionals from the "plain" function with a
 * series of mask operations. This is slow when done per pixel (since we do all
 * calculations for all pixels), but fast in parallel.
 */
static void alg_update_reference_frame_sse2_algo(struct context *cnt, int action)
{
    int accept_timer = cnt->lastrate * ACCEPT_STATIC_OBJECT_TIME;
    int i, threshold_ref;
    int *ref_dyn = cnt->imgs.ref_dyn;
    unsigned char *image_virgin = cnt->imgs.image_virgin;
    unsigned char *ref = cnt->imgs.ref;
    unsigned char *smartmask = cnt->imgs.smartmask_final;
    unsigned char *out = cnt->imgs.out;

    if (cnt->lastrate > 5)  /* Match rate limit */
        accept_timer /= (cnt->lastrate / 3);

    if (action == UPDATE_REF_FRAME) { /* Black&white only for better performance. */
        threshold_ref = cnt->noise * EXCLUDE_LEVEL_PERCENT / 100;

        for (i = cnt->imgs.motionsize; i > 0; i--) {
            int thresholdmask = ((int)(abs(*ref - *image_virgin)) > threshold_ref);
            int includemask = (thresholdmask && !(*smartmask == 0));
            int refdynzero = (*ref_dyn == 0);
            int refdyntimer = (*ref_dyn > accept_timer);
            int outzero = (*out == 0);

            *ref_dyn &= (includemask && !(refdynzero || refdyntimer || outzero));

            if (includemask && !(refdynzero || refdyntimer) && outzero) {
                *ref = (*ref + *image_virgin) / 2;
            }
            if (includemask && !((refdyntimer || outzero) && !refdynzero)) {
                *ref_dyn += 1;
            }
            if (!(includemask && !(refdyntimer && !refdynzero))) {
                *ref = *image_virgin;
            }
            ref++;
            image_virgin++;
            smartmask++;
            ref_dyn++;
            out++;
        } /* end for i */

    } else {   /* action == RESET_REF_FRAME - also used to initialize the frame at startup. */
        /* Copy fresh image */
        memcpy(cnt->imgs.ref, cnt->imgs.image_virgin, cnt->imgs.size);
        /* Reset static objects */
        memset(cnt->imgs.ref_dyn, 0, cnt->imgs.motionsize * sizeof(*cnt->imgs.ref_dyn));
    }
}
