#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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
#define BLOCKPX   50

static void
init (struct context *ctx, unsigned char **new)
{
	ctx->imgs.motionsize = WIDTH * HEIGHT;
	ctx->imgs.ref = malloc(ctx->imgs.motionsize);
	ctx->imgs.mask = malloc(ctx->imgs.motionsize);
	ctx->imgs.smartmask_final = malloc(ctx->imgs.motionsize);
	*new = malloc(ctx->imgs.motionsize);
}

static void
clean (struct context *ctx, unsigned char *new)
{
	ctx->noise = 0;
	memset(ctx->imgs.ref, 0, WIDTH * HEIGHT);
	memset(ctx->imgs.mask, 0, WIDTH * HEIGHT);
	memset(ctx->imgs.smartmask_final, 0, WIDTH * HEIGHT);
	memset(new, 0, WIDTH * HEIGHT);
}

static void
apply_pattern (unsigned char *pattern, unsigned char *img)
{
	int x = 0, y = 0;

	/* Each pattern represents BLOCKPX * BLOCKPX pixels in the output: */
	while (y < HEIGHT) {
		unsigned char *col = pattern;
		while (x < WIDTH) {
			*img++ = *col;
			if (++x % BLOCKPX == 0) {
				col++;
			}
		}
		/* After BLOCKPX rows, move to next: */
		if (++y % BLOCKPX == 0) {
			pattern += WIDTH / BLOCKPX;
		}
	}
}

static void
random_patterns (int seed, struct context *ctx, unsigned char *new)
{
	int i;
	unsigned char *c;
	unsigned char pattern[(HEIGHT * WIDTH) / BLOCKPX];
	unsigned char *ptrs[4];

	ptrs[0] = ctx->imgs.ref;
	ptrs[1] = ctx->imgs.mask;
	ptrs[2] = ctx->imgs.smartmask_final;
	ptrs[3] = new;

	srand(seed);

	for (i = 0; i < 4; i++) {
		for (c = pattern; c < (pattern + sizeof(pattern)); c++) {
			*c = rand() / (RAND_MAX / 256);
		}
		apply_pattern(pattern, ptrs[i]);
	}
}

static void
testsuite (char *name, struct context *ctx, unsigned char *new, void (*func)(struct context *, unsigned char *))
{
	int i;

	printf("---\n%s\n", name);
	clean(ctx, new);

	timer_start();
	for (i = 100; i > 0; i--) {
		func(ctx, new);
	}
	timer_stop();

	printf("Noise level: %d\nTime: %.4f sec\n", ctx->noise, timer_sec());

	for (i = 100; i > 0; i--) {
		clean(ctx, new);
		random_patterns(i, ctx, new);
		func(ctx, new);
		printf("%d ", ctx->noise);
	}
	puts("");
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
