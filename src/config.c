#include <stdio.h>
#include <ctype.h>
#include <stdlib.h>
#include <getopt.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>

#include "config.h"
#include "mytime.h"
#include "lib/tomlc99/toml.h"

#include <argp.h>
#include <stdbool.h>

error_t parse_opt(int key, char* arg, struct argp_state* state) {
	struct Arguments* arguments = state->input;
	switch (key) {
		case 'n':
			arguments->reminder = true;
			break;
		case 'f':
			arguments->file_name = "nonfugne";
			break;
		case ARGP_KEY_ARG:
			return 0;
		default:
			return ARGP_ERR_UNKNOWN;
	}
	return 0;
}

char* get_config_file_path() {
	/* Home expansion */
	char* homedir = getenv("HOME");
	if (homedir == NULL) {
		puts("$HOME is missing in the enviroment");
		return NULL;
	}
	char len = strlen(homedir) + strlen(CONFIG_FILE) + 1;
	char* file = malloc(sizeof(char) * len);
	if (file == NULL) {
		puts("Malloc error");
		return NULL;
	}

	if (snprintf(file, len, "%s%s", homedir, CONFIG_FILE) < 0) {
		puts("Error snprintf");
		free(file);
		return NULL;
	}
	return file;
}

int parse_config(struct Configuration* config) {
	char* file = get_config_file_path();
	if (file == NULL) {
		return -1;
	}

	FILE* fp = fopen(file, "r");
	if (fp == NULL) {
		puts("Error opening configuration file");
		return -1;
	}

	char errbuf[200];
	toml_table_t* conf = toml_parse_file(fp, errbuf, sizeof(errbuf));
	fclose(fp);
	if (!conf) {
		puts("Error parsing file");
		return -1;
	}

	toml_table_t* notification = toml_table_in(conf, "notification");
	if (!notification) {
		puts("Missing [notification]");
		return -1;
	}

	toml_datum_t enable = toml_bool_in(notification, "enable");
	if (!enable.ok) {
		puts("cannot read notification.enable");
	}
	else {
		config->enable = enable.u.b;
	}

	toml_datum_t last = toml_int_in(notification, "last");
	if (!last.ok) {
		puts("cannot read notification.last");
	}
	else {
		config->last = last.u.i;
	}

	toml_datum_t every = toml_int_in(notification, "every");
	if (!every.ok) {
		puts("cannot read notification.every");
	}
	else {
		config->every = every.u.i;
	}

	/* done with conf */
	toml_free(conf);

	return 0;
}

void print_config(struct Configuration config) {
	printf("[notification]\n");
	printf("enable = %s\n", config.enable ? "true" : "false");
	printf("last = %li\n", config.last);
	printf("every = %i\n", config.every);
}

int update_config(struct Configuration config) {
	char* file = get_config_file_path();
	FILE* fp;
	fp = fopen(file, "w");
	if (fp == NULL) {
		puts("CONF FILE MISSING");
		return -1;
	}
	else {
		puts("Update configuration");
		fprintf(fp, "[notification]\n");
		fprintf(fp, "enable = %s\n", config.enable ? "true" : "false");
		fprintf(fp, "last = %li\n", config.last);
		fprintf(fp, "every = %i\n", config.every);
	}
	fclose(fp);
	free(file);
	return 1;
}