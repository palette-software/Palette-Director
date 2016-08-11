#include "palette-director-types.h"
#include <mod_proxy.h>
#include <stdlib.h>
#include <string.h>

#ifdef _MSC_VER
#include "win/inet_ntop.h"
#else
#include <arpa/inet.h>
#endif

// implement the functions for these slice types

PAL__SLICE_TYPE_IMPL(binding_row, binding_rows);
PAL__SLICE_TYPE_IMPL(proxy_worker*, proxy_worker_slice);

enum {
  // The correct maximum IPv6 string length is 45 characters.
  kIP_BUFFER_SIZE = 46,
};

static const char* inet_ntop_strerror(int e) {
  switch (e) {
    case EAFNOSUPPORT:
      return "not a valid address family";
    case ENOSPC:
      return "The converted address string would exceed the size given by "
             "size.";
  }
  return NULL;
}

// Adds an IP address to the resolver
static void add_ip_to_resolver(ip_resolver_table* r, const char* hostname,
                               const char* ip) {
  size_t len = r->count;
  r->hostname[len] = hostname;
  r->ip_addr[len] = NULL;
  r->count = len + 1;
}

// Returns the ip address stored (or tries to look it up).
// If it cannot resolve the name to an ip, returns NULL and does
// not retry it until restart
const char* ip_resolver_lookup(ip_resolver_table* r, const char* hostname) {
  size_t i, len = r->count;

  // first check if we actually have it
  {
    for (i = 0; i < len; ++i) {
      // if the hostname matches, we have a winner
      if (strcmp(r->hostname[i], hostname) == 0) {
        return r->ip_addr[i];
      }
    }
  }

  // if we dont have it, look it up (the http service for now)
  {
    // int sockfd;
    struct addrinfo hints, *servinfo;
    int rv;

    char ip_buffer[kIP_BUFFER_SIZE];
    const char* ip_out = NULL;
    const char* addr_out = NULL;

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;  // use AF_INET6 to force IPv6
    hints.ai_socktype = SOCK_STREAM;

    // try to get the lookup
    if ((rv = getaddrinfo(hostname, "http", &hints, &servinfo)) != 0) {
      // put a null record for this address so we dont try to look it up later
      add_ip_to_resolver(r, hostname, NULL);

      ap_log_error(APLOG_MARK, APLOG_ERR, 0, ap_server_conf,
                   "Failed to look up hostname: '%s' reason: '%s'", hostname,
                   gai_strerror(rv));

      return NULL;
    }

    // Try to convert the addrinfo to IP address
    ip_out = inet_ntop(servinfo->ai_addr->sa_family, servinfo->ai_addr->sa_data,
                       ip_buffer, kIP_BUFFER_SIZE);
    if (ip_out == NULL) {
      ap_log_error(APLOG_MARK, APLOG_ERR, 0, ap_server_conf,
                   "Failed converting hostname '%s' to IP address reason: '%d'",
                   hostname, inet_ntop_strerror(errno));
      add_ip_to_resolver(r, hostname, NULL);
      return NULL;
    }

    // dupe the string
    addr_out = strdup(ip_out);

    // add the converted address to the list of IPs
    add_ip_to_resolver(r, hostname, addr_out);

    return addr_out;
  }
}
