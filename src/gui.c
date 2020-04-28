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

#define GUI_DIR  "/dati/luca/Progetti/pc-fit/data/"
#define GUI_FILE GUI_DIR "interface.ui"
#define BB_IMAGE    GUI_DIR "bb.png"

#define SCALE(r, g, b, a) r/255, g/255, b/255, a/255

#define RADIUS 10
#define MAX_MAGNITUDE	10.
#define MIN_WEIGHT		10.
#define SAMPLING_TIME		5.
#define SHOW_WEIGHT_TIME	3.

struct Image {
	GtkDrawingArea * area;
	GdkPixbuf * pixbuf;
	int x_offset;
	int y_offset;
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

void set_text(GtkLabel* l, float val) {
	char str[32];
	snprintf(str, 32, "%.2f Kg", val);
	gtk_label_set_text(l, str);
}

void update_cells_text (float cells[]) {
	set_text(gui.top_left,     cells[TL]);
	set_text(gui.top_right,    cells[TR]);
	set_text(gui.bottom_left,  cells[BL]);
	set_text(gui.bottom_right, cells[BR]);
}

static gboolean update_text (gpointer userdata) {
	float *cells = (float*) userdata;
	float kg = get_weight(cells);
	set_text(gui.weight, kg);
	update_cells_text(cells);
	free(cells);
	return G_SOURCE_REMOVE;
}

static gboolean show_weight (gpointer userdata) {
	float kg = *((float*) userdata);
	set_text(gui.weight, kg);
	free(userdata);
	return G_SOURCE_REMOVE;
}

gboolean draw_callback(GtkWidget * w, cairo_t *cr, gpointer data) {
	int x, y;
	get_point(&x, &y);

	gdk_cairo_set_source_pixbuf(cr, gui.image.pixbuf, gui.image.x_offset, gui.image.y_offset);
	cairo_paint(cr);
	// Green dot
	cairo_pattern_t * radpat = cairo_pattern_create_radial (x, y, 2, x, y, RADIUS);
	cairo_pattern_add_color_stop_rgba (radpat, 1, SCALE(40, 255, 40, 0));
	cairo_pattern_add_color_stop_rgba (radpat, 0, SCALE(40, 255, 40, 255));

	cairo_set_source (cr, radpat);
	cairo_arc (cr, x, y, RADIUS, 0, 2 * G_PI);
	cairo_fill(cr);

	cairo_pattern_destroy(radpat);

	return FALSE;
}

void set_point_coord(float center[]) {
	float ratiox = gui.image.x / TOTAL_WEIGHT;
	float ratioy = gui.image.y / TOTAL_WEIGHT;
	int pointx =  (center[X] * ratiox) + gui.image.x / 2. + gui.image.x_offset;
	int pointy = (-center[Y] * ratioy) + gui.image.y / 2. + gui.image.y_offset;

	set_point(pointx, pointy);

	return;
}

struct timeval sampling_time, show_weight_time;

struct Samples {
	int sample;
	float total_weight;
	float weight;
};

void add_sample(struct Samples *s, float weight) {
	s->total_weight += weight;
	s->sample++;
}

void reset_sampling(struct Samples *s) {
	*s = (struct Samples) {0};
	//memset(&s, 0, sizeof(struct Samples));
}

bool in_range(float magnitude, float weight) {
	return magnitude < MAX_MAGNITUDE && weight > MIN_WEIGHT;
}

void handle_bb_event(float *cells) {
	static struct Samples samp;
	static int state = 0;
	float center[2];
	float weight = get_weight(cells);
	center_of_gravity(cells, center);

	set_point_coord(center);

	float * tmp = malloc(sizeof(float) * 4);
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
				}
				else {
					add_sample(&samp, weight);
					g_main_context_invoke (NULL, update_text, tmp);
				}
			}
			else {  // Baricentro uscito da l'area
				state = 0;
			}
			break;
		case 2:  // Showing weight
			if (elapsed_time(show_weight_time) >= SHOW_WEIGHT_TIME) {  // Stop
				state = 0;
			}
	}

	gdk_threads_add_idle((GSourceFunc)gtk_widget_queue_draw,(void*) gui.image.area);
}


void device_list_show() {  // FIXME
	puts("Info: Button pressed.");
	start_pcfit();
}


void main_quit() {
	close_lib();
	gtk_main_quit();
	exit(0);
}

int gui_init() {
	GtkBuilder* builder = gtk_builder_new_from_file (GUI_FILE);

	GObject* window = gtk_builder_get_object(builder, "window");
	g_signal_connect(window, "destroy", G_CALLBACK (main_quit), NULL);

	GObject* find_btn = gtk_builder_get_object (builder, "devices-btn");
	g_signal_connect(find_btn, "clicked", G_CALLBACK (device_list_show), NULL);

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
	gui.image.pixbuf = gdk_pixbuf_new_from_file_at_scale(BB_IMAGE, widget.width, widget.height, TRUE, &err);
	if (err) {
		printf("Error : %s\n", err->message);
		g_error_free(err);
		return -1;
	}

	gui.image.x = gdk_pixbuf_get_width(gui.image.pixbuf);
	gui.image.x_offset = (widget.width - gui.image.x) / 2;
	gui.image.y = gdk_pixbuf_get_height(gui.image.pixbuf);
	gui.image.y_offset = (widget.height - gui.image.y) / 2;

	printf("Info: Widget (%d, %d) Image (%d x %d) offset (%d %d).\n", widget.width, widget.height, gui.image.x, gui.image.y, gui.image.x_offset, gui.image.y_offset);

	set_point(-RADIUS, -RADIUS);
	return 0;
}