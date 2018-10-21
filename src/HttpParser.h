#ifndef HTTP_PARSER_H
#define HTTP_PARSER_H

#define HTTP_PARSER_VERSION_MAJOR 1
#define HTTP_PARSER_VERSION_MINOR 0

#include <sys/types.h>
#if defined(_WIN32) && !defined(__MINGW32__) && \
  (!defined(_MSC_VER) || _MSC_VER<1600) && !defined(__WINE__)
#include <BaseTsd.h>
#include <stddef.h>
typedef __int8 int8_t;
typedef unsigned __int8 uint8_t;
typedef __int16 int16_t;
typedef unsigned __int16 uint16_t;
typedef __int32 int32_t;
typedef unsigned __int32 uint32_t;
typedef __int64 int64_t;
typedef unsigned __int64 uint64_t;
#else
#include <stdint.h>
#endif

#include <unordered_map>

#include "Domain.h"
#include "DynamicPage.h"
#include "ParserDetails.h"
#include "HttpRequest.h"

namespace awsim {

/* Compile with -DHTTP_PARSER_STRICT=1 to parse URLs and hostnames
 * strictly according to the RFCs
 */
#ifndef HTTP_PARSER_STRICT
# define HTTP_PARSER_STRICT 0
#endif

/* Compile with -DHTTP_PARSER_DEBUG=1 to add extra debugging information to
 * the error reporting facility.
 */
#ifndef HTTP_PARSER_DEBUG
# define HTTP_PARSER_DEBUG 0
#endif


/* Maximium header size allowed */
#define HTTP_MAX_HEADER_SIZE (80*1024)


typedef struct HttpParser HttpParser;
typedef struct HttpParserSettings HttpParserSettings;
typedef struct ParserDetails ParserDetails;
typedef struct http_parser_result http_parser_result;


/* Callbacks should return non-zero to indicate an error. The parser will
 * then halt execution.
 *
 * The one exception is on_headers_complete. In a HTTP_RESPONSE parser
 * returning '1' from on_headers_complete will tell the parser that it
 * should not expect a body. This is used when receiving a response to a
 * HEAD request which may contain 'Content-Length' or 'Transfer-Encoding:
 * chunked' headers that indicate the presence of a body.
 *
 * http_data_cb does not return data chunks. It will be call arbitrarally
 * many times for each string. E.G. you might get 10 callbacks for "on_path"
 * each providing just a few characters more data.
 */
typedef int (*http_data_cb) (HttpRequest*, ParserDetails*, HttpParser*,
  const char *at, size_t length);
typedef int (*http_cb) (HttpRequest*, ParserDetails*, HttpParser*);


/* Request Methods */
enum class HTTPRequestMethod
{
  DELETE = 0,
  GET = 1,
  HEAD = 2,
  POST = 3,
  PUT = 4,
  /* pathological */
  CONNECT = 5,
  OPTIONS = 6,
  TRACE = 7,
  /* webdav */
  COPY = 8,
  LOCK =  9,
  MKCOL = 10,
  MOVE = 11,
  PROPFIND = 12,
  PROPPATCH = 13,
  UNLOCK = 14,
  /* subversion */
  REPORT = 15,
  MKACTIVITY = 16,
  CHECKOUT = 17,
  MERGE = 18,
  /* upnp */
  MSEARCH = 19,
  NOTIFY = 20,
  SUBSCRIBE = 21,
  UNSUBSCRIBE = 22,
  /* RFC-5789 */
  PATCH = 23
};


enum http_parser_type { HTTP_REQUEST, HTTP_RESPONSE, HTTP_BOTH };


/* Flag values for HttpParser.flags field */
enum flags
  { F_CHUNKED               = 1 << 0
  , F_TRAILING              = 1 << 3
  , F_UPGRADE               = 1 << 4
  , F_SKIPBODY              = 1 << 5
  };


/* Map for errno-related constants
 *
 * The provided argument should be a macro that takes 2 arguments.
 */
#define HTTP_ERRNO_MAP(XX)                                           \
  /* No error */                                                     \
  XX(OK, "success")                                                  \
                                                                     \
  /* Callback-related errors */                                      \
  XX(CB_message_begin, "the on_message_begin callback failed")       \
  XX(CB_path, "the on_path callback failed")                         \
  XX(CB_query_string, "the on_query_string callback failed")         \
  XX(CB_url, "the on_url callback failed")                           \
  XX(CB_fragment, "the on_fragment callback failed")                 \
  XX(CB_header_field, "the on_header_field callback failed")         \
  XX(CB_header_value, "the on_header_value callback failed")         \
  XX(CB_headers_complete, "the on_headers_complete callback failed") \
  XX(CB_body, "the on_body callback failed")                         \
  XX(CB_message_complete, "the on_message_complete callback failed") \
  XX(CB_reason, "the on_reason callback failed")                     \
  XX(CB_chunk_header, "the on_chunk_header callback failed")         \
  XX(CB_chunk_complete, "the on_chunk_complete callback failed")     \
                                                                     \
  /* Parsing-related errors */                                       \
  XX(INVALID_EOF_STATE, "stream ended at an unexpected time")        \
  XX(HEADER_OVERFLOW,                                                \
     "too many header bytes seen; overflow detected")                \
  XX(CLOSED_CONNECTION,                                              \
     "data received after completed connection: close message")      \
  XX(INVALID_VERSION, "invalid HTTP version")                        \
  XX(INVALID_STATUS, "invalid HTTP status code")                     \
  XX(INVALID_METHOD, "invalid HTTP method")                          \
  XX(INVALID_URL, "invalid URL")                                     \
  XX(INVALID_HOST, "invalid host")                                   \
  XX(INVALID_PORT, "invalid port")                                   \
  XX(INVALID_PATH, "invalid path")                                   \
  XX(INVALID_QUERY_STRING, "invalid query string")                   \
  XX(INVALID_FRAGMENT, "invalid fragment")                           \
  XX(LF_EXPECTED, "LF character expected")                           \
  XX(INVALID_HEADER_TOKEN, "invalid character in header")            \
  XX(INVALID_CONTENT_LENGTH,                                         \
     "invalid character in content-length header")                   \
  XX(HUGE_CONTENT_LENGTH,                                            \
     "content-length header too large")                              \
  XX(INVALID_CHUNK_SIZE,                                             \
     "invalid character in chunk size header")                       \
  XX(HUGE_CHUNK_SIZE,                                                \
     "chunk header size too large")                                  \
  XX(INVALID_TRANSFER_ENCODING,                                      \
     "invalid character in transfer-encoding header")                \
  XX(INVALID_UPGRADE,                                                \
     "invalid character in upgrade header")                          \
  XX(INVALID_CONSTANT, "invalid constant string")                    \
  XX(INVALID_INTERNAL_STATE, "encountered unexpected internal state")\
  XX(STRICT, "strict mode assertion failed")                         \
  XX(PAUSED, "parser is paused")                                     \
  XX(UNKNOWN, "an unknown error occurred")


/* Define HPE_* values for each errno value above */
#define HTTP_ERRNO_GEN(n, s) HPE_##n,
enum http_errno {
  HTTP_ERRNO_MAP(HTTP_ERRNO_GEN)
};
#undef HTTP_ERRNO_GEN


/* Get an http_errno value from an HttpParser */
#define HTTP_PARSER_ERRNO(p)            ((enum http_errno) (p)->http_errno)

/* Get the line number that generated the current error */
#if HTTP_PARSER_DEBUG
#define HTTP_PARSER_ERRNO_LINE(p)       ((p)->error_lineno)
#else
#define HTTP_PARSER_ERRNO_LINE(p)       0
#endif

struct HttpParser {
  /** PRIVATE **/
  unsigned char type : 2;     /* enum http_parser_type */
  unsigned char flags : 6;    /* F_* values from 'flags' enum; semi-public */
  unsigned char state;        /* enum state from HttpParser.c */
  unsigned char header_state; /* enum header_state from HttpParser.c */
  unsigned char index;        /* index into current matcher */

  uint32_t nread;          /* # bytes read in various scenarios */
  int64_t content_length;  /* # bytes in body (0 if no Content-Length header) */

  /** READ-ONLY **/
  unsigned short http_major;
  unsigned short http_minor;
  unsigned short status_code; /* responses only */
  HTTPRequestMethod method;       /* requests only */
  unsigned char http_errno : 7;

  /* 1 = Upgrade header was present and the parser has exited because of that.
   * 0 = No upgrade header present.
   * Should be checked when http_parser_execute() returns in addition to
   * error checking.
   */
  char upgrade : 1;

#if HTTP_PARSER_DEBUG
  uint32_t error_lineno;
#endif

  /** PUBLIC **/
  void *data; /* A pointer to get hook to the "connection" or "socket" object */
};


struct HttpParserSettings {
  http_cb      on_message_begin;
  http_data_cb on_url;
  http_data_cb on_header_field;
  http_data_cb on_header_value;
  http_data_cb on_headers_complete;
  http_data_cb on_body;
  http_cb      on_message_complete;
  http_data_cb on_reason;
  /* When on_chunk_header is called, the current chunk length is stored
   * in parser->content_length.
   */
  http_cb      on_chunk_header;
  http_cb      on_chunk_complete;
};


enum http_parser_url_fields
  { UF_SCHEMA           = 0
  , UF_HOST             = 1
  , UF_PORT             = 2
  , UF_PATH             = 3
  , UF_QUERY            = 4
  , UF_FRAGMENT         = 5
  , UF_USERINFO         = 6
  , UF_MAX              = 7
};


/* Result structure for http_parser_parse_url().
 *
 * Callers should index into field_data[] with UF_* values iff field_set
 * has the relevant (1 << UF_*) bit set. As a courtesy to clients (and
 * because we probably have padding left over), we convert any port to
 * a uint16_t.
 */
struct http_parser_url {
  uint16_t field_set;           /* Bitmask of (1 << UF_*) values */
  uint16_t port;                /* Converted UF_PORT string */

  struct {
    uint16_t off;               /* Offset into buffer in which field starts */
    uint16_t len;               /* Length of run in buffer */
  } field_data[UF_MAX];
};


void http_parser_init(HttpParser *parser, enum http_parser_type type);


size_t http_parser_execute(HttpRequest *request,
                           ParserDetails *details,
                           HttpParser *parser,
                           const HttpParserSettings *settings,
                           const char *data,
                           size_t len);

/* Return a string name of the given error */
const char *http_errno_name(enum http_errno err);

/* Return a string description of the given error */
const char *http_errno_description(enum http_errno err);

/* Parse a URL; return nonzero on failure */
int http_parser_parse_url(const char *buf, size_t buflen,
                          int is_connect,
                          struct http_parser_url *u);

/* Pause or un-pause the parser; a nonzero value pauses */
void http_parser_pause(HttpParser *parser, int paused);

}

#endif
