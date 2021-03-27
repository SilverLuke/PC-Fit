#include <stdio.h>
#include <ctype.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdbool.h>

#include "config.h"
#include "pc-fit.h"
#include "gui.h"
#include "mytime.h"

// #define DATA_FILE "~/.local/share/pc-fit/data"
const char* argp_program_version = "PC-Fit v0.1";
const char* argp_program_bug_address = "<your@email.address>";
static char doc[] = "Use the Wii Balance Board as weight scale.";
static char args_doc[] = "Test";
static struct argp_option options[] = {
	{"notify", 'n', 0, 0, "Notify me"},
	{"file", 'f', 0, 0, "File"},
	{0}
};
static struct argp argp = {options, parse_opt, args_doc, doc, 0, 0, 0};

int main(int argc, char** argv) {

	// system("rfkill unblock bluetooth");
	// usleep(1000);

	struct Arguments arg;
	arg.file_name = NULL;
	arg.file_name_len = 0;
	arg.reminder = false;
	argp_parse(&argp, argc, argv, 0, 0, &arg);

	struct Configuration config;
	// Parsing configuration
	if (parse_config(&config) < 0) {
		puts("Error parsing configuration");
		exit(-1);
	}

	if (arg.reminder && config.enable) {
		print_config(config);
		if (check_time(config)) {
			puts("Reminder AND enable AND time");
			goto START;
		} else {
			puts("Time not ready!! Closing");
			return 1;
		}
	} else if (arg.reminder && !config.enable) {
		puts("Not enable in config");
		return 0;
	}

	puts("Direct start");
	START:
	gui_init(config);
	pcfit_init(arg, handle_bb_event);
	gui_start();

	return 0;
}
