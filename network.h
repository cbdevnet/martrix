#include "martrix.h"

typedef struct /*_artnet_pkt_header*/ {
	uint8_t magic[8];
	uint16_t opcode;
	uint16_t version;
} artnet_header;

typedef struct /*_artnet_output_pkt*/ {
	uint8_t sequence;
	uint8_t port;
	uint8_t universe;
	uint8_t net;
	uint16_t length;
} artnet_output_pkt;

enum artnet_pkt_opcode {
	OpDmx = 0x5000
};

int network_handle(config* cfg);
int network_listener(char* host, char* port);
