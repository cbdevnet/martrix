#ifndef XMARTRIX
#define XMARTRIX
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>
#include <signal.h>
#include <time.h>
#include <ctype.h>

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/extensions/Xrender.h>
#include "xfds.h"

#define MARTRIX_VERSION "martrix 1.0"
extern volatile sig_atomic_t shutdown_requested;

typedef enum /*_visualizer_channel*/ {
	ignore = 0,
	dimmer,
	strobe,
	red,
	green,
	blue,
	white,
	uv,
	channel_max
} fixture_channel_t;

typedef struct /*_visualizer_fixture_type*/ {
	char* name;
	size_t channels;
	fixture_channel_t* map;
} fixture_type_t;

typedef struct /*_visualizer_fixture*/ {
	bool alive;
	size_t type;
	uint8_t universe;
	uint16_t addr;
} fixture_t;

typedef struct /*_visualizer*/ {
	size_t dim_x, dim_y;
	size_t num_types;
	fixture_type_t* types;
	fixture_t* fixtures;
} martrix_t;

typedef struct /*_artnet_universe*/ {
	uint8_t data[512];
	uint16_t ident;
} input_universe_t;

typedef enum /*_martrix_data_input*/ {
	input_artnet = 0,
	input_sacn
} input_type_t;

typedef struct /*_input_data*/ {
	size_t num_universes;
	input_universe_t* universes;
	uint8_t net;
	input_type_t type;
	char* bindhost;
	int fd;
} input_t;

typedef struct /*_x11_data*/ {
	Display* display;
	int screen;
	Window main;
	Picture window_picture;
	Colormap colormap;
	XVisualInfo visual_info;
	X_FDS fds;
	Atom wm_delete;
	unsigned width, height;
	bool windowed;
	uint8_t rerender_required;
} x11_data_t;

typedef struct /*_visualizer_config*/ {
	martrix_t visualizer;
	x11_data_t xres;
	input_t network;
	struct timespec last_render;
} config_t;
#endif
