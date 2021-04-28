#include <stdio.h>
#include <ctype.h>
#include <stdlib.h>
#include <getopt.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <argp.h>
#include <stdbool.h>

#include "config.h"
#include "mytime.h"
#include "lib/tomlc99/toml.h"
#include "lib/libxdg-basedir/include/basedir_fs.h"

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

void default_config(struct Configuration * conf) {
	conf->enable = true;  // If reminder is enable
	conf->last = 0; // Last measurement in seconds
	conf->every = 86400;  // How often should remind (1 day)
}

char* get_config_dir_path(xdgHandle* handle) {
	const char* xdgconfig = xdgConfigHome(handle);
	char* path = malloc(strlen(xdgconfig) + strlen("/" CONFIG_DIR) + 1);
	strcpy(path, xdgconfig);
	strcat(path, "/" CONFIG_DIR);
	return path;
}

char* get_config_file_path(xdgHandle* handle) {
	char* dir = get_config_dir_path(handle);
	char* path = malloc(strlen(dir) + strlen("/" CONFIG_FILE) + 1);
	strcpy(path, dir);
	strcat(path, "/" CONFIG_FILE);
	free(dir);
	return path;
}

void create_file(struct Configuration * conf) {
	FILE* fp = xdgConfigOpen(CONFIG_DIR "/" CONFIG_FILE, "w+", &conf->xdg);
	if (fp == NULL) {
		printf("ERROR: %s\n", strerror(errno));
		exit(-1);
	} else {
		fclose(fp);
		default_config(conf);
		print_config(*conf);
		write_config(*conf);
	}
}

void create_config(struct Configuration * conf) {
	char* files = xdgConfigFind(CONFIG_DIR, &conf->xdg);  // Check if the condifg directory exist
	if (strlen(files) == 0) {  // If the directory do not exist
		char* path = get_config_dir_path(&conf->xdg);
		if (xdgMakePath(path, S_IRWXU | S_IRGRP | S_IWGRP) != 0) {  // Create the dir and check for errors
			printf("ERROR: xdgMakePath: %s\n", strerror(errno));
			exit(-1);
		}
		free(path);
	}
	create_file(conf);  // Create the file and return the default configuration.
}

void print_config(struct Configuration config) {
	printf("[notification]\n");
	printf("enable = %s\n", config.enable ? "true" : "false");
	printf("last = %li\n", config.last);
	printf("every = %i\n", config.every);
}

void write_config(struct Configuration config) {
	FILE* fp = xdgConfigOpen(CONFIG_DIR "/" CONFIG_FILE, "w", &config.xdg);
	if (fp == NULL) {
		puts("CONF FILE MISSING");
	} else {
		fprintf(fp, "[notification]\n");
		fprintf(fp, "enable = %s\n", config.enable ? "true" : "false");
		fprintf(fp, "last = %li\n", config.last);
		fprintf(fp, "every = %i\n", config.every);
		fclose(fp);
	}
}

int read_config(struct Configuration* config) {
	xdgInitHandle(&config->xdg);
	FILE* cf;
	char* files = xdgConfigFind(CONFIG_DIR "/" CONFIG_FILE, &config->xdg);  // Check if config file exist
	if (strlen(files) != 0) {
		cf = xdgConfigOpen(CONFIG_DIR "/" CONFIG_FILE, "r", &config->xdg);
	} else {
		puts("INFO: Config file not found creating new one.");
		create_config(config);
		return 1;
	}

	char errbuf[200];
	toml_table_t* conf = toml_parse_file(cf, errbuf, sizeof(errbuf));
	fclose(cf);
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
	} else {
		config->enable = enable.u.b;
	}

	toml_datum_t last = toml_int_in(notification, "last");
	if (!last.ok) {
		puts("cannot read notification.last");
	} else {
		config->last = last.u.i;
	}

	toml_datum_t every = toml_int_in(notification, "every");
	if (!every.ok) {
		puts("cannot read notification.every");
	} else {
		config->every = every.u.i;
	}

	toml_free(conf);
	return 0;
}

void update_last_config(struct Configuration config) {
	struct timeval now;
	set_time(&now);
	config.last = now.tv_sec;
}