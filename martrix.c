#include <string.h>
#include "martrix.h"
#include "configuration.h"
#include "x11.h"

volatile sig_atomic_t shutdown_requested = 0;

void signal_handler(int signum){
	shutdown_requested = 1;
}

int usage(char* fn){
	printf("%s - ArtNet Matrix visualizer", MARTRIX_VERSION);
	//TODO
	return EXIT_FAILURE;
}

int main(int argc, char** argv){
	config conf = {
		.listener = -1
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
	if(config_read(config_file, &conf)){
		return usage(argv[0]);
	}

	//initialize x11
	if(x11_init(&conf)){
		config_free(&conf);
		return usage(argv[0]);
	}

	//TODO run core loop

	//cleanup x11
	x11_cleanup(&conf);
	//free configuration
	config_free(&conf);

	return EXIT_SUCCESS;
}
