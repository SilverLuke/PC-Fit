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

#include <wiiuse.h>
#include <gtk/gtk.h>
#include "pc-fit.h"

#define GUI_FILE "../../gui/interface.ui"
#define IMAGE    "../../gui/bb.png"

#define RADIUS 10

#define MAX_MAGNITUDE    10.
#define STATIONARY_TIME  5.
#define SHOW_WEIGHT_TIME 3.

#define SCALE(r, g, b, a) r/255, g/255, b/255, a/255

/*
 * http://public.vrac.iastate.edu/vancegroup/docs/wiiuse/de/d67/structwiimote.html
 * http://www.macs.hw.ac.uk/~ruth/year4VEs/Labs/wiiuse.html
 */

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

	gdk_cairo_set_source_pixbuf(cr, gui.image.pixbuf, 0, 0);
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
	int pointx =  (center[X] * ratiox) + gui.image.x / 2.;
	int pointy = (-center[Y] * ratioy) + gui.image.y / 2.;

	set_point(pointx, pointy);

	return;
}

static struct Times {
	struct timeval start;
	struct timeval now;
} weight_time, show_weight_time;

static int sample = 0;
static float average = 0;

void set_time(struct timeval * now) {
	if(gettimeofday(now, NULL) != 0)
		printf("gettimeofday() failed, errno = %d\n", errno);
}

double get_sec(struct timeval x) {
	return ((double)x.tv_sec * 1000000 + (double)x.tv_usec) / 1000000;
}

double time_diff(struct Times x) {
	return get_sec(x.now) - get_sec(x.start) ;
}

void reset(struct Times * x) {
	memset(x, 0, sizeof(struct Times));
}

void add_sample(float cells[]) {
	average += get_weight(cells);
	sample++;
}

float check_distance(float center[], float cells[]) {
	float magnitude = hypot(center[0], center[1]);

	if (magnitude < MAX_MAGNITUDE) {
		if (get_sec(weight_time.start) == 0) {
			set_time(&(weight_time.start));
			set_time(&(weight_time.now));
		}
		else if (time_diff(weight_time) >= STATIONARY_TIME ) {
			add_sample(cells);
			float tmp = average / sample;
			printf("%f , %i, SPS:%f\n", average, sample, sample/STATIONARY_TIME);
			reset(&weight_time);
			return tmp;
		}
		else {
			set_time(&weight_time.now);
			add_sample(cells);
		}
	}
	else {
		reset(&weight_time);
		average = sample = 0;
	}
	return 0;
}

void event(float cells[]) {
	float center[2];
	center_of_gravity(cells, center);

	set_point_coord(center);

	float * tmp = malloc(sizeof(float) * 4);
	memcpy(tmp, cells, sizeof(float) * 4);

	float weight = 0;
	if (get_sec(show_weight_time.start) == 0) {
		if ( (weight = check_distance(center, cells)) != 0 ) {
			set_time(&show_weight_time.start);
			set_time(&show_weight_time.now);
			float *tmp2 = malloc(sizeof(float));
			tmp2[0] = weight;
			g_main_context_invoke (NULL, show_weight, tmp2);
			printf("weight:%f\n", weight);
		}
		else {
			g_main_context_invoke (NULL, update_text, tmp);
			gdk_threads_add_idle((GSourceFunc)gtk_widget_queue_draw,(void*) gui.image.area);
		}
	}
	else if (time_diff(show_weight_time) >= SHOW_WEIGHT_TIME) {
		reset(&show_weight_time);
	}
	else {
		set_time(&show_weight_time.now);
	}
}

struct ARG getarg(int argc, char **argv) {
	struct ARG a;
	a.file = NULL;
	a.notify = *event;
	int c;
	opterr = 0;
	while((c = getopt (argc, argv, "f:")) != -1)
		switch(c) {
			case 'f':
				a.file = optarg;
				break;
			case '?':
				if (optopt == 'f')
					fprintf(stderr, "Option -%c requires a file path.\n", optopt);
				else if (isprint (optopt))
					fprintf(stderr, "Unknown option `-%c'.\n", optopt);
				else
					fprintf(stderr, "Unknown option character `\\x%x'.\n", optopt);
				abort();
			default:
				abort();
	}
	printf ("arg.file = %s\n", a.file);
	return a;
}

void main_quit() {
	close_lib();
	gtk_main_quit();
	exit(0);
}

int main(int argc, char **argv) {
	struct ARG arg = getarg(argc, argv);

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

	set_point(-RADIUS, -RADIUS);

	init_pcfit(arg);

	gtk_main();
	return 0;
}
