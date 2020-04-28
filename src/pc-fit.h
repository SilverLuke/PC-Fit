#define	MAX_WEIGHT		34.
#define TOTAL_WEIGHT	(MAX_WEIGHT * 4.)  // 34 max Kg multiplied each load cell

#define X 0
#define Y 1
#define XY 2

#define TL 0
#define TR 1
#define BL 2
#define BR 3

#include "config.h"

struct xwii_iface * find_balance_board();

void close_lib();

void init_pcfit(struct Argument argument, void (*handler_fun)(float *));

int start_pcfit();

void set_point(int x, int y);

void get_point(int *x, int *y);

void center_of_gravity(float cells[], float center[]);

float get_weight(float cells[]);
