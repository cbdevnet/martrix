#include "martrix.h"
int config_read(char* path, config* cfg);
void config_free(config* cfg);

static char* fixture_channel_name[channel_max] = {
	[ignore] = "ignore",
	[dimmer] = "dimmer",
	[strobe] = "strobe",
	[red] = "red",
	[green] = "green",
	[blue] = "blue"
};