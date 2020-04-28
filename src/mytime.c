#include <sys/time.h>
#include <time.h>
#include <errno.h>
#include <stddef.h>
#include <stdio.h>

#include "mytime.h"
#include "config.h"

void set_time(struct timeval * now) {
	if(gettimeofday(now, NULL) != 0)
		printf("gettimeofday() failed, errno = %d\n", errno);
}

int is_elapsed(struct Configuration config) {
	struct timeval to = {0};
	to.tv_sec = config.last + config.every;

	return elapsed_time(to) <= 0;
}

double get_sec(struct timeval x) {
	return (double)x.tv_sec + ((double)x.tv_usec / 1000000);
}

double time_diff(struct timeval from, struct timeval to) {
	return get_sec(to) - get_sec(from);
}

double elapsed_time(struct timeval from) {
	struct timeval to;
	set_time(&to);
	return time_diff(from, to);
}

void reset(struct timeval * x) {
    *x = (struct timeval) {0};
}
