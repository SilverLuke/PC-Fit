/*
 * file.c
 *
 * Copyright 2017 Argentieri Luca <luca.argentieri@openmaibox.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 * MA 02110-1301, USA.
 *
 *
 */


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


void record_data(int file) {
	puts("TRIGGERED");
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
	else if (argc == 1) {
		puts("Random data");
		random_data(file);
	}
	else {
		printf("Wrong argument %s\n", argv[1]);
	}
	close(file);
	return 0;
}
