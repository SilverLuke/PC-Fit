/*
 * bb.c
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
 *
 * http://public.vrac.iastate.edu/vancegroup/docs/wiiuse/de/d67/structwiimote.html
 * http://www.macs.hw.ac.uk/~ruth/year4VEs/Labs/wiiuse.html
 * g_free();
 */


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

#define RADIUS			10
#define GUI_FILE		"../gui.glade"
#define IMAGE			"../bb.png"

#define MAX_WIIMOTES	1
#define TIMEOUT			5

#define	MAX_WEIGHT		34.
#define TOTAL_WEIGHT	(MAX_WEIGHT * 4.)  // 34 max Kg multiplied each load cell

#define FPS				1000
#define MILLI			(1. / FPS)
#define SEC				(1000 * 1000)
#define REFRESH_TIME	(MILLI * SEC)


#define TESTING			TRUE
#define TESTING_FILE	"../test.bin"

#define SCALE(r, g, b, a) r/255, g/255, b/255, a/255

struct WM {
	wiimote ** wiimotes;
	int connected;
} device;

struct Image {
	GtkDrawingArea * area;
	GdkPixbuf * pixbuf;
	int x;
	int y;
};

struct Gui {
	struct Image image;
	GtkLabel* weight;
	GtkLabel* top_left;
	GtkLabel* top_right;
	GtkLabel* bottom_left;
	GtkLabel* bottom_right;
} gui;

struct Point {
	int x;
	int y;
} point;

struct Thread {
	pthread_t t;
	bool run;
	bool stop;
}
reader = { .run = FALSE, .stop = FALSE},
finder = { .run = FALSE, .stop = FALSE};

pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;

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

void set_point_coord(struct wii_board_t * bb) {
	float massx = (bb->tr + bb->br) - (bb->tl + bb->bl);
	float massy = (bb->tl + bb->tr) - (bb->bl + bb->br);
	float ratx = gui.image.x / TOTAL_WEIGHT;
	float raty = gui.image.y / TOTAL_WEIGHT;

	float x =  (massx * ratx) + gui.image.x / 2.;
	float y = (-massy * raty) + gui.image.y / 2.;

	//printf("X:%.0f Y:%.0f\tMX:%.2f MY:%.2f\t" \
		"tl/tr/bl/br %.2f / %.2f / %.2f / %.2f\n", x, y, massx, massy, bb->tl, bb->tr, bb->bl, bb->br);
	pthread_mutex_lock(&lock);
	point.x = x;
	point.y = y;
	pthread_mutex_unlock(&lock);
	return;
}

void set_text(GtkLabel* l, float val) {
	char str[32];
	snprintf(str, 32, "%.2f Kg", val);
	gtk_label_set_text(l, str);
}

static gboolean update_text (gpointer userdata) {
	struct wii_board_t* wb = (wii_board_t*) userdata;
	float kg = wb->tl + wb->tr + wb->bl + wb->br;
	set_text(gui.weight, kg);
	set_text(gui.top_left, wb->tl);
	set_text(gui.top_right, wb->tr);
	set_text(gui.bottom_left, wb->bl);
	set_text(gui.bottom_right, wb->br);
	return G_SOURCE_REMOVE;
}

gboolean draw_callback(GtkWidget * w, cairo_t *cr, gpointer data) {
	gdk_cairo_set_source_pixbuf(cr, gui.image.pixbuf, 0, 0);
	cairo_paint(cr);

	int x, y;
	pthread_mutex_lock(&lock);
	x = point.x;
	y = point.y;
	pthread_mutex_unlock(&lock);

	cairo_pattern_t * radpat = cairo_pattern_create_radial (x, y, 2, x, y, RADIUS);
	cairo_pattern_add_color_stop_rgba (radpat, 1, SCALE(40, 255, 40, 0));
	cairo_pattern_add_color_stop_rgba (radpat, 0, SCALE(40, 255, 40, 255));

	cairo_set_source (cr, radpat);
	cairo_arc (cr, x, y, RADIUS, 0, 2 * G_PI);
	cairo_fill(cr);

	cairo_pattern_destroy(radpat);
	return FALSE;
}

void update_data(struct wii_board_t * bb) {
	g_main_context_invoke (NULL, update_text, bb);
	set_point_coord(bb);
	gdk_threads_add_idle((GSourceFunc)gtk_widget_queue_draw,(void*) gui.image.area);
}

void handle_event(struct wiimote_t* wm) {
	if (wm->exp.type == EXP_WII_BOARD) {
		struct wii_board_t* bb = (wii_board_t*)&wm->exp.wb;
		update_data(bb);
	}
}

void alloc_thread(struct Thread * thread, void* func) {
	if (thread->run == FALSE) {
		pthread_create(&(thread)->t, NULL, func, NULL);
		pthread_detach(thread->t);
		thread->run = TRUE;
		thread->stop = FALSE;
		puts("Thread started");
	}
	else {
		puts("Thread is already started");
	}
}

void reader_f(void* args) {
	#ifndef TESTING
	while (!reader.stop && any_wiimote_connected(device.wiimotes, MAX_WIIMOTES)) {
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

	#else

	int file = open(TESTING_FILE, O_RDONLY);
	if (file < 0) {
		puts("Error opening file");
	}
	struct wii_board_t bb;
	while (!reader.stop && read(file, &bb, sizeof(bb)) ) {
		update_data(&bb);
		usleep(REFRESH_TIME);
	}
	puts("EOF\n");
	close(file);

	#endif

	reader.run = FALSE;
	reader.stop = FALSE;
	printf("Close reader thread!\n");
	pthread_exit(NULL);
}

void* finder_f(void* args) {
	int found;
	device.wiimotes =  wiiuse_init(MAX_WIIMOTES);
	found = wiiuse_find(device.wiimotes, MAX_WIIMOTES, TIMEOUT);
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
	#ifndef TESTING
	alloc_thread(&finder, finder_f);
	#else
	alloc_thread(&reader, reader_f);
	#endif
}

void main_quit() {
	finder.stop = TRUE;
	reader.stop = TRUE;
	gtk_main_quit();
	pthread_mutex_destroy(&lock);
	exit(0);
}

int main(int argc, char **argv) {
	gtk_init(&argc, &argv);
	GtkBuilder* builder = gtk_builder_new_from_file (GUI_FILE);

	GObject* window = gtk_builder_get_object(builder, "window");
	g_signal_connect(window, "destroy", G_CALLBACK (main_quit), NULL);

	GObject* find_btn = gtk_builder_get_object (builder, "find-btn");
	g_signal_connect(find_btn, "clicked", G_CALLBACK (find_balance_board), NULL);

	gui.image.area   = GTK_DRAWING_AREA(gtk_builder_get_object(builder, "area"));
	gui.top_left     = GTK_LABEL(gtk_builder_get_object(builder, "top-left"));
	gui.top_right    = GTK_LABEL(gtk_builder_get_object(builder, "top-right"));
	gui.bottom_left  = GTK_LABEL(gtk_builder_get_object(builder, "bottom-left"));
	gui.bottom_right = GTK_LABEL(gtk_builder_get_object(builder, "bottom-right"));
	gui.weight       = GTK_LABEL(gtk_builder_get_object(builder, "weight"));

	g_signal_connect(G_OBJECT(gui.image.area), "draw", G_CALLBACK(draw_callback), NULL);

	GtkAllocation widget;
	gtk_widget_get_allocation(GTK_WIDGET(gui.image.area), &widget);

	GError *err = NULL;
	gui.image.pixbuf = gdk_pixbuf_new_from_file_at_scale(IMAGE, widget.width, widget.height, TRUE, &err);
	if (err) {
		printf("Error : %s\n", err->message);
		g_error_free(err);
		return FALSE;
	}

	gui.image.x = gdk_pixbuf_get_width(gui.image.pixbuf);
	gui.image.y = gdk_pixbuf_get_height(gui.image.pixbuf);

	printf("Image size (%d x %d)\n", gui.image.x, gui.image.y);
	point.x = gui.image.x / 2 + 2;
	point.y = gui.image.y / 2 - 2;

	gtk_main();
	return 0;
}
