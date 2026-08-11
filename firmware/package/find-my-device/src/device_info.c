#include "device_info.h"

#include <stdio.h>

size_t device_info_build_json( const DeviceInfo * info, char * output,
                               size_t output_size )
{
    int length;
    if( info == NULL || info->device_id == NULL || info->device_name == NULL ||
        info->device_model == NULL || output == NULL || output_size == 0U ) return 0U;
    length = snprintf( output, output_size,
        "{\"api_version\":\"3\",\"device_id\":\"%s\",\"device_name\":\"%s\","
        "\"device_model\":\"%s\","
        "\"authorization_endpoint\":\"/auth/authorize\",\"token_endpoint\":\"/auth/token\","
        "\"registration_endpoint\":\"/api/mobile/registrations\","
        "\"websocket_path\":\"/api/ws\"}", info->device_id, info->device_name,
        info->device_model );
    if( length < 0 || ( size_t ) length >= output_size ) return 0U;
    return ( size_t ) length;
}
