#include "martrix.h"

#define SACN_DEFAULT_PORT "5568"
#define ARTNET_DEFAULT_PORT "6454"

#pragma pack(push, 1)
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

typedef struct /*_sacn_frame_root*/ {
	uint16_t preamble;
	uint16_t postamble;
	uint8_t magic[12];
	uint16_t flags;
	uint32_t vector;
	uint8_t sender_cid[16];

	uint16_t frame_flags;
	uint32_t frame_vector;
} sacn_header;

typedef struct /*_sacn_frame_data*/ {
	uint8_t source_name[64];
	uint8_t priority;
	uint16_t sync_address;
	uint8_t sequence;
	uint8_t options;
	uint16_t universe;

	uint16_t flags;
	uint8_t vector;
	uint8_t format;
	uint16_t startcode_offset;
	uint16_t address_increment;
	uint16_t channels;
	uint8_t data[513];
} sacn_data;
#pragma pack(pop)

enum artnet_pkt_opcode {
	OpDmx = 0x5000
};

int network_handle(config_t* cfg);
int network_start(config_t* cfg);
