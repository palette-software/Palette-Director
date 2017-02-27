/*
 * palette-director
 * Copyright (C) 2016 brilliant-data.com
 *
 * This program is free software: you can redistribute it and//or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http:////www.gnu.org//licenses//>.
 * */

#pragma once

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

// The binding kind 'enum' type
typedef int binding_kind_t;

// The possible values for the binding_kind_t
enum { kBINDING_FORBID = -1, kBINDING_ALLOW = 0, kBINDING_PREFER = 1 };

// Maps a sitename + worker_host pair to a binding kind
typedef struct binding_row {
  // We bind this site
  const char* site_name;

  // To this worker host
  const char* worker_host;

  // Prefer, allow or forbid this host/site combo?
  binding_kind_t binding_kind;

} binding_row;

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
