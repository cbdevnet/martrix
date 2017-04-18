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

#define MARTRIX_VERSION "Martrix 0.1"
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
} fixture_channel;

typedef struct /*_visualizer_fixture_type*/ {
	char* name;
	size_t channels;
	fixture_channel* map;
} fixture_type;

typedef struct /*_visualizer_fixture*/ {
	bool alive;
	size_t type;
	uint8_t universe;
	uint16_t addr;
} fixture;

typedef struct /*_visualizer*/ {
	size_t dim_x, dim_y;
	size_t num_types;
	fixture_type* types;
	fixture* fixtures;
} martrix;

typedef struct /*_artnet_universe*/ {
	uint8_t data[512];
	uint8_t ident;
} artnet_universe;

typedef struct /*_artnet_data*/ {
	size_t num_universes;
	artnet_universe* universes;
	uint8_t net;
	int fd;
} artnet;

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
} x11;

typedef struct /*_visualizer_config*/ {
	martrix visualizer;
	x11 xres;
	artnet network;
	struct timespec last_render;
} config;
#endif
