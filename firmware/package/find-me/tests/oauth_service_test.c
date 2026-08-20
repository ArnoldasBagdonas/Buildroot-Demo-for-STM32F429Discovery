#include "oauth_service.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

static bool random_value( uint32_t * value )
{
    static uint32_t next = 0x12345678U;
    *value = next++;
    return true;
}

static OAuthPersistentState saved_state;
static bool state_saved;

static bool load_state( void * context, OAuthPersistentState * state )
{
    ( void ) context;
    if( !state_saved ) return false;
    memcpy( state, &saved_state, sizeof( *state ) );
    return true;
}

static bool save_state( void * context, const OAuthPersistentState * state )
{
    ( void ) context;
    memcpy( &saved_state, state, sizeof( saved_state ) );
    state_saved = true;
    return true;
}

int main( void )
{
    static const char query[] =
        "client_id=com.example.freertosonboarding&redirect_uri=freertosonboarding%3A%2F%2Foauth%2Fcallback"
        "&response_type=code&state=test-state&code_challenge=E9Melhoa2OwvFrEMTJguCHaoeK1t8URWbuGJSstw-cM"
        "&code_challenge_method=S256";
    static const char verifier[] = "dBjftJeZ4CVP-mB92K27uhbUJU1p1r_wW1gFWFOEjXk";
    OAuthService service;
    OAuthService restored;
    OAuthResponse response;
    char decision[ 512 ], token_form[ 384 ], restored_form[ 384 ];
    char code[ 32 ], refresh[ 32 ], old_refresh[ 32 ], old_owner_pin[ 9 ], generated_pin[ 9 ];
    const char * code_start;
    const char * code_end;

    oauth_service_init( &service, random_value );
    assert( oauth_service_restore( &service, load_state, save_state, NULL, 0U ) == false );
    oauth_authorize( &service, query, "Test Device", &response );
    assert( response.status == 200 );
    assert( strstr( response.body, "Confirm device" ) != NULL );
    assert( strstr( response.body, "blinking once" ) != NULL );
    assert( strstr( response.body, "Two flashes" ) != NULL );
    assert( strstr( response.body, "p.remove()" ) != NULL );
    assert( strstr( response.body, "#155eef" ) != NULL );
    assert( strstr( response.body, "#f4f7fb" ) != NULL );
    assert( strstr( response.body, "Use owner PIN" ) != NULL );
    assert( strstr( response.body, "name=client_id" ) == NULL );
    oauth_authorize( &service, "client_id=bad", "Test Device", &response );
    assert( response.status == 400 );

    ( void ) snprintf( decision, sizeof( decision ), "%s&decision=authorize", query );
    oauth_decide( &service, decision, strlen( decision ), 10U, &response );
    assert( response.status == 403 );
    oauth_confirm_physical( &service );
    oauth_confirmation_status( &service, &response );
    assert( strstr( response.body, "\"confirmed\":true" ) != NULL );
    assert( strstr( response.body, "owner_pin" ) != NULL );
    /* A lost/overlapped poll must not consume the only copy of the PIN. */
    oauth_confirmation_status( &service, &response );
    assert( strstr( response.body, service.pending_owner_pin ) != NULL );
    assert( service.owner_pin[ 0 ] == '\0' );
    assert( service.pending_owner_pin[ 0 ] != '\0' );
    ( void ) snprintf( generated_pin, sizeof( generated_pin ), "%s", service.pending_owner_pin );
    oauth_decide( &service, decision, strlen( decision ), 10U, &response );
    assert( response.status == 303 );
    assert( service.owner_pin[ 0 ] != '\0' );
    assert( service.pending_owner_pin[ 0 ] == '\0' );
    assert( state_saved );
    assert( strcmp( saved_state.owner_pin, service.owner_pin ) == 0 );
    code_start = strstr( response.location, "?code=" );
    assert( code_start != NULL );
    code_start += 6;
    code_end = strchr( code_start, '&' );
    assert( code_end != NULL && ( size_t ) ( code_end - code_start ) < sizeof( code ) );
    memcpy( code, code_start, ( size_t ) ( code_end - code_start ) );
    code[ code_end - code_start ] = '\0';
    ( void ) snprintf( token_form, sizeof( token_form ),
        "grant_type=authorization_code&code=%s&code_verifier=%s&client_id=com.example.freertosonboarding&redirect_uri=freertosonboarding%%3A%%2F%%2Foauth%%2Fcallback",
        code, verifier );
    oauth_token( &service, token_form, strlen( token_form ), 11U, &response );
    assert( response.status == 200 );
    assert( strstr( response.body, "access_token" ) != NULL );
    assert( strstr( response.body, generated_pin ) != NULL );
    assert( service.issued_owner_pin[ 0 ] == '\0' );
    code_start = strstr( response.body, "\"refresh_token\":\"" );
    assert( code_start != NULL );
    code_start += strlen( "\"refresh_token\":\"" );
    code_end = strchr( code_start, '"' );
    assert( code_end != NULL && ( size_t ) ( code_end - code_start ) < sizeof( refresh ) );
    memcpy( refresh, code_start, ( size_t ) ( code_end - code_start ) );
    refresh[ code_end - code_start ] = '\0';
    assert( state_saved );
    assert( strcmp( saved_state.client_id, "com.example.freertosonboarding" ) == 0 );
    oauth_service_init( &restored, random_value );
    assert( oauth_service_restore( &restored, load_state, save_state, NULL,
                                   ( uint64_t ) UINT32_MAX + 1000ULL ) );
    assert( strcmp( restored.owner_pin, service.owner_pin ) == 0 );
    ( void ) snprintf( restored_form, sizeof( restored_form ),
        "grant_type=refresh_token&refresh_token=%s&client_id=com.example.freertosonboarding", refresh );
    oauth_token( &restored, restored_form, strlen( restored_form ),
                 ( uint64_t ) UINT32_MAX + 1001ULL, &response );
    assert( response.status == 200 );
    ( void ) snprintf( old_refresh, sizeof( old_refresh ), "%s", refresh );
    oauth_token( &service, token_form, strlen( token_form ), 12U, &response );
    assert( response.status == 400 );
    ( void ) snprintf( token_form, sizeof( token_form ),
        "grant_type=refresh_token&refresh_token=%s&client_id=com.example.freertosonboarding", refresh );
    oauth_token( &service, token_form, strlen( token_form ), 1812U, &response );
    assert( response.status == 200 );
    assert( oauth_validate_access( &service, "invalid", 1812U ) == false );
    ( void ) snprintf( token_form, sizeof( token_form ),
        "grant_type=refresh_token&refresh_token=%s&client_id=com.example.freertosonboarding", old_refresh );
    oauth_token( &service, token_form, strlen( token_form ), 1813U, &response );
    assert( response.status == 400 );
    ( void ) snprintf( token_form, sizeof( token_form ),
        "grant_type=refresh_token&refresh_token=%s&client_id=wrong-client", refresh );
    oauth_token( &service, token_form, strlen( token_form ), 1813U, &response );
    assert( response.status == 400 );

    ( void ) snprintf( old_owner_pin, sizeof( old_owner_pin ), "%s", service.owner_pin );
    oauth_authorize( &service, query, "Test Device", &response );
    oauth_confirm_physical( &service );
    assert( service.pending_owner_pin[ 0 ] != '\0' );
    ( void ) snprintf( decision, sizeof( decision ), "%s&decision=deny", query );
    oauth_decide( &service, decision, strlen( decision ), 1814U, &response );
    assert( response.status == 303 );
    assert( strcmp( service.owner_pin, old_owner_pin ) == 0 );
    assert( strcmp( saved_state.owner_pin, old_owner_pin ) == 0 );
    assert( service.pending_owner_pin[ 0 ] == '\0' );
    puts( "OAuth service tests passed" );
    return 0;
}

