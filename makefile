CFLAGS ?= -g -Wall
LDLIBS = -lX11 -lXrender
SOURCES = xfds.c configuration.c network.c x11.c

OBJS = $(patsubst %.c,%.o,$(SOURCES))

martrix: $(OBJS)

clean:
	$(RM) $(OBJS)
	$(RM) martrix

run:
	valgrind --leak-check=full --track-origins=yes --show-reachable=yes ./martrix
