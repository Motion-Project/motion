static __inline __m128i
_mm_absdiff_epu8 (__m128i x, __m128i y)
{
    /* Calculate absolute difference: abs(x - y): */
    return _mm_or_si128(_mm_subs_epu8(x, y), _mm_subs_epu8(y, x));
}

static __inline __m128i
_mm_div255_epu16 (__m128i x)
{
    /* Divide 8 16-bit uints by 255:
     * x := ((x + 1) + (x >> 8)) >> 8: */
    return _mm_srli_epi16(_mm_adds_epu16(
        _mm_adds_epu16(x, _mm_set1_epi16(1)),
        _mm_srli_epi16(x, 8)), 8);
}

static __inline void
sse_u8_to_u16 (__m128i in, __m128i *__restrict lo, __m128i *__restrict hi)
{
    /* Zero-extend an 8-bit vector to two 16-bit vectors: */
    *lo = _mm_unpacklo_epi8(in, _mm_setzero_si128());
    *hi = _mm_unpackhi_epi8(in, _mm_setzero_si128());
}

static __inline void
sse_u16_to_u32 (__m128i in, __m128i *__restrict lo, __m128i *__restrict hi)
{
    /* Zero-extend a 16-bit vector to two 32-bit vectors: */
    *lo = _mm_unpacklo_epi16(in, _mm_setzero_si128());
    *hi = _mm_unpackhi_epi16(in, _mm_setzero_si128());
}

static __inline __m128i
_mm_scale_epu8 (__m128i x, __m128i y)
{
    /* Returns an "alpha blend" of x with y;
     *   x := x * (y / 255)
     * Reorder: x := (x * y) / 255
     */
    __m128i xlo, xhi;
    __m128i ylo, yhi;

    /* Unpack x and y into 16-bit uints: */
    sse_u8_to_u16(x, &xlo, &xhi);
    sse_u8_to_u16(y, &ylo, &yhi);

    /* Multiply x with y, keeping the low 16 bits: */
    xlo = _mm_mullo_epi16(xlo, ylo);
    xhi = _mm_mullo_epi16(xhi, yhi);

    /* Divide by 255: */
    xlo = _mm_div255_epu16(xlo);
    xhi = _mm_div255_epu16(xhi);

    /* Repack the 16-bit uints to 8-bit values: */
    return _mm_packus_epi16(xlo, xhi);
}
