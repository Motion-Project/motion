/**
 * alg_noise_tune_sse2
 *
 */
static void alg_noise_tune_sse2(struct context *cnt, unsigned char *new)
{
    struct images *imgs = &cnt->imgs;
    unsigned char *ref = imgs->ref;
    unsigned int sum = 0, count = 0;
    unsigned char *mask = imgs->mask;
    unsigned char *smartmask = imgs->smartmask_final;

    int j, i = imgs->motionsize;

    int sse_iters;
    __m128i maskrow, zeromask;
    __m128i alo, ahi;
    __m128i ones = _mm_set1_epi8(1);
    __m128i sum16lo = _mm_setzero_si128();
    __m128i sum16hi = _mm_setzero_si128();
    __m128i sum32 = _mm_setzero_si128();
    __m128i count8 = _mm_setzero_si128();
    uint32_t total[4];
    uint8_t counts[16] __attribute__((aligned(16)));

    /* SSE reads 16 bytes at a time; truncating division: */
    for (sse_iters = i >> 4; sse_iters > 0; sse_iters--)
    {
        /* Load 16 bytes from images. Addresses need not be 16-byte aligned: */
        __m128i refrow = _mm_loadu_si128((__m128i *)ref);
        __m128i newrow = _mm_loadu_si128((__m128i *)new);

        /* Calculate absolute difference per byte: abs(ref - new): */
        __m128i absdiff = _mm_absdiff_epu8(refrow, newrow);

        /* If there is a mask image, alpha blend the absdiff by its pixels: */
        if (mask)
        {
            /* Load mask image data: */
            maskrow = _mm_loadu_si128((__m128i *)mask);
            mask += 16;

            /* "Alpha blend" absdiff with mask, absdiff *= (mask / 255): */
            absdiff = _mm_scale_epu8(absdiff, maskrow);
        }
        /* Add 1 to all diff values: */
        absdiff = _mm_adds_epu8(absdiff, ones);

        /* Fetch the smartmask values: */
        maskrow = _mm_loadu_si128((__m128i *)smartmask);

        /* Set diff values to 0 where smartmask is 0: */
        zeromask = _mm_cmpeq_epi8(maskrow, _mm_setzero_si128());
        absdiff = _mm_andnot_si128(zeromask, absdiff);

        /* Increment count for every nonzero value of smartmask: */
        count8 = _mm_adds_epu8(count8, _mm_andnot_si128(zeromask, ones));

        /* Split 16 bytes of sum into 16x16-bit values:
         * 0 . 1 . 2 . 3 . 4 . 5 . 6 . 7 .
         * 8 . 9 . A . B . C . D . E . F .
         */
        sse_u8_to_u16(absdiff, &alo, &ahi);
        sum16lo = _mm_adds_epu16(sum16lo, alo);
        sum16hi = _mm_adds_epu16(sum16hi, ahi);

        /* Offload these 16-bit counters into a 32-bit counter at least once
         * every 128 rounds to prevent overflow:
         * Also do this in the last iteration to empty out the counters: */
        if (!(sse_iters & 0x7F) || sse_iters == 1)
        {
            /* Split these two into 4x32 bits and do 32-bit additions:
             * 0 . . . 1 . . . 2 . . . 3 . . . +
             * 4 . . . 5 . . . 6 . . . 7 . . . +
             * 8 . . . 9 . . . A . . . B . . . +
             * C . . . D . . . E . . . F . . .
             * Add all of these to the running sum: */

            sse_u16_to_u32(sum16lo, &alo, &ahi);
            sum32 = _mm_add_epi32(sum32, _mm_add_epi32(alo, ahi));

            sse_u16_to_u32(sum16hi, &alo, &ahi);
            sum32 = _mm_add_epi32(sum32, _mm_add_epi32(alo, ahi));

            sum16lo = _mm_setzero_si128();
            sum16hi = _mm_setzero_si128();

            _mm_store_si128((__m128i *)counts, count8);
            for (j = 0; j < 16; j++) {
                count += counts[j];
            }
            count8 = _mm_setzero_si128();
        }

        ref += 16;
        new += 16;
        smartmask += 16;
    }
    /* Outside the hot loop, write out the running sum to memory
     * and add the four component uint32's to get the total sum: */
    _mm_storeu_si128((__m128i *)&total, sum32);
    sum = total[0] + total[1] + total[2] + total[3];

    /* We handled all 16-bit blocks. Truncate i to its value mod 16, so that
     * the regular bytewise code can handle the remainder: */
    i &= 0x0F;

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
