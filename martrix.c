#include <string.h>
#include "martrix.h"
#include "configuration.h"
#include "network.h"
#include "x11.h"

#define max(a,b) (((a) > (b)) ? (a) : (b))
volatile sig_atomic_t shutdown_requested = 0;

void signal_handler(int signum){
	shutdown_requested = 1;
}

int usage(char* fn){
	printf("\nmartrix - Lighting grid/matrix visualizer\n");
	printf("Controlled by input via sACN or ArtNet\n");
	printf("Usage:\n\t%s <configfile>\n", fn);
	return EXIT_FAILURE;
}

void martrix(config_t* cfg){
	fd_set read_fds;
	struct timeval tv;
	size_t u;
	int maxfd, status = 0;

	while(!shutdown_requested){
		while(XPending(cfg->xres.display)){
			status |= x11_handle(cfg);
		}

		//abort requested by window
		if(shutdown_requested){
			break;
		}

		if(status || cfg->xres.rerender_required){
			cfg->xres.rerender_required = 0;
			x11_render(cfg);
			if(clock_gettime(CLOCK_MONOTONIC_RAW, &cfg->last_render)){
				perror("clock_gettime");
			}
		}

		XFlush(cfg->xres.display);

		FD_ZERO(&read_fds);
		tv.tv_sec = 0;
		tv.tv_usec = 10000;

		maxfd = cfg->network.fd;
		FD_SET(cfg->network.fd, &read_fds);
		for(u = 0; u < cfg->xres.fds.size; u++){
			FD_SET(cfg->xres.fds.fds[u], &read_fds);
			maxfd = max(maxfd, cfg->xres.fds.fds[u]);
		}

		status = select(maxfd + 1, &read_fds, NULL, NULL, &tv);
		if(status > 0){
			if(FD_ISSET(cfg->network.fd, &read_fds)){
				//process artnet input
				status = network_handle(cfg);
			}
			else{
				//x11 event
				status = 0;
			}
		}
		else if(status == 0){
			//timeout
		}
		else{
			perror("select");
			break;
		}
	}
}

int main(int argc, char** argv){
	config_t conf = {
		.network.fd = -1,
		.network.verbose = 1
	};
	char* config_file = "martrix.cfg";
	if(argc > 1){
		if(!strcmp(argv[1], "-h") || !strcmp(argv[1], "--help")){
			return usage(argv[0]);
		}
		config_file = argv[1];
	}

	signal(SIGINT, signal_handler);
	printf("%s starting up\n", MARTRIX_VERSION);

	//read configuration
	if(config_read(&conf, config_file)){
		return usage(argv[0]);
	}

	//start network listeners
	if(network_start(&conf)){
		return usage(argv[0]);
	}

	//initialize x11
	if(x11_init(&conf)){
		config_free(&conf);
		return usage(argv[0]);
	}

	//run core loop
	martrix(&conf);

	//cleanup x11
	x11_cleanup(&conf);
	//free configuration
	config_free(&conf);

	return EXIT_SUCCESS;
}
