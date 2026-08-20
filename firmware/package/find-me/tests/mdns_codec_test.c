#include "mdns_codec.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static const MdnsServiceConfig config = {
    "_device-setup._tcp.local",
    "PSoC 5LP._device-setup._tcp.local",
    "freertos-psoc5lp-001.local", "psoc5lp-001",
    "Workshop Finder",
    "FINDER-R01",
    { 192U, 168U, 1U, 42U }, 8080U
};

static bool contains( const uint8_t * data, size_t length, const char * text )
{
    size_t text_length = strlen( text );
    if( text_length > length ) return false;
    for( size_t index = 0U; index <= length - text_length; index++ )
        if( memcmp( data + index, text, text_length ) == 0 ) return true;
    return false;
}

static size_t make_ptr_query( uint8_t * output )
{
    const char * cursor = config.service_type;
    size_t position = 12U;
    memset( output, 0, 512U ); output[ 5 ] = 1U;
    while( *cursor != '\0' )
    {
        const char * dot = strchr( cursor, '.' );
        size_t length = dot == NULL ? strlen( cursor ) : ( size_t ) ( dot - cursor );
        output[ position++ ] = ( uint8_t ) length;
        memcpy( output + position, cursor, length ); position += length;
        if( dot == NULL ) break;
        cursor = dot + 1;
    }
    output[ position++ ] = 0U;
    output[ position++ ] = 0U; output[ position++ ] = MDNS_TYPE_PTR;
    output[ position++ ] = 0U; output[ position++ ] = 1U;
    return position;
}

int main( void )
{
    uint8_t packet[ 768 ], query[ 512 ];
    size_t length = mdns_build_announcement( &config, 120U, packet, sizeof( packet ) );
    assert( length > 0U );
    assert( contains( packet, length, "_device-setup" ) );
    assert( contains( packet, length, "id=psoc5lp-001" ) );
    assert( contains( packet, length, "name=Workshop Finder" ) );
    assert( contains( packet, length, "model=FINDER-R01" ) );
    assert( contains( packet, length, "api=1" ) );
    assert( contains( packet, length, "path=/api/info" ) );
    assert( contains( packet, length, "ws=/api/ws" ) );
    assert( contains( packet, length, "auth=/auth/authorize" ) );
    length = make_ptr_query( query );
    assert( mdns_build_response( &config, query, length, packet, sizeof( packet ) ) > 0U );
    query[ 12 ] = 0xc0U; query[ 13 ] = 12U;
    assert( mdns_build_response( &config, query, 18U, packet, sizeof( packet ) ) == 0U );
    puts( "mDNS codec tests passed" );
    return 0;
}

