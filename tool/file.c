#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <time.h>
#include <stdlib.h>
#include <limits.h>
#include <unistd.h>
#include <inttypes.h>
#include <errno.h>

#include <wiiuse.h>

#define M(i) i * 1000000

#define RAND_SIZE	M(1)

#define DIV			17
#define MAX_WEIGHT	34.
#define STEP		MAX_WEIGHT / DIV


#define MAX_WIIMOTES 1
struct WM {
	wiimote ** wiimotes;
	int connected;
};

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

void handle_event(struct wiimote_t* wm, int file) {
	if (wm->exp.type == EXP_WII_BOARD) {
		struct wii_board_t bb = *(wii_board_t*)&wm->exp.wb;
		write(file, &bb, sizeof(struct wii_board_t));
	}
}

void record_data(int file) {
	struct WM device;
	device.wiimotes =  wiiuse_init(MAX_WIIMOTES);
	int found = wiiuse_find(device.wiimotes, MAX_WIIMOTES, 5);
	if (!found) {
		printf("No wiimotes found.\n");
		return;
	}
	device.connected = wiiuse_connect(device.wiimotes, MAX_WIIMOTES);
	if (device.connected) {
		printf("Connected to %i wiimotes (of %i found).\n", device.connected, found);
	}
	else
		printf("Failed to connect to any wiimote.\n");

	while (any_wiimote_connected(device.wiimotes, MAX_WIIMOTES)) {
		if (wiiuse_poll(device.wiimotes, MAX_WIIMOTES)) {
			for (int i = 0; i < MAX_WIIMOTES; ++i) {
				switch (device.wiimotes[i]->event) {
					case WIIUSE_EVENT:
						handle_event(device.wiimotes[i], file);
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
	return;
}

void random_data(int file) {
	struct wii_board_t bb = {0};
	srand(time(NULL));
	for (int i = 0; i < RAND_SIZE; i++) {
		bb.tl = ((float)rand()/(float)(RAND_MAX)) * MAX_WEIGHT;
		bb.tr = ((float)rand()/(float)(RAND_MAX)) * MAX_WEIGHT;
		bb.bl = ((float)rand()/(float)(RAND_MAX)) * MAX_WEIGHT;
		bb.br = ((float)rand()/(float)(RAND_MAX)) * MAX_WEIGHT;
		write(file, &bb, sizeof(bb));
	}
	return;
}

void deterministic_data(int file) {
	printf("INC: %.3f\n", STEP);
	struct wii_board_t bb = {0};
	for (float tl = 0; tl < MAX_WEIGHT; tl += STEP ) {
		for (float tr = 0; tr < MAX_WEIGHT; tr += STEP ) {
			for (float bl = 0; bl < MAX_WEIGHT; bl += STEP ) {
				for (float br = 0; br < MAX_WEIGHT; br += STEP ) {
					bb.tl = tl;
					bb.tr = tr;
					bb.bl = bl;
					bb.br = br;
					write(file, &bb, sizeof(bb));
				}
			}
		}
	}
	return;
}

int main(int argc, char **argv) {
	int file = open("test.bin", O_RDWR | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP);
	if (file < 0) {
		puts("Error opening file");
	}
	if (argc == 2 && strcmp(argv[1], "rec") == 0) {
		puts("Record data");
		record_data(file);
	}
	if (argc == 2 && strcmp(argv[1], "det") == 0) {
		puts("Deterministic data");
		deterministic_data(file);
	}
	else if (argc == 2 && strcmp(argv[1], "ran") == 0) {
		puts("Random data");
		random_data(file);
	}
	else {
		printf("Use %s <rec|det|ran>.\n", argv[0]);
		puts("rec : record some data from a balance board");
		puts("det : data with a sense");
		puts("ran : data are random");
	}
	close(file);
	return 0;
}
