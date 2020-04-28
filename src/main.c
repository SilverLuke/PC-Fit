#include <stdio.h>
#include <ctype.h>
#include <unistd.h>
#include <stdlib.h>

#include <gtk/gtk.h>
#include "config.h"
#include "pc-fit.h"
#include "gui.h"
#include "mytime.h"

#define DATA_FILE "~/.local/share/pc-fit/data"

int main(int argc, char **argv) {
	struct Argument arg;
	struct Configuration config;

	arg = parse_arg(argc, argv);
	if(parse_config(&config) != 0) {
		puts("Error parse");
	}


	gtk_init(&argc, &argv);

	gui_init();

	init_pcfit(arg, handle_bb_event);

	if (start_pcfit())
		puts("Error: No balance Board found.");
	else
		puts("INFO : Ok");
	gtk_main();
	return 0;
}
