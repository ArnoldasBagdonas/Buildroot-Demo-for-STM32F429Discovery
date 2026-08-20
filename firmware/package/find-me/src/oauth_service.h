#ifndef OAUTH_SERVICE_H
#define OAUTH_SERVICE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define OAUTH_CALLBACK_URI "freertosonboarding://oauth/callback"

typedef bool ( * OAuthRandom )( uint32_t * value );

typedef struct
{
    char client_id[ 64 ];
    char refresh_hash[ 65 ];
    char owner_pin[ 9 ];
} OAuthPersistentState;

typedef bool ( * OAuthPersistentLoad )( void * context, OAuthPersistentState * state );
typedef bool ( * OAuthPersistentSave )( void * context, const OAuthPersistentState * state );

typedef struct
{
    char code_hash[ 65 ];
    char client_id[ 64 ];
    char challenge[ 44 ];
    char pending_client_id[ 64 ];
    char pending_state[ 129 ];
    uint64_t code_expires_at;
    char access_hash[ 65 ];
    char refresh_hash[ 65 ];
    uint64_t access_expires_at;
    uint64_t refresh_expires_at;
    OAuthRandom random;
    OAuthPersistentSave save;
    void * persistence_context;
    volatile bool confirmation_waiting;
    volatile bool physical_confirmed;
    char owner_pin[ 9 ];
    char pending_owner_pin[ 9 ];
    char issued_owner_pin[ 9 ];
    bool owner_pin_pending_display;
} OAuthService;

typedef struct
{
    int status;
    const char * content_type;
    char location[ 256 ];
    char body[ 1536 ];
    size_t body_length;
} OAuthResponse;

void oauth_service_init( OAuthService * service, OAuthRandom random );
bool oauth_service_restore( OAuthService * service, OAuthPersistentLoad load,
                            OAuthPersistentSave save, void * context,
                            uint64_t now_seconds );
void oauth_authorize( OAuthService * service, const char * query,
                      const char * device_name, OAuthResponse * response );
void oauth_decide( OAuthService * service, const char * form, size_t length,
                   uint64_t now_seconds, OAuthResponse * response );
void oauth_token( OAuthService * service, const char * form, size_t length,
                  uint64_t now_seconds, OAuthResponse * response );
bool oauth_validate_access( const OAuthService * service, const char * token,
                            uint64_t now_seconds );
void oauth_confirm_physical( OAuthService * service );
void oauth_confirmation_status( OAuthService * service, OAuthResponse * response );

#endif

