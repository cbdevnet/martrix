#include <unistd.h>
#include <string.h>
#include "configuration.h"

static enum {
	config_none,
	config_martrix,
	config_ftype,
	config_map
} parser_state = config_none;

static unsigned dimensions_first(char* dims){
	return strtoul(dims, NULL, 10);
}

static unsigned dimensions_second(char* dims){
	if(!strchr(dims, 'x')){
		return 0;
	}

	return strtoul(strchr(dims, 'x') + 1, NULL, 10);
}

static int config_section(config_t* cfg, char* line){
	size_t current_ftype = 0;
	if(!strncmp(line, "[martrix]", 9)){
		parser_state = config_martrix;
	}
	else if(!strncmp(line, "[fixture ", 9)){
		parser_state = config_ftype;

		//TODO check for duplicates
		//new fixture entry
		cfg->visualizer.types = realloc(cfg->visualizer.types, (cfg->visualizer.num_types + 1) * sizeof(fixture_type_t));
		if(!cfg->visualizer.types){
			printf("Failed to allocate memory\n");
			return 1;
		}
		
		current_ftype = cfg->visualizer.num_types++;
		cfg->visualizer.types[current_ftype].channels = 0;

		//store name
		cfg->visualizer.types[current_ftype].name = calloc(strlen(line + 9), sizeof(char));
		if(!cfg->visualizer.types[current_ftype].name){
			printf("Failed to allocate memory\n");
			return 1;
		}

		strncpy(cfg->visualizer.types[current_ftype].name, line + 9, strlen(line + 9) - 1);
		printf("Created type %s\n", cfg->visualizer.types[current_ftype].name);
	}
	else if(!strncmp(line, "[map]", 5)){
		parser_state = config_map;
	}
	return 0;
}

static int config_line(config_t* cfg, char* option, char* value){
	size_t u, uarg, channel;
	size_t current_ftype = cfg->visualizer.num_types - 1;
	char* map_target = NULL;

	switch(parser_state){
		case config_none:
			//ignore
			break;
		case config_martrix:
			//bind host
			if(!strncmp(option, "bindhost", 8)){
				if(cfg->network.bindhost){
					fprintf(stderr, "Bindhost already specified, aborting\n");
					return -1;
				}
				cfg->network.bindhost = strdup(value);
			}
			//artnet net
			else if(!strncmp(option, "net", 3)){
				cfg->network.net = strtoul(value, NULL, 10);
			}
			//input type
			else if(!strncmp(option, "input", 5)){
				if(!strncmp(value, "sacn", 4)){
					cfg->network.type = input_sacn;
				}
				else if(!strncmp(value, "artnet", 6)){
					cfg->network.type = input_artnet;
				}
				else{
					printf("Invalid network input protocol %s\n", value);
					return -1;
				}
			}
			//window dimensions
			else if(!strncmp(option, "window", 6)){
				cfg->xres.width = dimensions_first(value);
				cfg->xres.height = dimensions_second(value);
				cfg->xres.windowed = true;
			}
			//matrix dimensions
			else if(!strncmp(option, "dimensions", 10)){
				cfg->visualizer.dim_x = dimensions_first(value);
				cfg->visualizer.dim_y = dimensions_second(value);

				if(!cfg->visualizer.dim_x || !cfg->visualizer.dim_y){
					printf("Invalid dimensions specified\n");
					return -1;
				}

				cfg->visualizer.fixtures = calloc(cfg->visualizer.dim_x * cfg->visualizer.dim_y, sizeof(fixture_t));
				if(!cfg->visualizer.fixtures){
					printf("Failed to allocate memory\n");
					return 1;
				}
			}
			break;
		case config_map:
			uarg = dimensions_first(option);
			channel = dimensions_second(option);
			if(!uarg || !channel || uarg > cfg->visualizer.dim_x || channel > cfg->visualizer.dim_y){
				printf("Tried to map invalid coordinates %zux%zu on grid %zux%zu\n", uarg, channel, cfg->visualizer.dim_x, cfg->visualizer.dim_y);
				break;
			}

			//calculate offset into fixture array
			uarg += (channel - 1) * cfg->visualizer.dim_x - 1;

			//find fixture type to map to
			map_target = strtok(value, " ");
			for(u = 0; u < cfg->visualizer.num_types; u++){
				if(!strcmp(cfg->visualizer.types[u].name, map_target)){
					//get universe and address
					map_target = strtok(NULL, " ");
					if(!map_target){
						printf("Incomplete mapping: %s\n", option);
						return 1;
					}
					cfg->visualizer.fixtures[uarg].universe = strtoul(map_target, NULL, 10);

					map_target = strtok(NULL, " ");
					if(!map_target){
						printf("Incomplete mapping: %s\n", option);
						return 1;
					}
					cfg->visualizer.fixtures[uarg].addr = strtoul(map_target, &map_target, 10);
					cfg->visualizer.fixtures[uarg].type = u;
					cfg->visualizer.fixtures[uarg].alive = true;
					printf("Mapped %s at %zu\n", cfg->visualizer.types[u].name, uarg);
					break;
				}
			}

			if(u == cfg->visualizer.num_types){
				printf("Unknown type %s\n", map_target);
			}
			break;
		case config_ftype:
			//convert arg
			uarg = strtoul(value, NULL, 10);
			//channel count or channel mapping
			if(!strncmp(option, "channels", 8)){
				cfg->visualizer.types[current_ftype].channels = uarg;
				cfg->visualizer.types[current_ftype].map = calloc(uarg, sizeof(fixture_channel_t));
				if(!cfg->visualizer.types[current_ftype].map){
					printf("Failed to allocate memory\n");
					return 1;
				}
			}
			else{
				//check channel
				if(uarg >= cfg->visualizer.types[current_ftype].channels){
					printf("Ignoring invalid mapping: %s\n", option);
					return 0;
				}

				//find mapped function
				for(channel = 0; channel < channel_max; channel++){
					if(!strncmp(option, fixture_channel_name[channel], strlen(fixture_channel_name[channel]))){
						break;
					}
				}
				if(channel == channel_max){
					printf("Unknown channel mapped: %s\n", option);
					return 0;
				}

				//store mapping
				cfg->visualizer.types[current_ftype].map[uarg] = channel;
			}
			break;
	}
	return 0;
}

int config_read(config_t* cfg, char* path){
	int error;
	size_t field_separator = 0, current_line_allocated = 0, u, channel;
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
			error = config_section(cfg, current_line);
			if(error){
				return 1;
			}
		}
		//handle fields
		else{
			//find separator
			for(field_separator = 0; current_line[field_separator] && current_line[field_separator] != '='; field_separator++){
			}

			if(field_separator >= status){
				printf("Line format invalid, ignored: %s\n", current_line);
				continue;
			}

			for(field_separator++; current_line[field_separator] && !isgraph(current_line[field_separator]); field_separator++){
			}

			error = config_line(cfg, current_line, current_line + field_separator);
			if(error){
				return 1;
			}
		}
	}

	//allocate necessary universes
	for(u = 0; u < cfg->visualizer.dim_x * cfg->visualizer.dim_y; u++){
		if(cfg->visualizer.fixtures[u].alive){
			//check if universe for that fixture already allocated
			for(channel = 0; channel < cfg->network.num_universes; channel++){
				if(cfg->network.universes[channel].ident == cfg->visualizer.fixtures[u].universe){
					break;
				}
			}

			if(channel == cfg->network.num_universes){
				//allocate new universe
				cfg->network.universes = realloc(cfg->network.universes, (cfg->network.num_universes + 1) * sizeof(input_universe_t));
				if(!cfg->network.universes){
					printf("Failed to allocate memory\n");
					return 1;
				}

				memset(cfg->network.universes + cfg->network.num_universes, 0, sizeof(input_universe_t));
				cfg->network.universes[channel].ident = cfg->visualizer.fixtures[u].universe;
				printf("Listening for data on universe %u\n", cfg->network.universes[channel].ident);
				cfg->network.num_universes++;
			}
		}
	}

	free(current_line);
	fclose(file);
	return 0;
}

void config_free(config_t* cfg){
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
	free(cfg->network.bindhost);
}
