static void alg_update_reference_frame_plain(struct context *cnt, int action)
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
            int includemask = (thresholdmask && (*smartmask != 0));

            /* Exclude pixels from ref frame well below noise level. */
            if (includemask) {
                if (*ref_dyn == 0) { /* Always give new pixels a chance. */
                    *ref_dyn = 1;
                } else if (*ref_dyn > accept_timer) { /* Include static Object after some time. */
                    *ref_dyn = 0;
                    *ref = *image_virgin;
                } else if (*out) {
                    (*ref_dyn)++; /* Motionpixel? Keep excluding from ref frame. */
                } else {
                    *ref_dyn = 0; /* Nothing special - release pixel. */
                    *ref = (*ref + *image_virgin) / 2;
                }

            } else {  /* No motion: copy to ref frame. */
                *ref_dyn = 0; /* Reset pixel */
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
