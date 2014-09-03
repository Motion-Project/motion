#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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

static void
permutate (int action, void (*func)(struct context *, int))
{
	unsigned char ref[16];
	unsigned char out[16];
	unsigned char image_virgin[16];
	unsigned char smartmask_final[16];
	uint16_t ref_dyn[16];
	struct context ctx;
	unsigned int ref_cksum;
	unsigned int ref_dyn_cksum;

	int i, iter_ref_dyn, iter_smartmask, iter_image_virgin, iter_out, iter_ref;

	ctx.noise = 0;
	ctx.lastrate = 0;
	ctx.imgs.ref = ref;
	ctx.imgs.out = out;
	ctx.imgs.image_virgin = image_virgin;
	ctx.imgs.smartmask_final = smartmask_final;
	ctx.imgs.ref_dyn = ref_dyn;
	ctx.imgs.size = 16;
	ctx.imgs.motionsize = 16;

	/* For the purposes of the routine, smartmask is zero or nonzero: */
	for (iter_smartmask = 0; iter_smartmask < 2; iter_smartmask++) {
		memset(smartmask_final, iter_smartmask, ctx.imgs.size);

		/* For the purposes of the routine, out is zero or nonzero: */
		for (iter_out = 0; iter_out < 2; iter_out++) {
			memset(out, iter_out, ctx.imgs.size);

			ref_cksum = 0;
			ref_dyn_cksum = 0;

			for (iter_image_virgin = 0; iter_image_virgin < 256; iter_image_virgin++) {
				memset(image_virgin, iter_image_virgin, ctx.imgs.size);

				/* ref_dyn has a limited range: */
				for (iter_ref_dyn = 0; iter_ref_dyn < 10; iter_ref_dyn++) {
					for (i = 0; i < 16; i++) {
						ref_dyn[i] = iter_ref_dyn + 1;
					}
					for (iter_ref = 0; iter_ref < 256; iter_ref++) {
						memset(ref, iter_ref, ctx.imgs.size);
						func(&ctx, action);
						ref_cksum += ref[0];

						for (i = 0; i < 16; i++) {
							ref_dyn_cksum += ref_dyn[i];
						}
					}
				}
			}
			printf("%d %d\n", ref_cksum, ref_dyn_cksum);
		}
	}
}

static void
testsuite (char *name, struct context *ctx, int action, void (*func)(struct context *, int))
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

	permutate(action, func);
}

#define UPDATE_REF_FRAME  1
#define ACCEPT_STATIC_OBJECT_TIME 10  /* Seconds */
#define EXCLUDE_LEVEL_PERCENT 20

#include "../alg_update_reference_frame.plain.c"
#include "../alg_update_reference_frame.sse2-algo.c"

int
main ()
{
	struct context ctx;

	init(&ctx);

	testsuite("plain", &ctx, UPDATE_REF_FRAME, alg_update_reference_frame_plain);
	testsuite("plain, SSE2 algorithm demo", &ctx, UPDATE_REF_FRAME, alg_update_reference_frame_sse2_algo);

	free(ctx.imgs.ref);
	free(ctx.imgs.out);
	free(ctx.imgs.ref_dyn);
	free(ctx.imgs.image_virgin);
	free(ctx.imgs.smartmask_final);

	return 0;
}
