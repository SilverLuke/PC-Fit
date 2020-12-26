#include <sys/time.h>
#include <stdbool.h>
#include "config.h"

void set_time(struct timeval * now);

double get_sec(struct timeval x);

double time_diff(struct timeval from, struct timeval to);

double elapsed_time(struct timeval from);

void reset(struct timeval * x);

bool check_time(struct Configuration config);

void print_time(struct timeval x);
