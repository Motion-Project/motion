#include <stdio.h>
#include <stdlib.h>

#include "timer.h"

/* Stub structures for test purposes: */
struct images
{
	unsigned char *ref;
	unsigned char *mask;
	unsigned char *smartmask_final;
	int motionsize;
};

struct context
{
	struct images imgs;
	int noise;
};

#define WIDTH    600
#define HEIGHT   400

static void
init (struct context *ctx, unsigned char **new)
{
	ctx->noise = 0;
	ctx->imgs.motionsize = WIDTH * HEIGHT;
	ctx->imgs.ref = malloc(ctx->imgs.motionsize);
	ctx->imgs.mask = malloc(ctx->imgs.motionsize);
	ctx->imgs.smartmask_final = malloc(ctx->imgs.motionsize);
	*new = malloc(ctx->imgs.motionsize);
}

static void
testsuite (char *name, struct context *ctx, unsigned char *new, void (*func)(struct context *, unsigned char *))
{
	int i;

	printf("---\n%s\n", name);

	timer_start();
	for (i = 100; i > 0; i--) {
		func(ctx, new);
	}
	timer_stop();

	printf("Noise level: %d\nTime: %.4f sec\n", ctx->noise, timer_sec());

}

#include "../alg_noise_tune.plain.c"

int
main ()
{
	struct context ctx;
	unsigned char *new;

	init(&ctx, &new);

	testsuite("plain", &ctx, new, alg_noise_tune_plain);

	free(new);
	free(ctx.imgs.ref);
	free(ctx.imgs.mask);
	free(ctx.imgs.smartmask_final);

	return 0;
}
