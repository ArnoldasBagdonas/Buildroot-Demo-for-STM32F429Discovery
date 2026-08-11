#include "oauth_service.h"

#include <ctype.h>
#include <stdio.h>
#include <string.h>

typedef struct { uint32_t h[ 8 ]; uint64_t bits; uint8_t block[ 64 ]; size_t used; } Sha256;

static uint32_t rotate( uint32_t value, unsigned int count )
{
    return ( value >> count ) | ( value << ( 32U - count ) );
}

static void sha_block( Sha256 * context, const uint8_t block[ 64 ] )
{
    static const uint32_t constants[ 64 ] = {
        0x428a2f98U,0x71374491U,0xb5c0fbcfU,0xe9b5dba5U,0x3956c25bU,0x59f111f1U,0x923f82a4U,0xab1c5ed5U,
        0xd807aa98U,0x12835b01U,0x243185beU,0x550c7dc3U,0x72be5d74U,0x80deb1feU,0x9bdc06a7U,0xc19bf174U,
        0xe49b69c1U,0xefbe4786U,0x0fc19dc6U,0x240ca1ccU,0x2de92c6fU,0x4a7484aaU,0x5cb0a9dcU,0x76f988daU,
        0x983e5152U,0xa831c66dU,0xb00327c8U,0xbf597fc7U,0xc6e00bf3U,0xd5a79147U,0x06ca6351U,0x14292967U,
        0x27b70a85U,0x2e1b2138U,0x4d2c6dfcU,0x53380d13U,0x650a7354U,0x766a0abbU,0x81c2c92eU,0x92722c85U,
        0xa2bfe8a1U,0xa81a664bU,0xc24b8b70U,0xc76c51a3U,0xd192e819U,0xd6990624U,0xf40e3585U,0x106aa070U,
        0x19a4c116U,0x1e376c08U,0x2748774cU,0x34b0bcb5U,0x391c0cb3U,0x4ed8aa4aU,0x5b9cca4fU,0x682e6ff3U,
        0x748f82eeU,0x78a5636fU,0x84c87814U,0x8cc70208U,0x90befffaU,0xa4506cebU,0xbef9a3f7U,0xc67178f2U };
    uint32_t words[ 64 ], a, b, c, d, e, f, g, h;
    size_t index;
    for( index = 0; index < 16; index++ )
        words[ index ] = ( ( uint32_t ) block[ index * 4 ] << 24 ) |
                         ( ( uint32_t ) block[ index * 4 + 1 ] << 16 ) |
                         ( ( uint32_t ) block[ index * 4 + 2 ] << 8 ) | block[ index * 4 + 3 ];
    for( index = 16; index < 64; index++ )
    {
        uint32_t x = words[ index - 15 ], y = words[ index - 2 ];
        words[ index ] = words[ index - 16 ] + ( rotate( x, 7 ) ^ rotate( x, 18 ) ^ ( x >> 3 ) ) +
                         words[ index - 7 ] + ( rotate( y, 17 ) ^ rotate( y, 19 ) ^ ( y >> 10 ) );
    }
    a=context->h[0]; b=context->h[1]; c=context->h[2]; d=context->h[3];
    e=context->h[4]; f=context->h[5]; g=context->h[6]; h=context->h[7];
    for( index = 0; index < 64; index++ )
    {
        uint32_t s1=rotate(e,6)^rotate(e,11)^rotate(e,25), choose=(e&f)^(~e&g);
        uint32_t t1=h+s1+choose+constants[index]+words[index];
        uint32_t s0=rotate(a,2)^rotate(a,13)^rotate(a,22), majority=(a&b)^(a&c)^(b&c);
        uint32_t t2=s0+majority;
        h=g; g=f; f=e; e=d+t1; d=c; c=b; b=a; a=t1+t2;
    }
    context->h[0]+=a; context->h[1]+=b; context->h[2]+=c; context->h[3]+=d;
    context->h[4]+=e; context->h[5]+=f; context->h[6]+=g; context->h[7]+=h;
}

static void sha256( const char * input, uint8_t output[ 32 ] )
{
    Sha256 context = { { 0x6a09e667U,0xbb67ae85U,0x3c6ef372U,0xa54ff53aU,
                          0x510e527fU,0x9b05688cU,0x1f83d9abU,0x5be0cd19U }, 0, { 0 }, 0 };
    size_t length = strlen( input ), index;
    for( index = 0; index < length; index++ )
    {
        context.block[ context.used++ ] = ( uint8_t ) input[ index ]; context.bits += 8U;
        if( context.used == 64U ) { sha_block( &context, context.block ); context.used = 0; }
    }
    context.block[ context.used++ ] = 0x80U;
    if( context.used > 56U ) { while( context.used < 64U ) context.block[ context.used++ ] = 0; sha_block( &context, context.block ); context.used = 0; }
    while( context.used < 56U ) context.block[ context.used++ ] = 0;
    for( index = 0; index < 8; index++ ) context.block[ 63U - index ] = ( uint8_t ) ( context.bits >> ( index * 8U ) );
    sha_block( &context, context.block );
    for( index = 0; index < 32; index++ ) output[index]=(uint8_t)(context.h[index/4]>>(24U-(index%4)*8U));
}

static void hash_hex( const char * input, char output[ 65 ] )
{
    static const char digits[]="0123456789abcdef"; uint8_t digest[32]; size_t i; sha256(input,digest);
    for(i=0;i<32;i++){output[i*2]=digits[digest[i]>>4];output[i*2+1]=digits[digest[i]&15U];} output[64]='\0';
}

static void base64url( const uint8_t * input, size_t length, char * output )
{
    static const char table[]="ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_"; size_t i=0,o=0;
    while(i+2<length){uint32_t v=((uint32_t)input[i]<<16)|((uint32_t)input[i+1]<<8)|input[i+2]; output[o++]=table[v>>18];output[o++]=table[(v>>12)&63U];output[o++]=table[(v>>6)&63U];output[o++]=table[v&63U];i+=3;}
    if(i<length){uint32_t v=(uint32_t)input[i]<<16;output[o++]=table[v>>18];if(i+1<length){v|=(uint32_t)input[i+1]<<8;output[o++]=table[(v>>12)&63U];output[o++]=table[(v>>6)&63U];}else output[o++]=table[(v>>12)&63U];} output[o]='\0';
}

static bool secure_equal( const char * a, const char * b )
{
    size_t al=strlen(a),bl=strlen(b),n=al>bl?al:bl,i; unsigned int d=(unsigned int)(al^bl);
    for(i=0;i<n;i++) d|=(unsigned int)((i<al?(unsigned char)a[i]:0)^(i<bl?(unsigned char)b[i]:0));
    return d==0U;
}

static int hex_value( char c ){if(c>='0'&&c<='9')return c-'0';if(c>='a'&&c<='f')return c-'a'+10;if(c>='A'&&c<='F')return c-'A'+10;return -1;}
static bool form_value( const char * data,size_t length,const char * name,char * out,size_t size )
{
    size_t nl=strlen(name),p=0; while(p<length){size_t e=p;while(e<length&&data[e]!='&')e++;if(e>p+nl&&memcmp(data+p,name,nl)==0&&data[p+nl]=='='){size_t s=p+nl+1,t=0;while(s<e&&t+1<size){if(data[s]=='%'&&s+2<e){int h=hex_value(data[s+1]),l=hex_value(data[s+2]);if(h<0||l<0)return false;out[t++]=(char)(h*16+l);s+=3;}else{out[t++]=data[s]=='+'?' ':data[s];s++;}}if(s!=e||t==0)return false;out[t]='\0';return true;}p=e+1;}return false;
}
static bool safe( const char * value,size_t max ){size_t i,n=strlen(value);if(n==0||n>max)return false;for(i=0;i<n;i++){unsigned char c=(unsigned char)value[i];if(!(isalnum(c)||c=='-'||c=='_'||c=='.'||c=='~'))return false;}return true;}
static void respond( OAuthResponse * r,int status,const char * type,const char * body ){r->status=status;r->content_type=type;r->body_length=strlen(body);if(r->body_length>=sizeof(r->body))r->body_length=sizeof(r->body)-1;memcpy(r->body,body,r->body_length);r->body[r->body_length]='\0';}
static void error( OAuthResponse * r,int status,const char * code ){char b[128];(void)snprintf(b,sizeof(b),"{\"error\":\"%s\"}",code);respond(r,status,"application/json",b);}
static bool parameters( const char * data,size_t n,char client[64],char state[129],char challenge[44] ){char redirect[64],response[8],method[8];return form_value(data,n,"client_id",client,64)&&safe(client,63)&&form_value(data,n,"redirect_uri",redirect,64)&&strcmp(redirect,OAUTH_CALLBACK_URI)==0&&form_value(data,n,"response_type",response,8)&&strcmp(response,"code")==0&&form_value(data,n,"state",state,129)&&safe(state,128)&&form_value(data,n,"code_challenge",challenge,44)&&strlen(challenge)==43&&safe(challenge,43)&&form_value(data,n,"code_challenge_method",method,8)&&strcmp(method,"S256")==0;}
static bool token( OAuthService * s,char output[23] ){uint8_t bytes[16];size_t i;uint32_t v;for(i=0;i<4;i++){if(s->random==NULL||!s->random(&v))return false;memcpy(bytes+i*4,&v,4);}base64url(bytes,16,output);return true;}
static bool valid_owner_pin( const char * pin )
{
    size_t i;
    if(pin[0]=='\0')return true;
    if(strlen(pin)!=8U)return false;
    for(i=0U;i<8U;i++)
        if(pin[i]<'0'||pin[i]>'9')return false;
    return true;
}
static bool persist_state( OAuthService * s,const char * client,const char * refresh,const char * pin )
{
    OAuthPersistentState state;if(s->save==NULL)return false;memset(&state,0,sizeof(state));
    (void)snprintf(state.client_id,sizeof(state.client_id),"%s",client);
    (void)snprintf(state.refresh_hash,sizeof(state.refresh_hash),"%s",refresh);
    (void)snprintf(state.owner_pin,sizeof(state.owner_pin),"%s",pin);
    return s->save(s->persistence_context,&state);
}

void oauth_service_init( OAuthService * service,OAuthRandom random ){memset(service,0,sizeof(*service));service->random=random;}
bool oauth_service_restore( OAuthService * s,OAuthPersistentLoad load,OAuthPersistentSave save,void * context,uint64_t now )
{
    OAuthPersistentState state;bool credentials_valid;
    s->save=save;s->persistence_context=context;
    if(load==NULL||!load(context,&state))return false;
    if(memchr(state.client_id,'\0',sizeof(state.client_id))==NULL||memchr(state.refresh_hash,'\0',sizeof(state.refresh_hash))==NULL||memchr(state.owner_pin,'\0',sizeof(state.owner_pin))==NULL||!valid_owner_pin(state.owner_pin))return false;
    credentials_valid=state.client_id[0]!='\0'&&strlen(state.refresh_hash)==64U;
    if(!credentials_valid&&(state.client_id[0]!='\0'||state.refresh_hash[0]!='\0'))return false;
    (void)snprintf(s->owner_pin,sizeof(s->owner_pin),"%s",state.owner_pin);
    if(credentials_valid){(void)snprintf(s->client_id,sizeof(s->client_id),"%s",state.client_id);(void)snprintf(s->refresh_hash,sizeof(s->refresh_hash),"%s",state.refresh_hash);s->refresh_expires_at=now+2592000ULL;}
    return credentials_valid||s->owner_pin[0]!='\0';
}
void oauth_authorize( OAuthService * service,const char * query,const char * device_name,OAuthResponse * r )
{
    static const char page[]=
        "<!doctype html><meta name=viewport content=\"width=device-width\"><style>"
        "body{margin:0;background:#f4f7fb;color:#101828;font:16px Arial}main{max-width:400px;margin:auto;padding:44px 24px}b,.x{color:#155eef}h1{font-size:32px;margin:12px 0}p{color:#475467;line-height:24px}form{background:#fff;border-radius:16px;padding:22px}input,button{box-sizing:border-box;width:100%;font:inherit;border-radius:12px;padding:15px;margin-top:12px}input{border:1px solid #98a2b3}button{border:0;background:#155eef;color:#fff;font-weight:bold}.x{background:none}strong,small{display:block;margin:14px 0}strong{color:#155eef;font-size:28px;letter-spacing:3px}small{color:#667085;font-size:13px}</style>"
        "<main><b>FIND MY DEVICE</b><h1>Confirm device</h1><p id=m>Find the device blinking once; press its button.</p><form method=post action=/auth/decision>"
        "<span id=p><label>Owner PIN</label><input type=password inputmode=numeric name=owner_pin placeholder=\"8-digit PIN\"><button name=decision value=authorize>Use owner PIN</button></span><button class=x id=c name=decision value=deny>Cancel setup</button></form></main>"
        "<script>let q=setInterval(async()=>{let j=await(await fetch('/auth/confirmation/status')).json();if(j.confirmed&&!window.d){d=1;clearInterval(q);m.innerHTML=j.owner_pin?'Two flashes: save PIN, then Continue.<strong>'+j.owner_pin+'</strong><small>Valid after restart.</small>':'Two flashes: press Continue.';p.remove();c.insertAdjacentHTML('beforebegin','<button name=decision value=authorize>Continue</button>')}},750)</script>";
    char client[64],state[129],challenge[44];size_t length=sizeof(page)-1U;(void)device_name;memset(r,0,sizeof(*r));
    if(query==NULL||!parameters(query,strlen(query),client,state,challenge)){error(r,400,"invalid_request");return;}
    (void)snprintf(service->pending_client_id,sizeof(service->pending_client_id),"%s",client);(void)snprintf(service->pending_state,sizeof(service->pending_state),"%s",state);(void)snprintf(service->challenge,sizeof(service->challenge),"%s",challenge);
    service->confirmation_waiting=true;service->physical_confirmed=false;service->owner_pin_pending_display=false;memset(service->pending_owner_pin,0,sizeof(service->pending_owner_pin));memset(service->issued_owner_pin,0,sizeof(service->issued_owner_pin));
    if(length>=sizeof(r->body)){error(r,500,"server_error");return;}memcpy(r->body,page,length+1U);
    r->status=200;r->content_type="text/html; charset=utf-8";r->body_length=(size_t)length;
}
void oauth_decide( OAuthService * s,const char * form,size_t n,uint64_t now,OAuthResponse * r )
{
    char client[64],state[129],decision[16],code[23],pin[16]={0};bool pin_ok;memset(r,0,sizeof(*r));
    if(!s->confirmation_waiting||!form_value(form,n,"decision",decision,sizeof(decision))){error(r,400,"invalid_request");return;}
    (void)snprintf(client,sizeof(client),"%s",s->pending_client_id);(void)snprintf(state,sizeof(state),"%s",s->pending_state);
    if(strcmp(decision,"deny")==0){s->confirmation_waiting=false;s->physical_confirmed=false;memset(s->pending_owner_pin,0,sizeof(s->pending_owner_pin));memset(s->issued_owner_pin,0,sizeof(s->issued_owner_pin));s->owner_pin_pending_display=false;r->status=303;r->content_type="text/plain";(void)snprintf(r->location,sizeof(r->location),"%s?error=access_denied&state=%s",OAUTH_CALLBACK_URI,state);respond(r,303,"text/plain","");return;}
    pin_ok=form_value(form,n,"owner_pin",pin,sizeof(pin))&&s->owner_pin[0]!='\0'&&secure_equal(pin,s->owner_pin);
    if(strcmp(decision,"authorize")!=0){error(r,400,"invalid_request");return;}
    if(!s->physical_confirmed&&!pin_ok){error(r,403,"physical_confirmation_or_owner_pin_required");return;}
    if(!token(s,code)){error(r,500,"server_error");return;}if(s->physical_confirmed&&s->pending_owner_pin[0]!='\0'){if(!persist_state(s,s->client_id,s->refresh_hash,s->pending_owner_pin)){error(r,500,"persistent_storage_error");return;}memcpy(s->owner_pin,s->pending_owner_pin,sizeof(s->owner_pin));memcpy(s->issued_owner_pin,s->pending_owner_pin,sizeof(s->issued_owner_pin));}memset(s->pending_owner_pin,0,sizeof(s->pending_owner_pin));s->owner_pin_pending_display=false;s->confirmation_waiting=false;s->physical_confirmed=false;hash_hex(code,s->code_hash);(void)snprintf(s->client_id,sizeof(s->client_id),"%s",client);s->code_expires_at=now+300U;(void)snprintf(r->location,sizeof(r->location),"%s?code=%s&state=%s",OAUTH_CALLBACK_URI,code,state);respond(r,303,"text/plain","");
}
void oauth_token( OAuthService * s,const char * form,size_t n,uint64_t now,OAuthResponse * r )
{
    char grant[32],client[64],code[32],verifier[129],redirect[64],hash[65],challenge[44],access[23],refresh[23],access_hash[65],refresh_hash[65],body[192];uint8_t digest[32];bool valid;memset(r,0,sizeof(*r));
    if(!form_value(form,n,"grant_type",grant,sizeof(grant))||!form_value(form,n,"client_id",client,sizeof(client))){error(r,400,"invalid_request");return;}
    if(strcmp(grant,"refresh_token")==0)
    {
        valid=form_value(form,n,"refresh_token",code,sizeof(code));hash_hex(valid?code:"",hash);
        valid=valid&&s->refresh_hash[0]!='\0'&&now<=s->refresh_expires_at&&strcmp(client,s->client_id)==0&&secure_equal(hash,s->refresh_hash);
        if(!valid){error(r,400,"invalid_grant");return;}
        if(!token(s,access)||!token(s,refresh)){error(r,500,"server_error");return;}
        hash_hex(access,access_hash);hash_hex(refresh,refresh_hash);if(!persist_state(s,s->client_id,refresh_hash,s->owner_pin)){error(r,500,"server_error");return;}memcpy(s->access_hash,access_hash,sizeof(access_hash));memcpy(s->refresh_hash,refresh_hash,sizeof(refresh_hash));s->access_expires_at=now+1800ULL;s->refresh_expires_at=now+2592000ULL;
        (void)snprintf(body,sizeof(body),"{\"access_token\":\"%s\",\"refresh_token\":\"%s\",\"token_type\":\"Bearer\",\"expires_in\":1800}",access,refresh);respond(r,200,"application/json",body);return;
    }
    if(strcmp(grant,"authorization_code")!=0){error(r,400,"unsupported_grant_type");return;}
    valid=form_value(form,n,"code",code,sizeof(code))&&form_value(form,n,"code_verifier",verifier,sizeof(verifier))&&form_value(form,n,"redirect_uri",redirect,sizeof(redirect));hash_hex(valid?code:"",hash);sha256(verifier,digest);base64url(digest,32,challenge);
    valid=valid&&s->code_hash[0]!='\0'&&now<=s->code_expires_at&&secure_equal(hash,s->code_hash)&&strcmp(client,s->client_id)==0&&strcmp(redirect,OAUTH_CALLBACK_URI)==0&&secure_equal(challenge,s->challenge);memset(s->code_hash,0,sizeof(s->code_hash));if(!valid){error(r,400,"invalid_grant");return;}
    if(!token(s,access)||!token(s,refresh)){error(r,500,"server_error");return;}hash_hex(access,access_hash);hash_hex(refresh,refresh_hash);if(!persist_state(s,client,refresh_hash,s->owner_pin)){error(r,500,"server_error");return;}memcpy(s->access_hash,access_hash,sizeof(access_hash));memcpy(s->refresh_hash,refresh_hash,sizeof(refresh_hash));s->access_expires_at=now+1800ULL;s->refresh_expires_at=now+2592000ULL;if(s->issued_owner_pin[0]!='\0')(void)snprintf(body,sizeof(body),"{\"access_token\":\"%s\",\"refresh_token\":\"%s\",\"token_type\":\"Bearer\",\"expires_in\":1800,\"owner_pin\":\"%s\"}",access,refresh,s->issued_owner_pin);else (void)snprintf(body,sizeof(body),"{\"access_token\":\"%s\",\"refresh_token\":\"%s\",\"token_type\":\"Bearer\",\"expires_in\":1800}",access,refresh);memset(s->issued_owner_pin,0,sizeof(s->issued_owner_pin));respond(r,200,"application/json",body);
}
bool oauth_validate_access( const OAuthService * s,const char * token_value,uint64_t now ){char hash[65];if(token_value==NULL||s->access_hash[0]=='\0'||now>s->access_expires_at)return false;hash_hex(token_value,hash);return secure_equal(hash,s->access_hash);}
void oauth_confirm_physical( OAuthService * s )
{
    uint32_t value;if(s==NULL||!s->confirmation_waiting||s->physical_confirmed)return;s->physical_confirmed=true;
    if(s->random!=NULL&&s->random(&value)){(void)snprintf(s->pending_owner_pin,sizeof(s->pending_owner_pin),"%08lu",(unsigned long)(value%100000000UL));s->owner_pin_pending_display=true;}
}
void oauth_confirmation_status( OAuthService * s,OAuthResponse * r )
{
    char body[96];const char * pin="";memset(r,0,sizeof(*r));if(s->physical_confirmed&&s->owner_pin_pending_display)pin=s->pending_owner_pin;
    (void)snprintf(body,sizeof(body),"{\"confirmed\":%s,\"owner_pin\":\"%s\"}",s->physical_confirmed?"true":"false",pin);respond(r,200,"application/json",body);
}

