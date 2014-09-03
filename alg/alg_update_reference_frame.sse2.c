/* The basic algorithm is demonstrated in 'alg_update_reference_frame.sse2-algo.c'
 *  as regular (non-SIMD), more readable code. Comments below allude to
 *  snippets from that file. The idea is to use masks instead of
 *  branches to compose the output, then do it in parallel. */

static void alg_update_reference_frame_sse2(struct context *cnt, int action)
{
    int accept_timer = cnt->lastrate * ACCEPT_STATIC_OBJECT_TIME;
    int i, threshold_ref;
    uint16_t *ref_dyn = cnt->imgs.ref_dyn;
    unsigned char *image_virgin = cnt->imgs.image_virgin;
    unsigned char *ref = cnt->imgs.ref;
    unsigned char *smartmask = cnt->imgs.smartmask_final;
    unsigned char *out = cnt->imgs.out;

    int sse_iters;
    __m128i threshrow, accepttimerrow, mask;

    if (cnt->lastrate > 5)  /* Match rate limit */
        accept_timer /= (cnt->lastrate / 3);

    if (action == UPDATE_REF_FRAME) { /* Black&white only for better performance. */
        threshold_ref = cnt->noise * EXCLUDE_LEVEL_PERCENT / 100;

        i = cnt->imgs.motionsize;

        /* Below we'll do a calculation to see whether our 8-bit uints
         * are *larger* than threshold_ref. Threshold_ref is an int, but
         * for the purposes of this check we can cast it to an 8-bit uint
         * and clamp it to 255; the comparator can never exceed that value: */
        threshrow = _mm_set1_epi8((threshold_ref > 0xFF) ? 0xFF : threshold_ref);

        /* Create a row of 8 uint16_t's with almost clamped accept timer: */
        accepttimerrow = _mm_set1_epi16((accept_timer > 0xFFFE) ? 0xFFFE : accept_timer);

        /* SSE row size is 16 bytes: */
        for (sse_iters = i >> 4; sse_iters > 0; sse_iters--)
        {
            /* Load reference row and virgin image: */
            __m128i refrow = _mm_loadu_si128((__m128i *)ref);
            __m128i vgnrow = _mm_loadu_si128((__m128i *)image_virgin);

            /* int thresholdmask = ((int)(abs(*ref - *image_virgin)) > threshold_ref); */
            __m128i thresholdmask = _mm_cmpgt_epu8(_mm_absdiff_epu8(refrow, vgnrow), threshrow);

            /* int includemask = (thresholdmask && !(*smartmask == 0)); */
            __m128i smartmaskzero = _mm_cmpeq_epi8(_mm_loadu_si128((__m128i *)smartmask), _mm_setzero_si128());
            __m128i includemask = _mm_andnot_si128(smartmaskzero, thresholdmask);

            /* Load the two ref_dyn's: */
            __m128i refdynlo = _mm_loadu_si128((__m128i *)(ref_dyn + 0));
            __m128i refdynhi = _mm_loadu_si128((__m128i *)(ref_dyn + 8));

            /* int refdynzero = (*ref_dyn == 0); */
            /* Make an 8-bit mask with 0xFF where ref_dyn == 0: */
            __m128i refdynzero = _mm_packs_epi16(
                _mm_cmpeq_epi16(refdynlo, _mm_setzero_si128()),
                _mm_cmpeq_epi16(refdynhi, _mm_setzero_si128())
            );

            /* int refdyntimer = (*ref_dyn > accept_timer); */
            /* Make an 8-bit mask with 0xFF where ref_dyn > accept_timer: */
            __m128i refdyntimer = _mm_packs_epi16(
                _mm_cmpgt_epu16(refdynlo, accepttimerrow),
                _mm_cmpgt_epu16(refdynhi, accepttimerrow)
            );

            /* int outzero = (*out == 0); */
            __m128i outzero = _mm_cmpeq_epi8(_mm_loadu_si128((__m128i *)out), _mm_setzero_si128());

            /* *ref_dyn &= (includemask && !(refdynzero || refdyntimer || outzero)); */
            mask = _mm_andnot_si128(_mm_or_si128(_mm_or_si128(refdynzero, refdyntimer), outzero), includemask);

            /* Duplicate mask to 16-bit widths: */
            refdynlo = _mm_and_si128(refdynlo, _mm_unpacklo_epi8(mask, mask));
            refdynhi = _mm_and_si128(refdynhi, _mm_unpackhi_epi8(mask, mask));

            /* if (includemask && !(refdynzero || refdyntimer) && outzero) *ref = (*ref + *image_virgin) / 2; */
            mask = _mm_and_si128(_mm_andnot_si128(_mm_or_si128(refdynzero, refdyntimer), includemask), outzero);
            refrow = _mm_blendv_si128(refrow, _mm_avg_epu8(refrow, vgnrow), mask);

            /* if (includemask && !((refdyntimer || outzero) && !refdynzero)) *ref_dyn += 1; */
            mask = _mm_andnot_si128(_mm_andnot_si128(refdynzero, _mm_or_si128(refdyntimer, outzero)), includemask);
            refdynlo = _mm_adds_epu16(refdynlo, _mm_and_si128(_mm_set1_epi16(1), _mm_unpacklo_epi8(mask, mask)));
            refdynhi = _mm_adds_epu16(refdynhi, _mm_and_si128(_mm_set1_epi16(1), _mm_unpackhi_epi8(mask, mask)));

            /* Store the two ref dyn's back: */
            _mm_storeu_si128((__m128i *)(ref_dyn + 0), refdynlo);
            _mm_storeu_si128((__m128i *)(ref_dyn + 8), refdynhi);

            /* if (!(includemask && !(refdyntimer && !refdynzero))) *ref = *image_virgin; */
            mask = _mm_andnot_si128(_mm_andnot_si128(refdynzero, refdyntimer), includemask);
            refrow = _mm_blendv_si128(vgnrow, refrow, mask);

            /* Store ref back: */
            _mm_storeu_si128((__m128i *)ref, refrow);

            ref += 16;
            image_virgin += 16;
            smartmask += 16;
            ref_dyn += 16;
            out += 16;
        }

        /* Let the bytewise code handle the remaining bytes: */
        for (i = cnt->imgs.motionsize & 0x0F; i > 0; i--) {
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
