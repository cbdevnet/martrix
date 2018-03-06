#include "martrix.h"
int config_read(config_t* cfg, char* path);
void config_free(config_t* cfg);

static char* fixture_channel_name[channel_max] = {
	[ignore] = "ignore",
	[dimmer] = "dimmer",
	[strobe] = "strobe",
	[red] = "red",
	[green] = "green",
	[blue] = "blue",
	[white] = "white",
	[uv] = "uv"
};
