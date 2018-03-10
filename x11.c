#include "x11.h"
#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <X11/Xutil.h>
#include <X11/extensions/Xrender.h>
#include <sys/types.h>
#include <unistd.h>

int x11_init(config_t* cfg){
	char* window_title = "xmartrix";
	Window root;
	XSetWindowAttributes window_attributes;
	Atom wm_state_fullscreen;
	XTextProperty window_name;
	pid_t pid = getpid();
	int xrender_major, xrender_minor;

	//allocate some structures
	XSizeHints* size_hints = XAllocSizeHints();
	XWMHints* wm_hints = XAllocWMHints();
	XClassHint* class_hints = XAllocClassHint();

	if(!size_hints || !wm_hints || !class_hints){
		fprintf(stderr, "Failed to allocate X data structures\n");
		return -1;
	}

	//x data initialization
	cfg->xres.display = XOpenDisplay(NULL);

	if(!(cfg->xres.display)){
		fprintf(stderr, "Failed to open display\n");
		XFree(size_hints);
		XFree(wm_hints);
		XFree(class_hints);
		return -1;
	}

	XRenderQueryVersion(cfg->xres.display, &xrender_major, &xrender_minor);
	printf("Xrender version %d.%d\n", xrender_major, xrender_minor);
	//XSynchronize(cfg->xres.display, True);

	cfg->xres.screen = DefaultScreen(cfg->xres.display);
	root = RootWindow(cfg->xres.display, cfg->xres.screen);

	if(!XMatchVisualInfo(cfg->xres.display, cfg->xres.screen, 32, TrueColor, &(cfg->xres.visual_info))) {
		fprintf(stderr, "Display does not support RGBA TrueColor visual\n");
		return -1;
	}

	cfg->xres.colormap = XCreateColormap(cfg->xres.display, root, cfg->xres.visual_info.visual, AllocNone);

	//set up window params
	window_attributes.background_pixel = XBlackPixel(cfg->xres.display, cfg->xres.screen);
	window_attributes.border_pixel = XBlackPixel(cfg->xres.display, cfg->xres.screen);
	window_attributes.colormap = cfg->xres.colormap;
	window_attributes.cursor = None;
	window_attributes.event_mask = ExposureMask | KeyPressMask | ButtonPressMask | StructureNotifyMask;

	if(!cfg->xres.width){
		cfg->xres.width = DisplayWidth(cfg->xres.display, cfg->xres.screen);
		cfg->xres.height = DisplayHeight(cfg->xres.display, cfg->xres.screen);
	}

	//create window
	cfg->xres.main = XCreateWindow(cfg->xres.display,
				root,
				0,
				0,
				cfg->xres.width,
				cfg->xres.height,
				0,
				32,
				InputOutput,
				cfg->xres.visual_info.visual,
				CWBackPixel | CWCursor | CWEventMask | CWBorderPixel | CWColormap,
				&window_attributes);

	//set window properties
	if(XStringListToTextProperty(&window_title, 1, &window_name) == 0){
		fprintf(stderr, "Failed to create string list, aborting\n");
		return -1;
	}

	wm_hints->flags = 0;
	class_hints->res_name = window_title;
	class_hints->res_class = window_title;

	XSetWMProperties(cfg->xres.display, cfg->xres.main, &window_name, NULL, NULL, 0, NULL, wm_hints, class_hints);

	XFree(window_name.value);
	XFree(size_hints);
	XFree(wm_hints);
	XFree(class_hints);

	//set fullscreen mode
	if(!cfg->xres.windowed){
		wm_state_fullscreen = XInternAtom(cfg->xres.display, "_NET_WM_STATE_FULLSCREEN", False);
		XChangeProperty(cfg->xres.display, cfg->xres.main, XInternAtom(cfg->xres.display, "_NET_WM_STATE", False), XA_ATOM, 32, PropModeReplace, (unsigned char*) &wm_state_fullscreen, 1);
	}

	XChangeProperty(cfg->xres.display, cfg->xres.main, XInternAtom(cfg->xres.display, "_NET_WM_PID", False), XA_CARDINAL, 32, PropModeReplace, (unsigned char*)&pid, 1);

	//register for WM_DELETE_WINDOW messages
	cfg->xres.wm_delete = XInternAtom(cfg->xres.display, "WM_DELETE_WINDOW", False);
	XSetWMProtocols(cfg->xres.display, cfg->xres.main, &(cfg->xres.wm_delete), 1);

	cfg->xres.window_picture = XRenderCreatePicture(cfg->xres.display, cfg->xres.main, XRenderFindStandardFormat(cfg->xres.display, PictStandardARGB32), 0, 0);

	//map window
	XMapRaised(cfg->xres.display, cfg->xres.main);

	//get x socket fds
	if(!xfd_add(&(cfg->xres.fds), XConnectionNumber(cfg->xres.display))){
		fprintf(stderr, "Failed to allocate xfd memory\n");
		return -1;
	}
	if(XAddConnectionWatch(cfg->xres.display, xconn_watch, (void*)(&(cfg->xres.fds))) == 0){
		fprintf(stderr, "Failed to register connection watch procedure\n");
		return -1;
	}

	return 0;
}

int x11_render(config_t* cfg){
	size_t u, j;
	input_universe_t* universe;
	fixture_t* fix;
	float dimming_factor = 1.0, strobe_factor = 1.0;
	XRenderColor color = {
		.alpha = -1
	};
	unsigned x, y;
	unsigned width = cfg->xres.width / cfg->visualizer.dim_x;
	unsigned height = cfg->xres.height / cfg->visualizer.dim_y;

	XClearWindow(cfg->xres.display, cfg->xres.main);

	//draw single fixture
	for(u = 0; u < cfg->visualizer.dim_x * cfg->visualizer.dim_y; u++){
		fix = cfg->visualizer.fixtures + u;
		if(fix->alive){
			//calculate window coordinates
			x = width * (u % cfg->visualizer.dim_x);
			y = height * (u / cfg->visualizer.dim_x);

			//find mapped universe
			for(j = 0; j < cfg->network.num_universes; j++){
				if(cfg->network.universes[j].ident == fix->universe){
					universe = cfg->network.universes + j;
					break;
				}
			}

			//calculate color
			dimming_factor = strobe_factor = 1.0;
			color.red = color.green = color.blue = 0;
			for(j = 0; j < cfg->visualizer.types[fix->type].channels; j++){
				switch(cfg->visualizer.types[fix->type].map[j]){
					case strobe:
						if(universe->data[fix->addr + j] > 0){
							cfg->xres.rerender_required = 1;
							//time % offtime > ontime
							//FIXME this calculation might need to be tuned somewhat
							strobe_factor = ((cfg->last_render.tv_nsec % (long)(1e9 / (universe->data[fix->addr + j] / 4))) > (100e6 - ((double)universe->data[fix->addr + j] / 255.0) * 99e6)) ? 0 : 1.0;
						}
						break;
					case dimmer:
						dimming_factor = universe->data[fix->addr + j] / 255.f;
						break;
					case red:
						color.red += universe->data[fix->addr + j] << 8;
						break;
					case green:
						color.green += universe->data[fix->addr + j] << 8;
						break;
					case blue:
						color.blue += universe->data[fix->addr + j] << 8;
						break;
					case white:
						color.red += universe->data[fix->addr + j] << 8;
						color.green += universe->data[fix->addr + j] << 8;
						color.blue += universe->data[fix->addr + j] << 8;
						break;
					default:
						//ignore
						break;
				}
			}

			dimming_factor *= strobe_factor;
			color.red *= dimming_factor;
			color.green *= dimming_factor;
			color.blue *= dimming_factor;

			//draw
			XRenderFillRectangle(cfg->xres.display, PictOpOver, cfg->xres.window_picture, &color, x, y, width, height);
		}
	}
	return 0;
}

int x11_handle(config_t* cfg){
	XEvent event;
	char pressed_key;

	XNextEvent(cfg->xres.display, &event);

	switch(event.type){
		case ConfigureNotify:
			cfg->xres.width = event.xconfigure.width;
			cfg->xres.height = event.xconfigure.height;
			return 1;
		case Expose:
			return 1;
		case KeyPress:
			//translate key event into a character, respecting keyboard layout
			if(XLookupString(&event.xkey, &pressed_key, 1, NULL, NULL) != 1){
				//disregard combined characters / bound strings
				return 0;
			}
			switch(pressed_key){
				case 'q':
					shutdown_requested = 1;
					return 0;
				case 'r':
					fprintf(stderr, "Redrawing on request\n");
					return 1;
			}
			return 0;
		case ClientMessage:
			if(event.xclient.data.l[0] == cfg->xres.wm_delete){
				fprintf(stderr, "Closing down window\n");
				shutdown_requested = 1;
			}
			return 0;
	}

	printf("Unhandled X11 message\n");
	return 0;
}

void x11_cleanup(config_t* cfg){
	if(!cfg->xres.display){
		return;
	}

	if(cfg->xres.main){
		XDestroyWindow(cfg->xres.display, cfg->xres.main);
	}

	if(cfg->xres.colormap){
		XFreeColormap(cfg->xres.display, cfg->xres.colormap);
	}

	XCloseDisplay(cfg->xres.display);
	xfd_free(&(cfg->xres.fds));
}
