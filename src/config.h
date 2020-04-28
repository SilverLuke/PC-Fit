#ifndef BIN_CONFIG_H
#define BIN_CONFIG_H

#define CONFIG_FILE "~/.config/pc-fit.conf"

struct Configuration {
	int enable;  // If reminder is enable
	long int last; // Last measurement in seconds
	int every;  // How often should remind
};

struct Argument {
	char * file_name;
	size_t file_name_len;
	char reminder;  // If check reminder
};

int parse_config(struct Configuration * configuration);

struct Argument parse_arg(int argc, char **argv);

#endif //BIN_CONFIG_H
