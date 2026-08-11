#ifndef DEVICE_INFO_H
#define DEVICE_INFO_H

#include <stddef.h>

typedef struct
{
    const char * device_id;
    const char * device_name;
    const char * device_model;
} DeviceInfo;

size_t device_info_build_json( const DeviceInfo * info, char * output,
                               size_t output_size );

#endif /* DEVICE_INFO_H */

