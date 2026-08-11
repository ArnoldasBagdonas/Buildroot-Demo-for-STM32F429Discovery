#include "device_info.h"

#include <assert.h>
#include <string.h>

int main( void )
{
    char output[ 512 ];
    DeviceInfo info = { "mdns-test-001", "My Finder", "FINDER-R01" };
    size_t length = device_info_build_json( &info, output, sizeof( output ) );
    assert( length == strlen( output ) );
    assert( strstr( output, "\"api_version\":\"3\"") != NULL );
    assert( strstr( output, "\"device_id\":\"mdns-test-001\"") != NULL );
    assert( strstr( output, "\"device_model\":\"FINDER-R01\"") != NULL );
    assert( strstr( output, "\"websocket_path\":\"/api/ws\"") != NULL );
    assert( device_info_build_json( &info, output, 8U ) == 0U );
    return 0;
}

