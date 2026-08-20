#include "mdns_codec.h"

#include <ctype.h>
#include <stdio.h>
#include <string.h>

#define DNS_HEADER_SIZE 12U
#define DNS_CLASS_IN 1U
#define DNS_CACHE_FLUSH 0x8000U
#define MDNS_PTR_TTL 4500U
#define MDNS_UNIQUE_TTL 120U
#define MAX_DECODE_JUMPS 16U

typedef struct Writer
{
    uint8_t * data;
    size_t capacity;
    size_t position;
    bool ok;
} Writer;

static uint16_t read_u16( const uint8_t * value )
{
    return ( uint16_t ) ( ( ( uint16_t ) value[ 0 ] << 8 ) | value[ 1 ] );
}

static void put_u8( Writer * writer, uint8_t value )
{
    if( writer->position >= writer->capacity ) writer->ok = false;
    else writer->data[ writer->position++ ] = value;
}

static void put_u16( Writer * writer, uint16_t value )
{
    put_u8( writer, ( uint8_t ) ( value >> 8 ) );
    put_u8( writer, ( uint8_t ) value );
}

static void put_u32( Writer * writer, uint32_t value )
{
    put_u16( writer, ( uint16_t ) ( value >> 16 ) );
    put_u16( writer, ( uint16_t ) value );
}

static void put_bytes( Writer * writer, const void * data, size_t length )
{
    if( writer->position > writer->capacity || length > writer->capacity - writer->position ) writer->ok = false;
    else
    {
        memcpy( writer->data + writer->position, data, length );
        writer->position += length;
    }
}

static bool valid_name( const char * name )
{
    size_t total = name == NULL ? 0U : strlen( name );
    size_t label = 0U;
    if( total == 0U || total > 254U ) return false;
    for( size_t index = 0; index <= total; index++ )
    {
        if( name[ index ] == '.' || name[ index ] == '\0' )
        {
            if( label == 0U || label > 63U ) return false;
            label = 0U;
        }
        else label++;
    }
    return true;
}

bool mdns_build_service_names( const char * model, const char * platform,
                               const char * device_id, char * instance_name,
                               size_t instance_name_size, char * host_name,
                               size_t host_name_size )
{
    int instance_length;
    int host_length;

    if( model == NULL || platform == NULL || device_id == NULL ||
        instance_name == NULL || host_name == NULL ) return false;
    instance_length = snprintf( instance_name, instance_name_size,
        "%s-%s-%s." MDNS_SERVICE_TYPE,
        model, platform, device_id );
    host_length = snprintf( host_name, host_name_size,
        "freertos-%s-%s.local", platform, device_id );
    return instance_length > 0 && host_length > 0 &&
           ( size_t ) instance_length < instance_name_size &&
           ( size_t ) host_length < host_name_size &&
           valid_name( instance_name ) && valid_name( host_name );
}

static void put_name( Writer * writer, const char * name )
{
    const char * label = name;
    const char * cursor = name;
    if( !valid_name( name ) ) { writer->ok = false; return; }
    for( ;; cursor++ )
    {
        if( *cursor == '.' || *cursor == '\0' )
        {
            size_t length = ( size_t ) ( cursor - label );
            put_u8( writer, ( uint8_t ) length );
            put_bytes( writer, label, length );
            if( *cursor == '\0' ) break;
            label = cursor + 1;
        }
    }
    put_u8( writer, 0U );
}

static void put_pointer( Writer * writer, size_t offset )
{
    if( offset >= 0x4000U ) writer->ok = false;
    else put_u16( writer, ( uint16_t ) ( 0xc000U | offset ) );
}

static void patch_u16( Writer * writer, size_t offset, uint16_t value )
{
    if( offset + 1U >= writer->capacity ) writer->ok = false;
    else { writer->data[ offset ] = ( uint8_t ) ( value >> 8 ); writer->data[ offset + 1U ] = ( uint8_t ) value; }
}

bool mdns_decode_name( const uint8_t * packet, size_t length, size_t offset,
                       char * output, size_t output_size, size_t * consumed )
{
    size_t position = offset, written = 0U, direct = 0U, jumps = 0U;
    bool jumped = false;
    if( packet == NULL || output == NULL || consumed == NULL || output_size == 0U ) return false;
    while( position < length )
    {
        uint8_t label = packet[ position ];
        if( ( label & 0xc0U ) == 0xc0U )
        {
            size_t target;
            if( position + 1U >= length || ++jumps > MAX_DECODE_JUMPS ) return false;
            target = ( ( size_t ) ( label & 0x3fU ) << 8 ) | packet[ position + 1U ];
            if( target >= length || target == position ) return false;
            if( !jumped ) direct += 2U;
            jumped = true; position = target; continue;
        }
        if( ( label & 0xc0U ) != 0U || label > 63U ) return false;
        position++;
        if( !jumped ) direct++;
        if( label == 0U )
        {
            if( written >= output_size ) return false;
            output[ written ] = '\0'; *consumed = direct; return true;
        }
        if( position + label > length ) return false;
        if( written != 0U )
        {
            if( written + 1U >= output_size ) return false;
            output[ written++ ] = '.';
        }
        if( written + label >= output_size ) return false;
        memcpy( output + written, packet + position, label );
        written += label; position += label;
        if( !jumped ) direct += label;
    }
    return false;
}

static bool same_name( const char * left, const char * right )
{
    while( *left != '\0' && *right != '\0' )
        if( tolower( ( unsigned char ) *left++ ) != tolower( ( unsigned char ) *right++ ) ) return false;
    return *left == *right;
}

static bool config_valid( const MdnsServiceConfig * config )
{
    return config != NULL && valid_name( config->service_type ) && valid_name( config->instance_name ) &&
           valid_name( config->host_name ) && config->device_id != NULL && config->name != NULL &&
           config->model != NULL && strlen( config->device_id ) <= 200U &&
           strlen( config->name ) <= 250U && strlen( config->model ) <= 249U && config->port != 0U;
}

static void put_rr_header( Writer * writer, size_t owner, uint16_t type,
                           uint16_t dns_class, uint32_t ttl, size_t * rdlength )
{
    put_pointer( writer, owner ); put_u16( writer, type ); put_u16( writer, dns_class );
    put_u32( writer, ttl ); *rdlength = writer->position; put_u16( writer, 0U );
}

static void put_txt( Writer * writer, const char * text )
{
    size_t length = strlen( text );
    if( length > 255U ) writer->ok = false;
    else { put_u8( writer, ( uint8_t ) length ); put_bytes( writer, text, length ); }
}

static size_t build_records( const MdnsServiceConfig * config, uint32_t ttl,
                             uint8_t * output, size_t output_size )
{
    Writer writer = { output, output_size, 0U, true };
    size_t instance_offset, host_offset, length_offset, start;
    char id_txt[ 204 ], name_txt[ 256 ], model_txt[ 256 ];
    char api_txt[] = "api=1", path_txt[] = "path=/api/info";
    char ws_txt[] = "ws=/api/ws", auth_txt[] = "auth=/auth/authorize";
    uint32_t ptr_ttl = ttl == 0U ? 0U : MDNS_PTR_TTL;
    uint32_t unique_ttl = ttl == 0U ? 0U : MDNS_UNIQUE_TTL;
    if( !config_valid( config ) || output == NULL || output_size < DNS_HEADER_SIZE ) return 0U;
    memset( output, 0, DNS_HEADER_SIZE ); writer.position = DNS_HEADER_SIZE;
    output[ 2 ] = 0x84U; output[ 7 ] = 4U;

    put_name( &writer, config->service_type );
    put_u16( &writer, MDNS_TYPE_PTR ); put_u16( &writer, DNS_CLASS_IN ); put_u32( &writer, ptr_ttl );
    length_offset = writer.position; put_u16( &writer, 0U ); start = writer.position;
    instance_offset = writer.position; put_name( &writer, config->instance_name );
    patch_u16( &writer, length_offset, ( uint16_t ) ( writer.position - start ) );

    put_rr_header( &writer, instance_offset, MDNS_TYPE_SRV, DNS_CLASS_IN | DNS_CACHE_FLUSH, unique_ttl, &length_offset );
    start = writer.position; put_u16( &writer, 0U ); put_u16( &writer, 0U ); put_u16( &writer, config->port );
    host_offset = writer.position; put_name( &writer, config->host_name );
    patch_u16( &writer, length_offset, ( uint16_t ) ( writer.position - start ) );

    put_rr_header( &writer, instance_offset, MDNS_TYPE_TXT, DNS_CLASS_IN | DNS_CACHE_FLUSH, unique_ttl, &length_offset );
    start = writer.position;
    if( snprintf( id_txt, sizeof( id_txt ), "id=%s", config->device_id ) < 0 ) writer.ok = false;
    if( snprintf( name_txt, sizeof( name_txt ), "name=%s", config->name ) < 0 ) writer.ok = false;
    if( snprintf( model_txt, sizeof( model_txt ), "model=%s", config->model ) < 0 ) writer.ok = false;
    put_txt( &writer, id_txt ); put_txt( &writer, name_txt ); put_txt( &writer, model_txt );
    put_txt( &writer, api_txt ); put_txt( &writer, path_txt );
    put_txt( &writer, ws_txt ); put_txt( &writer, auth_txt );
    patch_u16( &writer, length_offset, ( uint16_t ) ( writer.position - start ) );

    put_rr_header( &writer, host_offset, MDNS_TYPE_A, DNS_CLASS_IN | DNS_CACHE_FLUSH, unique_ttl, &length_offset );
    patch_u16( &writer, length_offset, 4U ); put_bytes( &writer, config->ipv4, sizeof( config->ipv4 ) );
    return writer.ok ? writer.position : 0U;
}

static bool query_matches( const MdnsServiceConfig * config, const uint8_t * query, size_t length )
{
    size_t position = DNS_HEADER_SIZE; uint16_t questions; char name[ 256 ];
    if( query == NULL || length < DNS_HEADER_SIZE || ( query[ 2 ] & 0x80U ) != 0U ) return false;
    questions = read_u16( query + 4U );
    if( questions == 0U || questions > 32U ) return false;
    for( uint16_t index = 0U; index < questions; index++ )
    {
        size_t consumed; uint16_t type, dns_class;
        if( !mdns_decode_name( query, length, position, name, sizeof( name ), &consumed ) ) return false;
        position += consumed; if( position + 4U > length ) return false;
        type = read_u16( query + position ); dns_class = ( uint16_t ) ( read_u16( query + position + 2U ) & 0x7fffU );
        position += 4U;
        if( dns_class == DNS_CLASS_IN && ( type == MDNS_TYPE_ANY ||
            ( type == MDNS_TYPE_PTR && same_name( name, config->service_type ) ) ||
            ( ( type == MDNS_TYPE_SRV || type == MDNS_TYPE_TXT ) && same_name( name, config->instance_name ) ) ||
            ( type == MDNS_TYPE_A && same_name( name, config->host_name ) ) ) ) return true;
    }
    return false;
}

size_t mdns_build_response( const MdnsServiceConfig * config, const uint8_t * query,
                            size_t query_length, uint8_t * output, size_t output_size )
{
    if( !config_valid( config ) || !query_matches( config, query, query_length ) ) return 0U;
    return build_records( config, MDNS_UNIQUE_TTL, output, output_size );
}

size_t mdns_build_announcement( const MdnsServiceConfig * config, uint32_t ttl,
                                uint8_t * output, size_t output_size )
{
    return build_records( config, ttl, output, output_size );
}

size_t mdns_build_probe( const MdnsServiceConfig * config, uint8_t * output, size_t output_size )
{
    Writer writer = { output, output_size, 0U, true };
    if( !config_valid( config ) || output == NULL || output_size < DNS_HEADER_SIZE ) return 0U;
    memset( output, 0, DNS_HEADER_SIZE ); writer.position = DNS_HEADER_SIZE; output[ 5 ] = 2U;
    put_name( &writer, config->host_name ); put_u16( &writer, MDNS_TYPE_A ); put_u16( &writer, DNS_CLASS_IN | DNS_CACHE_FLUSH );
    put_name( &writer, config->instance_name ); put_u16( &writer, MDNS_TYPE_SRV ); put_u16( &writer, DNS_CLASS_IN | DNS_CACHE_FLUSH );
    return writer.ok ? writer.position : 0U;
}

