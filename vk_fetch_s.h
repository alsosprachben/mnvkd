#ifndef VK_FETCH_S_H
#define VK_FETCH_S_H

#include <stdint.h>
#include <stddef.h>

#include "vk_socket.h"
#include "vk_enum.h"

enum HTTP_METHOD {
	CONNECT,
	DELETE,
	GET,
	HEAD,
	OPTIONS,
	PATCH,
	POST,
	PRI,
	PUT,
	TRACE,
	TRACK,
	NO_METHOD,
};
struct http_method {
	enum HTTP_METHOD method;
	char* repr;
	int list;
};

enum HTTP_METHOD_LIST {
	HTTP_METHOD_LIST_CORS_SAFE=1,
	HTTP_METHOD_LIST_FORBIDDEN=2,

};

static struct http_method methods[] = {
	{
		CONNECT,
		"CONNECT",
		HTTP_METHOD_LIST_FORBIDDEN,
	},
	{
		DELETE,
		"DELETE",
		0,
	},
	{
		GET,
		"GET",
		HTTP_METHOD_LIST_CORS_SAFE,
	},
	{
		HEAD,
		"HEAD",
		HTTP_METHOD_LIST_CORS_SAFE,
	},
	{
		OPTIONS,
		"OPTIONS",
		0,
	},
	{
		PATCH,
		"PATCH",
		0,
	},
	{
		POST,
		"POST",
		HTTP_METHOD_LIST_CORS_SAFE
	},
	{
		PRI,
		"PRI",
		0,
	},
	{
		PUT,
		"PUT",
		0,
	},
	{
		TRACE,
		"TRACE",
		HTTP_METHOD_LIST_FORBIDDEN,
	},
	{
		TRACK,
		"TRACK",
		HTTP_METHOD_LIST_FORBIDDEN,
	},
};

DEFINE_ENUM_INTERFACE(HTTP_METHOD, methods, method, repr, http_method);

enum HTTP_VERSION {
	HTTP09,
	HTTP10,
	HTTP11,
	HTTP20,
	NO_VERSION,
};
struct http_version {
	enum HTTP_VERSION version;
	char* repr;
};
static struct http_version versions[] = {
	{
		HTTP09,
		"HTTP/0.9",
	},
	{
		HTTP10,
		"HTTP/1.0",
	},
	{
		HTTP11,
		"HTTP/1.1",
	},
	{
		HTTP20,
		"HTTP/2.0",
	},
};

DEFINE_ENUM_INTERFACE(HTTP_VERSION, versions, version, repr, http_version);

static const char *http_default_server     = "mnvkd/0.9";
static const char *http_default_user_agent = "Mozilla/5.0 (POSIX.1-2017) mnvkd/0.9";
static const char *http_default_accept_doc = "text/html,application/xhtml+xml,application/xml;q=0.9,*/*;q=0.8";
static const char *http_default_accept_api = "application/json,application/xml;q=0.9,application/*;q=0.8,*/*;q=0.7";
static const char *http_default_accept_src = "image/*;q=0.9,video/*;q=0.9,*/*;q=0.8";
static const char *http_default_accept_enc = "identity;q=1.0";

enum HTTP_HEADER {
	ACCEPT_CHARSET,
	ACCEPT_ENCODING,
	ACCESS_CONTROL_REQUEST_HEADERS,
	ACCESS_CONTROL_REQUEST_METHOD,
	ORIGIN,
	AUTHORIZATION,
	CONNECTION,
	CONTENT_ENCODING,
	CONTENT_LANGUAGE,
	CONTENT_LENGTH,
	CONTENT_LOCATION,
	CONTENT_TYPE,
	COOKIE,
	COOKIE2,
	DATE,
	DNT,
	EXPECT,
	HOST,
	KEEP_ALIVE,
	REFERER,
	RANGE,
	SETCOOKIE,
	SETCOOKIE2,
	TE,
	TRAILER,
	TRANSFER_ENCODING,
	UPGRADE,
	VIA,
	PROXY,
	SEC,
	X_HTTP_METHOD,
	X_HTTP_METHOD_OVERRIDE,
	X_METHOD_OVERRIDE,
	HTTP_HEADER_COUNT,
};
enum vk_header_list {
	HTTP_HEADER_NONWC=1,
	HTTP_HEADER_NOCORS=2,
	HTTP_HEADER_CORS_SAFE=4,
	HTTP_HEADER_NOCORS_SAFE=8,
	HTTP_HEADER_FORBIDDEN=16,
	HTTP_HEADER_CORS_FORBIDDEN=32,
	HTTP_HEADER_COOKIE_FORBIDDEN=64,
	HTTP_HEADER_COOKIE_RESPONSE_FORBIDDEN=128,
	HTTP_HEADER_REQUEST_BODY=256,
	HTTP_HEADER_METHOD_FORBIDDEN=512,
};
struct http_header {
	enum HTTP_HEADER http_header;
	char* repr;
	enum vk_header_list list;
};
static struct http_header http_headers[] = {
	{
		ACCEPT_CHARSET,
		"Accept-Charset",
		HTTP_HEADER_FORBIDDEN,
		},
	{
		ACCEPT_ENCODING,
		"Accept-Encoding",
		HTTP_HEADER_FORBIDDEN,
	},
	{
		ACCESS_CONTROL_REQUEST_HEADERS,
		"Access-Control-Request-Headers",
		HTTP_HEADER_CORS_FORBIDDEN,
	},
	{
		ACCESS_CONTROL_REQUEST_METHOD,
		"Access-Control-Request-Method",
		HTTP_HEADER_CORS_FORBIDDEN,
		},
	{
		AUTHORIZATION,
		"Authorization",
		HTTP_HEADER_NONWC,
	},
	{
		CONNECTION,
		"Connection",
		HTTP_HEADER_FORBIDDEN,
		},
	{
		CONTENT_ENCODING,
		"Content-Encoding",
		HTTP_HEADER_REQUEST_BODY,
	},
	{
		CONTENT_LANGUAGE,
		"Content-Language",
		HTTP_HEADER_REQUEST_BODY,
	},
	{
		CONTENT_LENGTH,
		"Content-Length",
		HTTP_HEADER_FORBIDDEN,
		},
	{
		CONTENT_LOCATION,
		"Content-Location",
		HTTP_HEADER_REQUEST_BODY,
	},
	{
		CONTENT_TYPE,
		"Content-Type",
		HTTP_HEADER_REQUEST_BODY,
	},
	{
		COOKIE,
		"Cookie",
		HTTP_HEADER_COOKIE_FORBIDDEN,
		},
	{
		COOKIE2,
		"Cookie2",
		HTTP_HEADER_COOKIE_FORBIDDEN,
		},
	{
		DATE,
		"Date",
		HTTP_HEADER_FORBIDDEN,
		},
	{
		DNT,
		"DNT",
		HTTP_HEADER_FORBIDDEN,
		},
	{
		EXPECT,
		"Expect",
		HTTP_HEADER_FORBIDDEN,
		},
	{
		HOST,
		"Host",
		HTTP_HEADER_FORBIDDEN,
		},
	{
		KEEP_ALIVE,
		"Keep-Alive",
		HTTP_HEADER_FORBIDDEN,
		},
	{
		PROXY,
		"proxy-%",
		HTTP_HEADER_FORBIDDEN,
	},
	{
		RANGE,
		"Range",
		HTTP_HEADER_NOCORS,
	},
	{
		REFERER,
		"Referer",
		HTTP_HEADER_FORBIDDEN,
		},
	{
		SEC,
		"sec-%",
		HTTP_HEADER_FORBIDDEN,
	},
	{
		SETCOOKIE,
		"Set-Cookie",
		HTTP_HEADER_COOKIE_FORBIDDEN | HTTP_HEADER_COOKIE_RESPONSE_FORBIDDEN,
		},
	{
		SETCOOKIE2,
		"Set-Cookie2",
		HTTP_HEADER_COOKIE_RESPONSE_FORBIDDEN,
		},
	{
		TE,
		"TE",
		HTTP_HEADER_FORBIDDEN,
	},
	{
		TRAILER,
		"Trailer",
		0,
	},
	{
		TRANSFER_ENCODING,
		"Transfer-Encoding",
		HTTP_HEADER_FORBIDDEN,
	},
	{
		UPGRADE,
		"Upgrade",
		HTTP_HEADER_FORBIDDEN,
		},
	{
		VIA,
		"Via",
		HTTP_HEADER_FORBIDDEN,
		},
	{
		X_HTTP_METHOD,
		"X-HTTP-Method",
		HTTP_HEADER_METHOD_FORBIDDEN,
	},
	{
		X_HTTP_METHOD_OVERRIDE,
		"X-HTTP-Method-Override",
		HTTP_HEADER_METHOD_FORBIDDEN,
	},
	{
		X_METHOD_OVERRIDE,
		"X-Method-Override",
		HTTP_HEADER_METHOD_FORBIDDEN,
	},
};

DEFINE_ENUM_INTERFACE(HTTP_HEADER, http_headers, http_header, repr, http_header);


enum HTTP_TRAILER {
	POST_CONTENT_LENGTH,
	HTTP_TRAILER_COUNT,
};
struct http_trailer {
	enum HTTP_TRAILER http_trailer;
	char* repr;
	enum vk_header_list list;
};
static struct http_trailer http_trailers[] = {
	{
		POST_CONTENT_LENGTH,
		"Content-Length",
		HTTP_HEADER_FORBIDDEN,
	},
};

DEFINE_ENUM_INTERFACE(HTTP_TRAILER, http_trailers, http_trailer, repr, http_trailer);

struct header {
	char key[32];
	char val[96];
};

struct request {
	enum HTTP_METHOD method;
	char uri[1024];
	enum HTTP_VERSION version;
	struct header headers[15];
	int header_count;
	size_t content_length;
	int chunked;
	int close;
};


enum vk_headers_guard {
	VK_HEADERS_GUARD_IMMUTABLE,
	VK_HEADERS_GUARD_REQUEST,
	VK_HEADERS_GUARD_REQUEST_NO_CORS,
	VK_HEADERS_GUARD_RESPONSE,
	VK_HEADERS_GUARD_NONE,
};

struct vk_header {
	char key[32];
	char value[96];
};

struct vk_headers {
	struct vk_header headers[32]; /* 4096 bytes */
	enum vk_headers_guard headers_guard;
	size_t header_count;
};

/* Definition of the fetch structure */
struct vk_fetch {
	struct vk_headers headers;
	char method[10];
	char url[2048];
	int close;
	int chunked;
	size_t content_length;
};

enum vk_request_destination {
	VK_REQUEST_DESTINATION_NONE=0,
	VK_REQUEST_DESTINATION_AUDIO,
	VK_REQUEST_DESTINATION_AUDIOWORKLET,
	VK_REQUEST_DESTINATION_DOCUMENT,
	VK_REQUEST_DESTINATION_EMBED,
	VK_REQUEST_DESTINATION_FONT,
	VK_REQUEST_DESTINATION_FRAME,
	VK_REQUEST_DESTINATION_IFRAME,
	VK_REQUEST_DESTINATION_IMAGE,
	VK_REQUEST_DESTINATION_MANIFEST,
	VK_REQUEST_DESTINATION_OBJECT,
	VK_REQUEST_DESTINATION_PAINTWORKLET,
	VK_REQUEST_DESTINATION_REPORT,
	VK_REQUEST_DESTINATION_SCRIPT,
	VK_REQUEST_DESTINATION_SHAREDWORKER,
	VK_REQUEST_DESTINATION_STYLE,
	VK_REQUEST_DESTINATION_TRACK,
	VK_REQUEST_DESTINATION_VIDEO,
	VK_REQUEST_DESTINATION_WORKER,
	VK_REQUEST_DESTINATION_XSLT,
	VK_REQUEST_DESTINATION_COUNT,
};

enum vk_request_mode {
	VK_REQUEST_MODE_CORS,
	VK_REQUEST_MODE_NAVIGATE=0,
	VK_REQUEST_MODE_NO_CORS,
	VK_REQUEST_MODE_SAME_ORIGIN,
	VK_REQUEST_MODE_COUNT,
};

enum vk_request_credentials {
	VK_REQUEST_CREDENTIALS_INCLUDE,
	VK_REQUEST_CREDENTIALS_OMIT=0,
	VK_REQUEST_CREDENTIALS_SAME_ORIGIN,
	VK_REQUEST_CREDENTIALS_COUNT,
};

enum vk_request_cache {
	VK_REQUEST_CACHE_DEFAULT=0,
	VK_REQUEST_CACHE_FORCE_CACHE,
	VK_REQUEST_CACHE_NO_STORE,
	VK_REQUEST_CACHE_NO_CACHE,
	VK_REQUEST_CACHE_ONLY_IF_CACHED,
	VK_REQUEST_CACHE_RELOAD,
	VK_REQUEST_CACHE_COUNT,
};

enum vk_request_redirect {
	VK_REQUEST_REDIRECT_ERROR,
	VK_REQUEST_REDIRECT_FOLLOW=0,
	VK_REQUEST_REDIRECT_MANUAL,
	VK_REQUEST_REDIRECT_COUNT,
};

enum vk_request_duplex {
	VK_REQUEST_DUPLEX_HALF=0,
	VK_REQUEST_DUPLEX_COUNT,
};

enum vk_request_priority {
	VK_REQUEST_PRIORITY_AUTO,
	VK_REQUEST_PRIORITY_HIGH=0,
	VK_REQUEST_PRIORITY_LOW,
	VK_REQUEST_PRIORITY_COUNT,
};

struct vk_request {
	enum HTTP_METHOD method;
	char url[2048];
	int keepalive;
	struct vk_headers headers;
};

struct vk_response {

};


struct vk_url {

};

struct vk_params {

};




#endif /* VK_FETCH_S_H */