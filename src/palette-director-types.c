#pragma once

#include "palette-director-types.h"
#include <stdlib.h>
#include <string.h>

// implement the functions for these slice types

PAL__SLICE_TYPE_IMPL(binding_row, binding_rows);
PAL__SLICE_TYPE_IMPL(proxy_worker*, proxy_worker_slice);