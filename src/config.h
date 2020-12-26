#ifndef BIN_CONFIG_H
#define BIN_CONFIG_H

#include <argp.h>
#include <stdbool.h>

#define CONFIG_FILE "/.config/pc-fit.conf"

struct Configuration {
	int enable;  // If reminder is enable
	long int last; // Last measurement in seconds
	int every;  // How often should remind
};

struct Arguments {
	char* file_name;
	size_t file_name_len;
	bool reminder;  // If check reminder
};

int parse_config(struct Configuration* configuration);

error_t parse_opt(int key, char* arg, struct argp_state* state);

void print_config(struct Configuration config);

int update_config(struct Configuration config);

#endif //BIN_CONFIG_H
