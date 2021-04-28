#include <stdio.h>
#include <ctype.h>
#include <unistd.h>
#include <stdlib.h>
#include <unistd.h>
#include <assert.h>
#include <stdbool.h>
#include <pthread.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <math.h>
#include <errno.h>
#include <sys/time.h>

#include <gtk/gtk.h>

#include "pc-fit.h"
#include "mytime.h"
#include "config.h"

#define GUI_FILE    "res/gui/interface.ui"
#define BB_IMAGE    "res/images/bb.png"

#define SCALE(r, g, b, a) r/255, g/255, b/255, a/255

#define RADIUS           10
#define MAX_MAGNITUDE    10.
#define MIN_WEIGHT       10.
#define SAMPLING_TIME    5.
#define SHOW_WEIGHT_TIME 3.
#define NEXT_SCAN        3  // Seconds between bluetooth scans (The OS scans for bluetooth device)

struct Image {
	GtkDrawingArea* area;
	GdkPixbuf* pixbuf;
	int x_offset;
	int y_offset;
	int x;
	int y;
};

struct BB_gui {
	struct Image image;
	GtkLabel* weight;
	GtkLabel* top_left;
	GtkLabel* top_right;
	GtkLabel* bottom_left;
	GtkLabel* bottom_right;
};

struct Gui {
	GtkBuilder* builder;
	struct BB_gui bb;
	GtkApplicationWindow* app;
	GtkBox* bb_not_found;
	GtkBox* bb_found;
	GtkLabel* time;
	struct Configuration config;
} gui;

char* get_resource(char* relative_pos) {
	int len = sizeof(char) * PATH_MAX;
	char* cwd = malloc(len);
	if (getcwd(cwd, len) != NULL) {
		for (int i = strlen(cwd); i >= 0; i--) {
			if (cwd[i] == '/') {
				cwd[i+1] = 0;
				break;
			}
		}
		strcat(cwd, relative_pos);
	} else {
		perror("getcwd() error");
		exit(1);
	}
	return cwd;
}

void set_text(GtkLabel* l, float val) {
	char str[32];
	snprintf(str, 32, "%.2f Kg", val);
	gtk_label_set_text(l, str);
}

void update_cells_text(float cells[]) {
	set_text(gui.bb.top_left, cells[TL]);
	set_text(gui.bb.top_right, cells[TR]);
	set_text(gui.bb.bottom_left, cells[BL]);
	set_text(gui.bb.bottom_right, cells[BR]);
}

static gboolean update_text(gpointer userdata) {
	float* cells = (float*) userdata;
	float kg = get_weight(cells);
	set_text(gui.bb.weight, kg);
	update_cells_text(cells);
	free(cells);
	return G_SOURCE_REMOVE;
}

static gboolean show_weight(gpointer userdata) {
	float kg = *((float*) userdata);
	set_text(gui.bb.weight, kg);
	free(userdata);
	return G_SOURCE_REMOVE;
}

gboolean draw_callback(GtkWidget* w, cairo_t* cr, gpointer data) {
	int x, y;
	get_point(&x, &y);

	gdk_cairo_set_source_pixbuf(cr, gui.bb.image.pixbuf, gui.bb.image.x_offset, gui.bb.image.y_offset);
	cairo_paint(cr);
	// Green dot
	cairo_pattern_t* radpat = cairo_pattern_create_radial(x, y, 2, x, y, RADIUS);
	cairo_pattern_add_color_stop_rgba(radpat, 1, SCALE(40, 255, 40, 0));
	cairo_pattern_add_color_stop_rgba(radpat, 0, SCALE(40, 255, 40, 255));

	cairo_set_source(cr, radpat);
	cairo_arc(cr, x, y, RADIUS, 0, 2 * G_PI);
	cairo_fill(cr);

	cairo_pattern_destroy(radpat);

	return FALSE;
}

void set_point_coord(float center[]) {
	float ratiox = gui.bb.image.x / TOTAL_WEIGHT;
	float ratioy = gui.bb.image.y / TOTAL_WEIGHT;
	int pointx = (center[X] * ratiox) + gui.bb.image.x / 2. + gui.bb.image.x_offset;
	int pointy = (-center[Y] * ratioy) + gui.bb.image.y / 2. + gui.bb.image.y_offset;

	set_point(pointx, pointy);

	return;
}

struct timeval sampling_time, show_weight_time;

struct Samples {
	int sample;
	float total_weight;
};

int sec_expired = NEXT_SCAN;

void add_sample(struct Samples* s, float weight) {
	s->total_weight += weight;
	s->sample++;
}

void reset_sampling(struct Samples* s) {
	*s = (struct Samples) {0};
}

bool in_range(float magnitude, float weight) {
	return magnitude < MAX_MAGNITUDE && weight > MIN_WEIGHT;
}

void handle_bb_event(float* cells) {
	static struct Samples samp;
	static int state = 0;
	float center[2];
	float weight = get_weight(cells);
	center_of_gravity(cells, center);

	set_point_coord(center);

	float* tmp = malloc(sizeof(float) * 4);
	memcpy(tmp, cells, sizeof(float) * 4);

	switch (state) {
		case 0:  // First value and set reset timer
			if (in_range(hypot(center[0], center[1]), weight)) {
				set_time(&sampling_time);
				reset_sampling(&samp);
				add_sample(&samp, weight);
				state++;
			}
			g_main_context_invoke(NULL, update_text, tmp);
			break;
		case 1:  // add values
			if (in_range(hypot(center[0], center[1]), weight)) {
				if (elapsed_time(sampling_time) >= SAMPLING_TIME) {  // sampling time is expired
					add_sample(&samp, weight);

					float* tmp2 = malloc(sizeof(float));
					*tmp2 = samp.total_weight / samp.sample;
					g_main_context_invoke(NULL, show_weight, tmp2);

					printf("Info: weight: %f (%f / %i) Sample per Sec:%f\n",
						   *tmp2, samp.total_weight, samp.sample, samp.sample / SAMPLING_TIME);

					set_time(&show_weight_time);
					state++;
				} else {
					add_sample(&samp, weight);
					g_main_context_invoke(NULL, update_text, tmp);
				}
			} else {  // Baricentro uscito da l'area
				state = 0;
			}
			break;
		case 2:  // Showing weight
			if (elapsed_time(show_weight_time) >= SHOW_WEIGHT_TIME) {  // Stop
				state = 0;
			}
	}

	gdk_threads_add_idle((GSourceFunc) gtk_widget_queue_draw, (void*) gui.bb.image.area);
}

void main_quit() {
	puts("QUITTING...");
	close_lib();
	update_last_config(gui.config);
	write_config(gui.config);
	gtk_main_quit();
}

void bb_instance() {
	g_signal_connect(G_OBJECT(gui.bb.image.area), "draw", G_CALLBACK(draw_callback), NULL);
	GtkAllocation widget;
	gtk_widget_get_allocation(GTK_WIDGET(gui.bb.image.area), &widget);

	GError* err = NULL;
	char * image_res = get_resource(BB_IMAGE);
	gui.bb.image.pixbuf = gdk_pixbuf_new_from_file_at_scale(image_res, widget.width, widget.height, TRUE, &err);
	free(image_res);
	if (err) {
		printf("Error : %s\n", err->message);
		g_error_free(err);
		main_quit();
	}

	gui.bb.image.x = gdk_pixbuf_get_width(gui.bb.image.pixbuf);
	gui.bb.image.x_offset = (widget.width - gui.bb.image.x) / 2;
	gui.bb.image.y = gdk_pixbuf_get_height(gui.bb.image.pixbuf);
	gui.bb.image.y_offset = (widget.height - gui.bb.image.y) / 2;

	int pointx = gui.bb.image.x / 2. + gui.bb.image.x_offset;
	int pointy = gui.bb.image.y / 2. + gui.bb.image.y_offset;
	set_point(pointx, pointy);

	printf("Info: Widget (%d, %d) Image (%d x %d) offset (%d %d).\n",
		   widget.width,
		   widget.height,
		   gui.bb.image.x,
		   gui.bb.image.y,
		   gui.bb.image.x_offset,
		   gui.bb.image.y_offset);

}

int gui_bb_start() {
	puts("START BB INTERFACE");

	gtk_container_remove(GTK_CONTAINER(gui.app), GTK_WIDGET(gui.bb_not_found));
	gtk_container_add(GTK_CONTAINER(gui.app), GTK_WIDGET(gui.bb_found));

	g_signal_connect((gui.bb.image.area), "size-allocate", G_CALLBACK(bb_instance), NULL);
	return 0;
}

static gboolean next_scan_update(gpointer data) {
	GtkLabel* label = (GtkLabel*) data;
	char buf[256];
	memset(&buf, 0x0, 256);
	snprintf(buf, 255, "Next scan in %d secs", --sec_expired);
	if (sec_expired <= 0) {
		if (pcfit_start() > 0) {
			gui_bb_start();
			return FALSE;  // Stop the timer
		} else {
			sec_expired = NEXT_SCAN;
		}
	}
	gtk_label_set_label(label, buf);
	return TRUE;
}

void gui_start() {
	puts("STARTING GUI");

	int bb_connected = pcfit_start();
	if (bb_connected > 0) {
		gui_bb_start();
	} else {
		next_scan_update(gui.time);
		g_timeout_add_seconds(1, next_scan_update, gui.time);
	}

	gtk_main();
}

void open_settings() {
	puts("TODO");
}

void gui_init(struct Configuration config) {
	gtk_init(NULL, NULL);
	gui.config = config;
	char* builder_file = get_resource(GUI_FILE);
	gui.builder = gtk_builder_new_from_file(builder_file);
	free(builder_file);

	gui.app = GTK_APPLICATION_WINDOW(gtk_builder_get_object(gui.builder, "container_window"));
	g_signal_connect(gui.app, "destroy", G_CALLBACK(main_quit), NULL);

	GObject* settings_btn = gtk_builder_get_object(gui.builder, "settings-btn");
	g_signal_connect(settings_btn, "clicked", G_CALLBACK(open_settings), NULL);

	gui.bb_not_found = GTK_BOX(gtk_builder_get_object(gui.builder, "bb_not_found_box"));
	gui.bb_found = GTK_BOX(gtk_builder_get_object(gui.builder, "bb_found_box"));

	gui.bb.image.area = GTK_DRAWING_AREA(gtk_builder_get_object(gui.builder, "area"));
	gui.bb.weight = GTK_LABEL(gtk_builder_get_object(gui.builder, "weight"));
	gui.bb.top_left = GTK_LABEL(gtk_builder_get_object(gui.builder, "top-left"));
	gui.bb.top_right = GTK_LABEL(gtk_builder_get_object(gui.builder, "top-right"));
	gui.bb.bottom_left = GTK_LABEL(gtk_builder_get_object(gui.builder, "bottom-left"));
	gui.bb.bottom_right = GTK_LABEL(gtk_builder_get_object(gui.builder, "bottom-right"));

	gui.time = GTK_LABEL(gtk_builder_get_object(gui.builder, "next_scan_label"));

	puts("INIT GUI");
}
