#include <sys/time.h>
#include "config.h"

void set_time(struct timeval * now);

double get_sec(struct timeval x);

double time_diff(struct timeval from, struct timeval to);

double elapsed_time(struct timeval from);

void reset(struct timeval * x);

int is_elapsed(struct Configuration config);
