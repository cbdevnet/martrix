#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include "network.h"

#define min(a,b) (((a) < (b)) ? (a) : (b))

static int network_handle_sacn(config_t* cfg, uint8_t* data, size_t len){
	sacn_header* hdr = (sacn_header*) data;
	sacn_data* frame = NULL;
	size_t u;

	if(len > sizeof(sacn_header)){
		frame = (sacn_data*) (data + sizeof(sacn_header));
		if(!memcmp(hdr->magic, SACN_MAGIC, sizeof(hdr->magic))){
			if(len > sizeof(sacn_header) + sizeof(sacn_data)
					&& be16toh(hdr->preamble) == 0x10
					&& hdr->postamble == 0
					&& be32toh(hdr->vector) == SACN_ROOT_DATA
					&& be32toh(hdr->frame_vector) == SACN_FRAME_DATA
					&& frame->vector == SACN_DMP_SET
					&& frame->format == 0xA1
					&& frame->startcode_offset == 0
					&& be16toh(frame->address_increment) == 1
					&& be16toh(frame->channels <= 513
					&& be16toh(frame->channels) >= 1)){

				//FIXME source priority is currently ignored, could be used to mux
				//FIXME sacn synchronization currently ignored

				//terminate source name
				frame->source_name[63] = 0;
				if(cfg->network.verbose > 0){
					printf("Sequence %u from %s for universe %u, %u channels -> ", frame->sequence, frame->source_name, be16toh(frame->universe), be16toh(frame->channels) - 1);
				}
				if(len == sizeof(sacn_header) + sizeof(sacn_data) + be16toh(frame->channels)){
					for(u = 0; u < cfg->network.num_universes; u++){
						if(cfg->network.universes[u].ident == be16toh(frame->universe)){
							memcpy(cfg->network.universes[u].data, data + sizeof(sacn_header) + sizeof(sacn_data) + 1, be16toh(frame->channels) - 1);
							if(cfg->network.verbose > 0){
								printf("accepted\n");
							}
							return 1;
						}
					}

					if(u == cfg->network.num_universes && cfg->network.verbose > 0){
						printf("ignored\n");
					}
				}
				else if(cfg->network.verbose > 0){
					printf("rejected\n");
				}
			}
			else{
				printf("Malformed sACN packet received\n");
			}
		}
	}

	return 0;
}

static int network_handle_artnet(config_t* cfg, uint8_t* data, size_t len){
	artnet_header* hdr = (artnet_header*) data;
	artnet_output_pkt* output = NULL;
	size_t u;

	if(len > sizeof(artnet_header)){
		if(!memcmp(hdr->magic, ARTNET_MAGIC, sizeof(hdr->magic))){
			switch(hdr->opcode){
				case OpDmx:
					if(len > sizeof(artnet_header) + sizeof(artnet_output_pkt)){
						//convert data
						output = (artnet_output_pkt*) (data + sizeof(artnet_header));
						output->length = be16toh(output->length);

						if(cfg->network.verbose > 0){
							printf("Sequence %u for net %u universe %u, %u bytes -> ", output->sequence, output->net, output->universe, output->length);
						}
						if(cfg->network.net == output->net && len == sizeof(artnet_header) + sizeof(artnet_output_pkt) + output->length){
							//find matching universe
							for(u = 0; u < cfg->network.num_universes; u++){
								if(cfg->network.universes[u].ident == output->universe){
									memcpy(cfg->network.universes[u].data, data + sizeof(artnet_header) + sizeof(artnet_output_pkt), min(512, output->length));
									if(cfg->network.verbose > 0){
										printf("accepted\n");
									}
									return 1;
								}
							}

							if(u == cfg->network.num_universes && cfg->network.verbose > 0){
								printf("ignored\n");
							}
						}
						else if(cfg->network.verbose > 0){
							printf("rejected\n");
						}
					}
					else{
						printf("Short read on ArtOutput packet\n");
					}
					break;
				default:
					printf("Unhandled ArtNet opcode %04X\n", hdr->opcode);
					break;
			}
		}
	}
	return 0;
}

int network_handle(config_t* cfg){
	int rv = 0;
	uint8_t data[RECV_BUFFER];
	ssize_t bytes_read;

	do{
		bytes_read = recvfrom(cfg->network.fd, &data, sizeof(data), MSG_DONTWAIT, NULL, NULL);

		if(bytes_read > 0){
			switch(cfg->network.type){
				case input_artnet:
					rv |= network_handle_artnet(cfg, data, bytes_read);
					break;
				case input_sacn:
					rv |= network_handle_sacn(cfg, data, bytes_read);
					break;
				default:
					printf("Unknown protocol handler\n");
					return -1;
			}
		}
	} while(bytes_read >= 0);

	if(bytes_read < 0 && errno != EAGAIN){
		shutdown_requested = 1;
		perror("recv");
	}
	return rv;
}

static int network_listener(char* bindhost, char* port){
	int fd = -1, status, yes = 1;
	struct addrinfo hints = {
		.ai_family = AF_UNSPEC,
		.ai_socktype = SOCK_DGRAM,
		.ai_flags = AI_PASSIVE
	};
	struct addrinfo* info;
	struct addrinfo* addr_it;

	status = getaddrinfo(bindhost, port, &hints, &info);
	if(status){
		fprintf(stderr, "Failed to get socket info for %s port %s: %s\n", bindhost, port, gai_strerror(status));
		return -1;
	}

	for(addr_it = info; addr_it != NULL; addr_it = addr_it->ai_next){
		fd = socket(addr_it->ai_family, addr_it->ai_socktype, addr_it->ai_protocol);
		if(fd < 0){
			continue;
		}

		yes = 1;
		if(setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, (void*)&yes, sizeof(yes)) < 0){
			fprintf(stderr, "Failed to set SO_REUSEADDR on socket\n");
		}

		yes = 1;
		if(setsockopt(fd, SOL_SOCKET, SO_BROADCAST, (void*)&yes, sizeof(yes)) < 0){
			fprintf(stderr, "Failed to set SO_BROADCAST on socket\n");
		}

		status = bind(fd, addr_it->ai_addr, addr_it->ai_addrlen);
		if(status < 0){
			close(fd);
			continue;
		}

		break;
	}

	freeaddrinfo(info);

	if(!addr_it){
		fprintf(stderr, "Failed to create listening socket for %s port %s\n", bindhost, port);
		return -1;
	}
	return fd;
}

int network_start(config_t* cfg){
	char* listen_port = (cfg->network.type == input_sacn) ? SACN_DEFAULT_PORT : ARTNET_DEFAULT_PORT;
	char* listen_host = cfg->network.bindhost;
	size_t u;

	struct ip_mreq mcast_req = {
		.imr_interface = { INADDR_ANY }
	};

	if(strchr(listen_host, ' ')){
		listen_port = strchr(listen_host, ' ');
		*listen_port++ = 0;
	}

	cfg->network.fd = network_listener(listen_host, listen_port);
	if(cfg->network.fd < 0){
		printf("Failed to open data input listener\n");
		return -1;
	}

	//for sacn, join the multicast groups
	if(cfg->network.type == input_sacn){
		for(u = 0; u < cfg->network.num_universes; u++){
			mcast_req.imr_multiaddr.s_addr = htobe32(0xefff0000 | ((uint32_t) cfg->network.universes[u].ident));
			if(setsockopt(cfg->network.fd, IPPROTO_IP, IP_ADD_MEMBERSHIP, &mcast_req, sizeof(mcast_req))){
				printf("Failed to join sACN universe multicast group for universe %u: %s\n", cfg->network.universes[u].ident, strerror(errno));
			}
		}
	}

	return 0;
}
