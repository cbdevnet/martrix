#include <unistd.h>
#include <string.h>
#include "configuration.h"
#include "network.h"

static unsigned dimensions_first(char* dims){
	return strtoul(dims, NULL, 10);
}

static unsigned dimensions_second(char* dims){
	if(!strchr(dims, 'x')){
		return 0;
	}

	return strtoul(strchr(dims, 'x') + 1, NULL, 10);
}

//FIXME this function is really ugly
int config_read(char* path, config* cfg){
	enum {
		none,
		martrix,
		ftype,
		map
	} parser_state = none;
	size_t current_ftype = 0, field_separator = 0;
	char* map_target;
	size_t uarg, channel;
	size_t current_line_allocated = 0;
	char* current_line = NULL;
	ssize_t status;
	FILE* file = fopen(path, "r");

	if(!file){
		printf("Failed to open configuration file %s\n", path);
		return -1;
	}

	for(status = getline(&current_line, &current_line_allocated, file); status > 0; status = getline(&current_line, &current_line_allocated, file)){
		//skip comments
		if(*current_line == ';'){
			continue;
		}

		//trim newline
		for(status--; status > 0 && current_line[status] && !isgraph(current_line[status]); status--){
			current_line[status] = 0;
		}

		if(status < 1){
			continue;
		}

		//handle sections
		if(*current_line == '[' && current_line[status] == ']'){
			if(!strncmp(current_line, "[martrix]", 9)){
				parser_state = martrix;
			}
			else if(!strncmp(current_line, "[fixture ", 9)){
				parser_state = ftype;

				//new fixture entry
				cfg->visualizer.types = realloc(cfg->visualizer.types, (cfg->visualizer.num_types + 1) * sizeof(fixture_type));
				if(!cfg->visualizer.types){
					printf("Failed to allocate memory\n");
					return 1;
				}
				current_ftype = cfg->visualizer.num_types++;
				cfg->visualizer.types[current_ftype].channels = 0;

				//store name
				cfg->visualizer.types[current_ftype].name = calloc(strlen(current_line + 9), sizeof(char));
				if(!cfg->visualizer.types[current_ftype].name){
					printf("Failed to allocate memory\n");
					return 1;
				}
				strncpy(cfg->visualizer.types[current_ftype].name, current_line + 9, strlen(current_line + 9) - 1);

				printf("Created type %s\n", cfg->visualizer.types[current_ftype].name);
			}
			else if(!strncmp(current_line, "[map]", 5)){
				parser_state = map;
			}
		}
		//handle fields
		else{
			for(field_separator = 0; current_line[field_separator] && current_line[field_separator] != '='; field_separator++){
			}

			if(field_separator >= status){
				printf("Line format invalid, ignored: %s\n", current_line);
				continue;
			}

			for(field_separator++; current_line[field_separator] && !isgraph(current_line[field_separator]); field_separator++){
			}

			switch(parser_state){
				case none:
					//ignore
					break;
				case martrix:
					//bind host
					if(!strncmp(current_line, "bindhost", 8)){
						cfg->network.fd = network_listener(current_line + field_separator, "6454");
						if(cfg->network.fd < 0){
							printf("Failed to open ArtNet listener\n");
							return -1;
						}
					}
					//net
					else if(!strncmp(current_line, "net", 3)){
						cfg->network.net = strtoul(current_line + field_separator, NULL, 10);
					}
					//window dimensions
					else if(!strncmp(current_line, "window", 6)){
						cfg->xres.width = dimensions_first(current_line + field_separator);
						cfg->xres.height = dimensions_second(current_line + field_separator);
						cfg->xres.windowed = true;
					}
					//matrix dimensions
					else if(!strncmp(current_line, "dimensions", 10)){
						cfg->visualizer.dim_x = dimensions_first(current_line + field_separator);
						cfg->visualizer.dim_y = dimensions_second(current_line + field_separator);

						if(!cfg->visualizer.dim_x || !cfg->visualizer.dim_y){
							printf("Invalid dimensions specified\n");
							return -1;
						}

						cfg->visualizer.fixtures = calloc(cfg->visualizer.dim_x * cfg->visualizer.dim_y, sizeof(fixture));
						if(!cfg->visualizer.fixtures){
							printf("Failed to allocate memory\n");
							return 1;
						}
					}
					break;
				case map:
					uarg = dimensions_first(current_line);
					channel = dimensions_second(current_line);
					if(!uarg || !channel || uarg > cfg->visualizer.dim_x || channel > cfg->visualizer.dim_y){
						printf("Tried to map invalid coordinates %zux%zu on grid %zux%zu\n", uarg, channel, cfg->visualizer.dim_x, cfg->visualizer.dim_y);
						break;
					}

					//calculate offset into fixture array
					uarg += (channel - 1) * cfg->visualizer.dim_y - 1;

					//find fixture type to map to
					map_target = strtok(current_line + field_separator, " ");
					for(current_ftype = 0; current_ftype < cfg->visualizer.num_types; current_ftype++){
						if(!strcmp(cfg->visualizer.types[current_ftype].name, map_target)){
							//get universe and address
							map_target = strtok(NULL, " ");
							if(!map_target){
								printf("Incomplete mapping: %s\n", current_line);
								return 1;
							}
							cfg->visualizer.fixtures[uarg].universe = strtoul(map_target, NULL, 10);

							map_target = strtok(NULL, " ");
							if(!map_target){
								printf("Incomplete mapping: %s\n", current_line);
								return 1;
							}
							cfg->visualizer.fixtures[uarg].addr = strtoul(map_target, &map_target, 10);
							cfg->visualizer.fixtures[uarg].type = current_ftype;
							cfg->visualizer.fixtures[uarg].alive = true;
							printf("Mapped %s at %zu\n", cfg->visualizer.types[current_ftype].name, uarg);
							break;
						}
					}

					if(current_ftype == cfg->visualizer.num_types){
						printf("Unknown type %s\n", map_target);
					}
					break;
				case ftype:
					//convert arg
					uarg = strtoul(current_line + field_separator, NULL, 10);
					//channel count or channel mapping
					if(!strncmp(current_line, "channels", 8)){
						cfg->visualizer.types[current_ftype].channels = uarg;
						cfg->visualizer.types[current_ftype].map = calloc(uarg, sizeof(fixture_channel));
						if(!cfg->visualizer.types[current_ftype].map){
							printf("Failed to allocate memory\n");
							return 1;
						}
					}
					else{
						//check channel
						if(uarg >= cfg->visualizer.types[current_ftype].channels){
							printf("Ignoring invalid mapping: %s\n", current_line);
							continue;
						}

						//find mapped function
						for(channel = 0; channel < channel_max; channel++){
							if(!strncmp(current_line, fixture_channel_name[channel], strlen(fixture_channel_name[channel]))){
								break;
							}
						}
						if(channel == channel_max){
							printf("Unknown channel mapped: %s\n", current_line);
							continue;
						}

						//store mapping
						cfg->visualizer.types[current_ftype].map[uarg] = channel;
					}
					break;
			}
		}
	}

	//allocate necessary universes
	for(uarg = 0; uarg < cfg->visualizer.dim_x * cfg->visualizer.dim_y; uarg++){
		if(cfg->visualizer.fixtures[uarg].alive){
			//check if universe for that fixture already allocated
			for(channel = 0; channel < cfg->network.num_universes; channel++){
				if(cfg->network.universes[channel].ident == cfg->visualizer.fixtures[uarg].universe){
					break;
				}
			}

			if(channel == cfg->network.num_universes){
				//allocate new universe
				cfg->network.universes = realloc(cfg->network.universes, (cfg->network.num_universes + 1) * sizeof(artnet_universe));
				if(!cfg->network.universes){
					printf("Failed to allocate memory\n");
					return 1;
				}

				memset(cfg->network.universes + cfg->network.num_universes, 0, sizeof(artnet_universe));
				cfg->network.universes[channel].ident = cfg->visualizer.fixtures[uarg].universe;
				printf("Listening for data on universe %u\n", cfg->network.universes[channel].ident);
				cfg->network.num_universes++;
			}
		}
	}

	free(current_line);
	fclose(file);
	return 0;
}

void config_free(config* cfg){
	size_t u;

	cfg->visualizer.dim_x = 0;
	cfg->visualizer.dim_y = 0;

	free(cfg->visualizer.fixtures);

	for(u = 0; u < cfg->visualizer.num_types; u++){
		free(cfg->visualizer.types[u].name);
		free(cfg->visualizer.types[u].map);
	}
	free(cfg->visualizer.types);

	cfg->visualizer.num_types = 0;

	free(cfg->network.universes);
	cfg->network.num_universes = 0;
	close(cfg->network.fd);
	cfg->network.fd = -1;
}
