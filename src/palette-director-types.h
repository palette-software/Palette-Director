#pragma once

#include <stddef.h>
#include <stdint.h>

#include "palette-macros.h"

#ifndef WIN32
typedef struct addrinfo {
  int             ai_flags;
  int             ai_family;
  int             ai_socktype;
  int             ai_protocol;
  size_t          ai_addrlen;
  char            *ai_canonname;
  struct sockaddr  *ai_addr;
  struct addrinfo  *ai_next;
} ADDRINFOA, *PADDRINFOA;
#endif

enum {
  // The size of the stack buffers we'll use in storing temporary bindings
  kBINDINGS_BUFFER_SIZE = 512,

  // The size of the stack buffer for the workers lists
  kWORKERS_BUFFER_SIZE = 256,

  // The buffer size for each substring we'll be reading from a line
  kCONFIG_MAX_STRING_SIZE = 256,
};

// The handler name we'll use to display the status pages
static const char* PALETTE_DIRECTOR_STATUS_HANDLER = "palette-director-status";

// The binding kind 'enum' type
typedef int binding_kind_t;

typedef struct binding_row {
  // For identity checks
  size_t row_id;

  // We bind this site
  const char* site_name;

  // To this worker host
  const char* worker_host;

  // Prefer, allow or forbid this host/site combo?
  binding_kind_t binding_kind;

} binding_row;

// The possible values for the 'binding' kind column in the config
enum { kBINDING_FORBID = -1, kBINDING_ALLOW = 0, kBINDING_PREFER = 1 };

// FWD-declare the proxy worker struct
typedef struct proxy_worker proxy_worker;

typedef struct ip_resolver_table {
  const char* hostname[kWORKERS_BUFFER_SIZE];
  const char* ip_addr[kWORKERS_BUFFER_SIZE];

  size_t count;
} ip_resolver_table;

// The slice types we'll use more often
PAL__SLICE_TYPE(binding_row, binding_rows);
PAL__SLICE_TYPE(proxy_worker*, proxy_worker_slice);

// Returns the ip address stored (or tries to look it up).
// If it cannot resolve the name to an ip, returns NULL and does
// not retry it until restart
const char* ip_resolver_lookup(ip_resolver_table* r, const char* hostname);
