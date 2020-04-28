#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <inttypes.h>
#include <stdint.h>
#include <assert.h>
#include <unistd.h>
#include <stdbool.h>
#include <pthread.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>
#include <poll.h>

#include <xwiimote.h>
#include "pc-fit.h"
#include "config.h"

#define MAX_WIIMOTES	1
#define TIMEOUT			5

#define FPS				(60)
#define MILLI			(1. / FPS)  // Time between two frame
#define MICROSECOND		(1000 * 1000)  // One second in microsecond
#define REFRESH_TIME	(MILLI * MICROSECOND)  // In microsecond

void (*gui_callback)(float *);
struct Argument arg;

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

// Thread Section

struct Thread{
	pthread_t t;
	bool run;
	bool stop;
	pthread_mutex_t state_lock;
}
reader = {.run = false, .stop = false};

bool is_running(struct Thread thr) {
    bool stop;
    pthread_mutex_lock(&(reader.state_lock));
    stop = reader.stop;
    pthread_mutex_unlock(&(reader.state_lock));
    return stop;
}

void wait_stop(struct Thread thr) {
    pthread_mutex_lock(&(reader.state_lock));
    reader.stop = true;
    pthread_mutex_unlock(&(reader.state_lock));
}

struct xwii_iface * iface;
static void handle_event(const struct xwii_event *event) {
	float cell[] = {
	        event->v.abs[2].x / 100.,  // TL
	        event->v.abs[0].x / 100.,  // TR
	        event->v.abs[3].x / 100.,  // BL
	        event->v.abs[1].x / 100.   // BR
	};
	gui_callback(cell);
}

void alloc_thread(struct Thread * t, void* func, const char *name) {
	struct Thread thread = *t;
	if (thread.run == false) {
		pthread_create(&(thread.t), NULL, func, NULL);
		pthread_detach(thread.t);
		// thread.run = true;
		thread.stop = false;
        //pthread_setname_np(thread, name);
		printf("Info : Thread %s is started\n", name);
	}
	else {
		printf("Info : Thread %s is already started\n", name);
	}
}

int read_board() {
    struct xwii_event event;
	int ret = 0, fds_num;
	xwii_iface_open(iface, xwii_iface_available(iface) | XWII_IFACE_WRITABLE);

	struct pollfd fds[2];

	memset(&fds, 0, sizeof(fds));
	fds[0].fd = 0;
	fds[0].events = POLLIN;
	fds[1].fd = xwii_iface_get_fd(iface);
	fds[1].events = POLLIN;
	fds_num = 2;

	ret = xwii_iface_watch(iface, true);
	if (ret)
		puts("Error: Cannot initialize hotplug watch descriptor");
	
	pthread_mutex_lock(&(reader.state_lock));
	while (!(reader.stop)) {
		pthread_mutex_unlock(&(reader.state_lock));

		ret = poll(fds, fds_num, -1);
		if (ret < 0) {
			if (errno != EINTR) {
				ret = -errno;
				printf("Error: Cannot poll fds: %d\n", ret);
				break;
			}
		}

		ret = xwii_iface_dispatch(iface, &event, sizeof(event));
		if (ret != false) {
			if (ret != -EAGAIN) {
				printf("Error: Read failed with err:%d\n", ret);
			}
		}
		else {
			switch (event.type) {
				case XWII_EVENT_BALANCE_BOARD:
					handle_event(&event);
					break;
				case XWII_EVENT_GONE:
					puts("Info: Device gone");
					fds[1].fd = -1;
					fds[1].events = 0;
					fds_num = 1;
					break;
				case XWII_EVENT_WATCH:
					ret = xwii_iface_open(iface, xwii_iface_available(iface) | XWII_IFACE_WRITABLE);
					if (ret)
						puts("Error: Cannot open interface: %d");
					break;
				case XWII_EVENT_KEY:
				case XWII_EVENT_ACCEL:
				case XWII_EVENT_IR:
				case XWII_EVENT_MOTION_PLUS:
				case XWII_EVENT_NUNCHUK_KEY:
				case XWII_EVENT_NUNCHUK_MOVE:
				case XWII_EVENT_CLASSIC_CONTROLLER_KEY:
				case XWII_EVENT_CLASSIC_CONTROLLER_MOVE:
				case XWII_EVENT_PRO_CONTROLLER_KEY:
				case XWII_EVENT_PRO_CONTROLLER_MOVE:
				case XWII_EVENT_GUITAR_KEY:
				case XWII_EVENT_GUITAR_MOVE:
				case XWII_EVENT_DRUMS_KEY:
				case XWII_EVENT_DRUMS_MOVE:
					puts("Eventi vari non gestiti");
					break;
				default:
					puts("Default");
			}
		}
        pthread_mutex_lock(&(reader.state_lock));
	}
	reader.run = false;
	return 0;
}

int read_file() {  // FIXME
	int file = open(arg.file_name, O_RDONLY);
	if (file <= 0) {
		puts("Error opening file");
		return -1;
	}
	/*
		while (!reader.stop && read(file, &bb, sizeof(bb)) ) {
		arg.bb_even_handler(&bb);
	*/
	usleep(REFRESH_TIME);
	//}
	puts("EOF");
	//close(file);
	return 0;
}

void reader_f(void* args) {
	if (arg.file_name == NULL)
		read_board();
	else
		read_file();
	reader.run = false;
	reader.stop = false;
	puts("Info: Close reader thread!");
	pthread_exit(NULL);
}

struct xwii_iface * balance_board() {
	struct xwii_monitor *mon;
	struct xwii_iface *dev;
	char *ent, *str;

	mon = xwii_monitor_new(false, false);
	if (!mon) {
		printf("Cannot create monitor\n");
		return NULL;
	}

	while ((ent = xwii_monitor_poll(mon))) {
		if (xwii_iface_new(&dev, ent))
			puts("Error: Can't create a new device object");
		if (xwii_iface_get_devtype(dev, &str) != 0)
			puts("Error: Reading the current device-type");

		if (strcmp(str, "balanceboard") == 0) {
			free(str);
			free(ent);
			xwii_monitor_unref(mon);
			puts("INFO : Trovata");
			return dev;
		}
		free(str);
		free(ent);
		free(dev);
	}
	xwii_monitor_unref(mon);
	return NULL;
}

void init_pcfit(struct Argument argument, void (*handler_fun)(float *)) {
	gui_callback = handler_fun;
	memset(&reader, 0, sizeof(struct Thread));
	int ret = pthread_mutex_init(&reader.state_lock, NULL);
	if (ret != 0) {
		puts("Error init mutex");
		exit(-1);
	}
	memcpy(&arg, &argument, sizeof(struct Argument));
}

int start_pcfit() {
	if(is_running(reader)) {
		wait_stop(reader);
	}
	iface = balance_board();
	if (iface != NULL) {
		alloc_thread(&reader, reader_f, "Reader");
		return 0;
	}
	return -1;
}

void close_lib() {
	wait_stop(reader);
	pthread_mutex_destroy(&point_lock);
	pthread_mutex_destroy(&(reader.state_lock));
}
