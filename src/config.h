#ifndef BIN_CONFIG_H
#define BIN_CONFIG_H

#include <argp.h>
#include <stdbool.h>

#include "lib/libxdg-basedir/include/basedir_fs.h"

#define CONFIG_DIR  "pc-fit"
#define CONFIG_FILE "pc-fit.conf"


struct Arguments {
	char* file_name;
	size_t file_name_len;
	bool reminder;  // If check reminder
};

error_t parse_opt(int key, char* arg, struct argp_state* state);

struct Configuration {
	bool enable;  // If reminder is enable
	long int last; // Last measurement in seconds
	int every;  // How often should remind
	xdgHandle xdg;
};

void default_config(struct Configuration *);

void write_config(struct Configuration config);

int read_config(struct Configuration* config);

void print_config(struct Configuration config);

void update_last_config(struct Configuration config);
#endif //BIN_CONFIG_H
