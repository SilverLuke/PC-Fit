#include <stdio.h>
#include <getopt.h>
#include <ctype.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>

#include "config.h"
#include "lib/tomlc99/toml.h"
#include "mytime.h"

int parse_config(struct Configuration * configuration) {
	struct Configuration config = *configuration;
	FILE* fp;
	toml_table_t* conf;
	toml_table_t* notification;
	const char* raw;
	char errbuf[200];

	/* open file and parse */
	if (0 == (fp = fopen(CONFIG_FILE, "rw"))) {
		puts("CONF FILE MISSING");
		return -1;
	}
	conf = toml_parse_file(fp, errbuf, sizeof(errbuf));
	fclose(fp);
	if (0 == conf) {
		return -1;
	}

	/* locate the [notification] table */
	if (0 == (notification = toml_table_in(conf, "notification"))) {
		return -1;
	}

	if (0 == (raw = toml_raw_in(notification, "enable"))) {
		return -1;
	}
	if (toml_rtob(raw, &(config.enable))) {
		return -1;
	}
	// Extract the last measurement
	if (0 == (raw = toml_raw_in(notification, "last"))) {
		return -1;
	}
	if (toml_rtoi(raw, &(config.last))) {
		return -1;
	}
	// Extract the the frequency
	if (0 == (raw = toml_raw_in(notification, "every"))) {
		return -1;
	}
	if (toml_rtoi(raw, &(config.last))) {
		return -1;
	}

	/* done with conf */
	toml_free(conf);

	if (config.enable && is_elapsed(config))
		return 1;
	return 0;
}

struct Argument parse_arg(int argc, char **argv) {
	struct Argument arg;
	arg.file_name = NULL;
	arg.file_name_len = 0;
	arg.reminder = 0;

	int c;
	opterr = 0;
	while((c = getopt (argc, argv, "f:")) != -1)
		switch(c) {
			case 'f':
				arg.file_name_len = strlen(optarg);
				memcpy(&(arg.file_name), &optarg, arg.file_name_len);
				break;
			case 'n':
				arg.reminder = 1;
				break;
			case '?':
				if (optopt == 'f')
					fprintf(stderr, "Option -%c requires arg file path.\n", optopt);
				else if (isprint (optopt))
					fprintf(stderr, "Unknown option `-%c'.\n", optopt);
				else
					fprintf(stderr, "Unknown option character `\\x%x'.\n", optopt);
				abort();
			default:
				abort();
		}
	printf ("arg.file = %s\n", arg.file_name);
	return arg;
}