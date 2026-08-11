#ifndef MDNS_CODEC_H
#define MDNS_CODEC_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define MDNS_TYPE_A 1U
#define MDNS_TYPE_PTR 12U
#define MDNS_TYPE_TXT 16U
#define MDNS_TYPE_SRV 33U
#define MDNS_TYPE_ANY 255U
#define MDNS_SERVICE_TYPE "_device-setup._tcp.local"

typedef struct MdnsServiceConfig
{
    const char * service_type;
    const char * instance_name;
    const char * host_name;
    const char * device_id;
    const char * name;
    const char * model;
    uint8_t ipv4[ 4 ];
    uint16_t port;
} MdnsServiceConfig;

bool mdns_build_service_names( const char * model, const char * platform,
                               const char * device_id, char * instance_name,
                               size_t instance_name_size, char * host_name,
                               size_t host_name_size );

bool mdns_decode_name( const uint8_t * packet, size_t length, size_t offset,
                       char * output, size_t output_size, size_t * consumed );
size_t mdns_build_response( const MdnsServiceConfig * config,
                            const uint8_t * query, size_t query_length,
                            uint8_t * output, size_t output_size );
size_t mdns_build_announcement( const MdnsServiceConfig * config, uint32_t ttl,
                                uint8_t * output, size_t output_size );
size_t mdns_build_probe( const MdnsServiceConfig * config,
                         uint8_t * output, size_t output_size );

#endif

