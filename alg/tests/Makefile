CFLAGS += -std=c89 -Werror -Wall -Wextra -pedantic -msse2 -O3
LDFLAGS += -lrt

.PHONY: clean

test_alg_noise_tune: test_alg_noise_tune.o timer.o
	$(CC) $(LDFLAGS) -o $@ $^

test_alg_noise_tune.o: ../alg_noise_tune.plain.c ../alg_noise_tune.sse2.c test_alg_noise_tune.c
	$(CC) $(CFLAGS) -o $@ -c test_alg_noise_tune.c

timer.o: timer.c
	$(CC) $(CFLAGS) -o $@ -c $^

clean:
	rm -f *.o test_alg_noise_tune
