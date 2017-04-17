#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <X11/Xlib.h>

typedef struct /*XFD_AGGREG*/ {
	int* fds;
	unsigned size;
} X_FDS;

bool xfd_add(X_FDS* set, int fd);
bool xfd_remove(X_FDS* set, int fd);
void xfd_free(X_FDS* set);
void xconn_watch(Display* dpy, XPointer client_data, int fd, Bool opening, XPointer* watch_data);
