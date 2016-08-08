#ifndef HTTPD_PALETTE_DIRECTOR_TYPES_H
#define HTTPD_PALETTE_DIRECTOR_TYPES_H

#include <stddef.h>
#include <stdint.h>

#include "palette-macros.h"

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

typedef struct binding_row {
  // For identity checks
  size_t row_id;

  // We bind this site
  const char* site_name;

  // To this worker host
  const char* worker_host;

  // The base priority for the route.
  int priority;

  // True if this is a fallback route so we dont need to do
  // site name matching (and these routes are always lower priority
  // then the named routes)
  int is_fallback;

} binding_row;

typedef struct proxy_worker proxy_worker;

PAL__SLICE_TYPE(binding_row, binding_rows);
PAL__SLICE_TYPE(proxy_worker*, proxy_worker_slice);

typedef struct matched_workers_lists {
  proxy_worker_slice dedicated;
  proxy_worker_slice fallback;
} matched_workers_lists;

#endif  // HTTPD_PALETTE_DIRECTOR_TYPES_H
