#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <emmintrin.h>

#include "../sse2.h"
#include "timer.h"

/* Stub structures for test purposes: */
struct images
{
	unsigned char *ref;
	unsigned char *out;
	uint16_t *ref_dyn;
	unsigned char *image_virgin;
	unsigned char *smartmask_final;
	int size;
	int motionsize;
};

struct context
{
	struct images imgs;
	int noise;
	unsigned int lastrate;
};

#define WIDTH    600
#define HEIGHT   400
#define BLOCKPX   50

static void
init (struct context *ctx)
{
	ctx->imgs.motionsize = WIDTH * HEIGHT;
	ctx->imgs.ref = malloc(ctx->imgs.motionsize);
	ctx->imgs.out = malloc(ctx->imgs.motionsize);
	ctx->imgs.ref_dyn = malloc(ctx->imgs.motionsize * sizeof(*ctx->imgs.ref_dyn));
	ctx->imgs.image_virgin = malloc(ctx->imgs.motionsize);
	ctx->imgs.smartmask_final = malloc(ctx->imgs.motionsize);
}

static void
clean (struct context *ctx)
{
	ctx->noise = 0;
	ctx->lastrate = 0;
	memset(ctx->imgs.ref, 0, WIDTH * HEIGHT);
	memset(ctx->imgs.out, 0, WIDTH * HEIGHT);
	memset(ctx->imgs.ref_dyn, 0, WIDTH * HEIGHT * sizeof(*ctx->imgs.ref_dyn));
	memset(ctx->imgs.image_virgin, 0, WIDTH * HEIGHT);
	memset(ctx->imgs.smartmask_final, 0, WIDTH * HEIGHT);
	ctx->imgs.size = WIDTH * HEIGHT;
	ctx->imgs.motionsize = WIDTH * HEIGHT;
}

static int
equal_output (struct context *ctx, int action, void (*func_a)(struct context *, int), void (*func_b)(struct context *, int))
{
	int i, ret = 1;
	struct context cxs[2];

	for (i = 0; i < 2; i++)
	{
		/* Copy original context: */
		memcpy(&cxs[i], ctx, sizeof(*ctx));
		memcpy(&cxs[i].imgs, &ctx->imgs, sizeof(ctx->imgs));

		/* Copy the original image structures: */
		#define CPY(x)  cxs[i].imgs.x = malloc(ctx->imgs.size * sizeof(*ctx->imgs.x)); memcpy(cxs[i].imgs.x, ctx->imgs.x, ctx->imgs.size * sizeof(*ctx->imgs.x));
		CPY(ref)
		CPY(out)
		CPY(image_virgin)
		CPY(smartmask_final)
		CPY(ref_dyn)
		#undef CPY
	}
	/* Run both functions on their own copy: */
	func_a(&cxs[0], action);
	func_b(&cxs[1], action);

	/* Compare image outputs: */
	#define CMP(x)  if (memcmp(cxs[0].imgs.x, cxs[1].imgs.x, sizeof(*cxs[0].imgs.x)) != 0) { ret = 0; goto out; }
	CMP(ref)
	CMP(ref_dyn)
	#undef CMP

out:	/* Free memory, return: */
	for (i = 0; i < 2; i++) {
		free(cxs[i].imgs.ref);
		free(cxs[i].imgs.out);
		free(cxs[i].imgs.image_virgin);
		free(cxs[i].imgs.smartmask_final);
		free(cxs[i].imgs.ref_dyn);
	}
	return ret;
}

static void
permutate (int action, void (*func_a)(struct context *, int), void (*func_b)(struct context *, int))
{
	#define STRIPSZ 41

	unsigned char ref[STRIPSZ];
	unsigned char out[STRIPSZ];
	unsigned char image_virgin[STRIPSZ];
	unsigned char smartmask_final[STRIPSZ];
	uint16_t ref_dyn[STRIPSZ];
	struct context ctx;

	int i, iter_ref_dyn, iter_smartmask, iter_image_virgin, iter_out, iter_ref;

	ctx.noise = 0;
	ctx.lastrate = 0;
	ctx.imgs.ref = ref;
	ctx.imgs.out = out;
	ctx.imgs.image_virgin = image_virgin;
	ctx.imgs.smartmask_final = smartmask_final;
	ctx.imgs.ref_dyn = ref_dyn;
	ctx.imgs.size = STRIPSZ;
	ctx.imgs.motionsize = STRIPSZ;

	/* For the purposes of the routine, smartmask is zero or nonzero: */
	for (iter_smartmask = 0; iter_smartmask < 2; iter_smartmask++) {
		memset(smartmask_final, iter_smartmask, ctx.imgs.size);

		/* For the purposes of the routine, out is zero or nonzero: */
		for (iter_out = 0; iter_out < 2; iter_out++) {
			memset(out, iter_out, ctx.imgs.size);

			for (iter_image_virgin = 0; iter_image_virgin < 256; iter_image_virgin++) {
				for (i = 0; i < ctx.imgs.size; i++) {
					image_virgin[i] = iter_image_virgin + i;
				}
				/* ref_dyn has a limited range: */
				for (iter_ref_dyn = 0; iter_ref_dyn < 10; iter_ref_dyn++) {
					for (i = 0; i < ctx.imgs.size; i++) {
						ref_dyn[i] = iter_ref_dyn + i;
					}
					for (iter_ref = 0; iter_ref < 256; iter_ref++) {
						for (i = 0; i < ctx.imgs.size; i++) {
							ref[i] = iter_ref + i;
						}
						/* For this permutation, check that both functions
						 * return the same output data: */
						if (equal_output(&ctx, action, func_a, func_b) == 0) {
							printf("Functions do NOT match!\n");
							return;
						}
					}
				}
			}
		}
	}
	printf("Functions MATCH\n");
}

static void
timing (char *name, struct context *ctx, int action, void (*func)(struct context *, int))
{
	int i;
	float total_time = 0.0f;

	printf("---\n%s\n", name);
	clean(ctx);

	for (i = 300; i > 0; i--) {
		timer_start();
		func(ctx, action);
		timer_stop();
		total_time += timer_sec();
	}

	/* Print bogus value to prevent the loop from being optimized out: */
	printf("Value: %d\nTime: %.4f sec\n", ctx->imgs.ref[0], total_time);
}

#define UPDATE_REF_FRAME  1
#define ACCEPT_STATIC_OBJECT_TIME 10  /* Seconds */
#define EXCLUDE_LEVEL_PERCENT 20

#include "../alg_update_reference_frame.plain.c"
#include "../alg_update_reference_frame.sse2-algo.c"
#include "../alg_update_reference_frame.sse2.c"

int
main ()
{
	struct context ctx;

	init(&ctx);

	timing("plain", &ctx, UPDATE_REF_FRAME, alg_update_reference_frame_plain);
	timing("plain, SSE2 algorithm demo", &ctx, UPDATE_REF_FRAME, alg_update_reference_frame_sse2_algo);
	timing("SSE2", &ctx, UPDATE_REF_FRAME, alg_update_reference_frame_sse2);

	permutate(UPDATE_REF_FRAME, alg_update_reference_frame_plain, alg_update_reference_frame_sse2_algo);
	permutate(UPDATE_REF_FRAME, alg_update_reference_frame_plain, alg_update_reference_frame_sse2);

	free(ctx.imgs.ref);
	free(ctx.imgs.out);
	free(ctx.imgs.ref_dyn);
	free(ctx.imgs.image_virgin);
	free(ctx.imgs.smartmask_final);

	return 0;
}
