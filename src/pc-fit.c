#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <assert.h>
#include <unistd.h>
#include <stdbool.h>
#include <pthread.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <wiiuse.h>
#include <gtk/gtk.h>
#include "pc-fit.h"

#define MAX_WIIMOTES	1
#define TIMEOUT			5

#define FPS				(75)
#define MILLI			(1. / FPS)  // Time between two frame
#define MICROSECOND		(1000 * 1000)  // One second in microsecond
#define REFRESH_TIME	(MILLI * MICROSECOND)  // In microsecond

/*
 * Front end
 */

static struct ARG arg;

struct PointBuffer {
	int x;
	int y;
} point;

pthread_mutex_t point_lock = PTHREAD_MUTEX_INITIALIZER;

void get_point(int *x, int *y) {
	pthread_mutex_lock(&point_lock);
	*x = point.x;
	*y = point.y;
	pthread_mutex_unlock(&point_lock);
}

void set_point(int x, int y) {
	pthread_mutex_lock(&point_lock);
	point.x = x;
	point.y = y;
	pthread_mutex_unlock(&point_lock);
}

float get_weight(float cells[]) {
	float sum = 0;
	for(int i = 0; i < 4; i++) {
		sum += cells[i];
	}
	return sum;
}

void center_of_gravity(float cells[], float center[]) {
	center[X] = (cells[TR] + cells[BR]) - (cells[TL] + cells[BL]);
	center[Y] = (cells[TL] + cells[TR]) - (cells[BL] + cells[BR]);
}

/*
 * BACK END
 */

// Wii balance board zone.

struct WM {
	wiimote ** wiimotes;
	int connected;
} device;

short any_wiimote_connected(wiimote** wm, int wiimotes) {
	if (!wm) {
		return 0;
	}
	for (int i = 0; i < wiimotes; i++) {
		if (wm[i] && WIIMOTE_IS_CONNECTED(wm[i])) {
			return 1;
		}
	}
	return 0;
}

void handle_event(struct wiimote_t* wm) {
	if (wm->exp.type == EXP_WII_BOARD) {
		struct wii_board_t bb = *(wii_board_t*)&wm->exp.wb;
		float cells[] = {bb.tl, bb.tr, bb.bl, bb.br};
		arg.notify(cells);
	}
}

// Thread Section

struct Thread{
	pthread_t t;
	bool run;
	bool stop;
	char name[20];
}
reader = {.run = FALSE, .stop = FALSE, .name = {"reader"}},
finder = {.run = FALSE, .stop = FALSE, .name = {"finder"}};

void alloc_thread(struct Thread * t, void* func) {
	struct Thread thread = *t;
	if (thread.run == FALSE) {
		pthread_create(&(thread.t), NULL, func, NULL);
		pthread_detach(thread.t);
		thread.run = TRUE;
		thread.stop = FALSE;
		printf("Thread %s is started\n", thread.name);
	}
	else {
		printf("Thread %s is already started\n", thread.name);
	}
}

int read_board() {
	while (any_wiimote_connected(device.wiimotes, MAX_WIIMOTES)) {
		if (wiiuse_poll(device.wiimotes, MAX_WIIMOTES)) {
			for (int i = 0; i < MAX_WIIMOTES; ++i) {
				switch (device.wiimotes[i]->event) {
					case WIIUSE_EVENT:
						handle_event(device.wiimotes[i]);
						break;
					case WIIUSE_DISCONNECT:
					case WIIUSE_UNEXPECTED_DISCONNECT:
						puts("the wiimote disconnected.");
						break;
					case WIIUSE_WII_BOARD_CTRL_INSERTED:
						puts("Balance board controller inserted.");
						break;
					default:
						puts("Default.");
						break;
				}
			}
		}
	}
	wiiuse_cleanup(device.wiimotes, MAX_WIIMOTES);
	reader.run = FALSE;
	return 0;
}

int read_file() {
	int file = open(arg.file, O_RDONLY);
	if (file < 0) {
		puts("Error opening file");
		return -1;
	}
	struct wii_board_t bb;
	while (!reader.stop && read(file, &bb, sizeof(bb)) ) {
		arg.notify(&bb);
		usleep(REFRESH_TIME);
	}
	puts("EOF\n");
	close(file);
	return 0;
}

void reader_f(void* args) {
	if (arg.file == NULL)
		read_board();
	else
		read_file();
	reader.run = FALSE;
	reader.stop = FALSE;
	printf("Close reader thread!\n");
	pthread_exit(NULL);
}

void* finder_f(void* args) {
	int found = wiiuse_find(device.wiimotes, MAX_WIIMOTES, TIMEOUT);
	if (!found) {
		printf("No wiimotes found.\n");
		return NULL;
	}
	device.connected = wiiuse_connect(device.wiimotes, MAX_WIIMOTES);
	if (device.connected) {
		printf("Connected to %i wiimotes (of %i found).\n", device.connected, found);
		reader.stop = TRUE;
		alloc_thread(&reader, reader_f);
	}
	else
		printf("Failed to connect to any wiimote.\n");
	finder.stop = FALSE;
	finder.run = FALSE;
	pthread_exit(NULL);
}

void find_balance_board() {
	if (arg.file == NULL)
		alloc_thread(&finder, finder_f);
	else
		alloc_thread(&reader, reader_f);
}

void init_pcfit(struct ARG a) {
	memcpy(&arg, &a, sizeof(struct ARG));
	if (arg.file == NULL)
		device.wiimotes =  wiiuse_init(MAX_WIIMOTES);
}

void close_lib() {
	finder.stop = TRUE;
	reader.stop = TRUE;
	pthread_mutex_destroy(&point_lock);
}
