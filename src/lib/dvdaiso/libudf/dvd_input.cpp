#include "dvd_input.h"

dvd_input_t dvdinput_open(dvd_input_t dev) {
	return dev;
}

int dvdinput_close(dvd_input_t dev) {
	(void)dev;
	return 0;
}

int dvdinput_seek(dvd_input_t dev, int block) {
	if (!dev->seek(2048 * block)) {
		return 0;
	}
	return block;
}

int dvdinput_read(dvd_input_t dev, void *buffer, int blocks, int encrypted) {
	if (encrypted == 1) {
		return 0;
	}
	return dev->read(buffer, 2048 * blocks) / 2048;
}
