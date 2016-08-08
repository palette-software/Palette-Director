//
// Created by Miles Gibson on 03/08/16.
//

#include <stdio.h>
#include <stdlib.h>

#include <mod_proxy.h>
#include <sys/stat.h>
#include "config-loader.h"
#include "csv/csv.h"
#include "mod_proxy.h"


/////////////////////////////////////////////////////////////////////////////

// Note: This function returns a pointer to a substring of the original string.
// If the given string was allocated dynamically, the caller must not overwrite
// that pointer with the returned value, since the original pointer must be
// deallocated using the same allocator with which it was allocated.  The return
// value must NOT be deallocated using free() etc.
static char* trimwhitespace(char* str) {
  char* end;

  // Trim leading space
  while (isspace(*str)) str++;

  if (*str == 0)  // All spaces?
    return str;

  // Trim trailing space
  end = str + strlen(str) - 1;
  while (end > str && isspace(*end)) end--;

  // Write new null terminator
  *(end + 1) = 0;

  return str;
}

// Loads a file into memory and returns a pointer
static char* load_file_into_memory(FILE* f, size_t* out_size) {
  char* string;
  long fsize;

  fseek(f, 0, SEEK_END);
  fsize = ftell(f);
  fseek(f, 0, SEEK_SET);  // same as rewind(f);

  string = (char*)malloc(fsize + 1);
  fread(string, fsize, 1, f);

  if (errno != 0) {
    ap_log_error(APLOG_MARK, APLOG_ERR, 0, ap_server_conf,
                 "File load error: '%s'", strerror(errno));
    return NULL;
  }

  fclose(f);

  *out_size = fsize;
  string[fsize] = 0;
  return string;
}

// Duplicates a string by duping num_chars chars (and a 0) only
const char* strndup(const char* str, size_t num_chars) {
  // clip the length
  const size_t copied_chars = min(num_chars, strlen(str));
  char* o = (char*)malloc(copied_chars + 1);

  strncpy(o, str, copied_chars);

  // null terminate it
  o[copied_chars] = '\0';

  return o;
}

/////////////////////////////////////////////////////////////////

// the indices of the read fields
enum {
  kiDX_SITE_NAME = 0,
  kIDX_WORKER_HOST_NAME = 1,
  kIDX_PRIORITY = 2,
  kIDX_KIND = 3,

  kIDX_INDEX_COUNT
};

// The state of our config loader funcion
typedef struct config_loader_state {
  // The buffer for the rows
  binding_row rows[kBINDINGS_BUFFER_SIZE];
  size_t row_count;

  // The current binding as it builds up
  binding_row current_row;

  // The thing that keeps our current field
  int state;

  // Marks if the current row is for VizQL (all or vizql as kind)
  int is_row_for_vizql;
} config_loader_state;

static int put_comma = 0;

// Handler on fields
void on_csv_cell(void* s, size_t i, void* p) {
  // get the state
  config_loader_state* state = (config_loader_state*)p;
  int state_idx = state->state;
  // clone the string, beacause all steps require a null-terminated string
  char* tmp = (char*)strndup((char*)s, i);

  // check which field we are at and handle it
  switch (state_idx) {
    case kiDX_SITE_NAME:
      // if the site name is a star, mark it as fallback
      if (strncmp(tmp, "*", i) == 0) {
        state->current_row.site_name = strdup("*");
        state->current_row.is_fallback = TRUE;
      } else {
        // if note, dupe the site name
        state->current_row.site_name = tmp;
        state->current_row.is_fallback = FALSE;
      }
      break;

    case kIDX_WORKER_HOST_NAME:
      state->current_row.worker_host = tmp;
      break;

    case kIDX_PRIORITY:
      state->current_row.priority = strtol(tmp, NULL, 10);
      free(tmp);
      break;

    case kIDX_KIND:
      state->is_row_for_vizql = (strncmp((char*)s, "all", i) == 0) ||
                                (strncmp((char*)s, "vizql", i) == 0);
      free(tmp);
      break;

    default:
      ap_log_error(APLOG_MARK, APLOG_ERR, 0, ap_server_conf,
                   "Extra columns in the config file: '%s'", tmp);
      free(tmp);
      break;
  };

  state_idx++;
  state->state = state_idx;
}

void on_csv_row_end(int c, void* p) {
  config_loader_state* state = (config_loader_state*)p;

  // check if we need to add this row to the state
  if (state->is_row_for_vizql) {
    state->rows[state->row_count] = state->current_row;
    state->row_count += 1;
  }

  // reset the state
  state->state = kiDX_SITE_NAME;
  state->is_row_for_vizql = FALSE;
}

/////////////////////////////////////////////////////////////////

binding_rows parse_csv_config(const char* path) {
  FILE* fp;
  int line_count = 1;
  // Set the default fallback to null
  const char* fallback_worker_host = NULL;
  // The number of records filled in buffer
  size_t loaded_site_count = 0;

  // struct stat sb;
  char* file_buffer;
  size_t file_size = 0;

  // Try to open the config file
  if ((fp = fopen(path, "rb")) == NULL) {
    ap_log_error(APLOG_MARK, APLOG_ERR, 0, ap_server_conf,
                 "Cannot open worker bindings config file '%s'", path);
    return empty_binding_rows;
  }

  // try to load the whole file
  file_buffer = load_file_into_memory(fp, &file_size);
  fclose(fp);

  if (file_buffer == NULL) {
    ap_log_error(APLOG_MARK, APLOG_ERR, 0, ap_server_conf,
                 "Cannot load worker bindings config file '%s'", path);
    return empty_binding_rows;
  }

  // Parse the file as csv
  {
    // init the parser and the state
    config_loader_state state = {0};
    struct csv_parser parser;

    csv_init(&parser, 0);

    // parse the CSV
    csv_parse(&parser, file_buffer, file_size, on_csv_cell, on_csv_row_end,
              &state);

    // flush the remaining data
    csv_fini(&parser, on_csv_cell, on_csv_row_end, &state);

    // clean up the parser
    csv_free(&parser);
    // clean up the buffer
    free(file_buffer);

    // build the output
    {
      size_t bytes_to_copy = sizeof(state.rows[0]) * state.row_count;
      // create the output
      binding_rows output = {(binding_row*)malloc(bytes_to_copy),
                             state.row_count};
      // copy the rows
      memcpy(output.entries, state.rows, bytes_to_copy);
      return output;
    }
  }
}

/////////////////////////////////////////////////////////

static int compare_bindings_row(const void* s1, const void* s2) {
  binding_row* e1 = (binding_row*)s1;
  binding_row* e2 = (binding_row*)s2;

  // identity
  if (e1->row_id == e2->row_id) return 0;

  // if one is a fallback and the other is not the non-fallback
  // always wins
  if (!e1->is_fallback && e2->is_fallback) return -1;
  if (e1->is_fallback && !e2->is_fallback) return 1;

  return e1->priority - e2->priority;
}

/*
 * Returns true if the worker matches the description of the binding_row b
 */
static int worker_matches_binding(const binding_row* b, const proxy_worker* w) {
  return (strcmp(b->worker_host, w->s->hostname) == 0) ? TRUE : FALSE;
}

static void build_worker_list(
    // INPUT
    proxy_worker** workers, size_t worker_count, binding_row* bindings,
    size_t bindings_count,

    // OUTPUT
    proxy_worker_slice* workers_out,
    // proxy_worker** workers_out, size_t* workers_out_size,
    size_t output_capacity) {
  // the output buffer for the workers we (may) match
  size_t workers_out_count = 0;
  size_t i = 0;

  // clip workers out count to capacity
  size_t max_bindings_count = bindings_count;
  if (max_bindings_count > output_capacity) {
    max_bindings_count = output_capacity;

    ap_log_error(APLOG_MARK, APLOG_ERR, 0, ap_server_conf,
                 "Not enough capacity to build worker list. Expecting %lu "
                 "slots, got capacity for %lu slots.",
                 bindings_count, output_capacity);
  }

  // go throght the prioritised list

  for (i = 0; i < max_bindings_count; ++i) {
    binding_row* b = &bindings[i];
    size_t worker_idx = 0;

    ap_log_error(APLOG_MARK, APLOG_DEBUG, 0, ap_server_conf,
                 " ---- Route:  %s -> %s (pri: %d, #%lu, %d)", b->site_name,
                 b->worker_host, b->priority, b->row_id, b->is_fallback);

    // loop all workers to find any matching ones for the current
    // binding.
    for (; worker_idx < worker_count; ++worker_idx) {
      proxy_worker* worker = workers[worker_idx];

      // if the worker is ok, add it to the output list.
      // If multiple workers are on the same host, this will
      // add all of them (but keep the original ordering
      // from the desired priority list)
      if (worker_matches_binding(b, worker)) {
        workers_out->entries[workers_out_count] = worker;
        workers_out_count++;

        ap_log_error(APLOG_MARK, APLOG_DEBUG, 0, ap_server_conf,
                     "   Matched worker : %s -> %s (idx: %d)",
                     worker->s->hostname, worker->s->name, worker->s->index);
      }
    }
  }

  // update the size
  workers_out->count = workers_out_count;
}

void find_matching_workers(const char* site_name,
                           const binding_rows bindings_in,
                           proxy_worker** workers, size_t worker_count,
                           matched_workers_lists* output,
                           size_t output_dedicated_capacity,
                           size_t output_fallback_capacity) {
  // a large enough stack buffer for any matching rows
  binding_row buffer_dedicated[kBINDINGS_BUFFER_SIZE];
  size_t buffer_dedicated_size = 0;

  binding_row buffer_fallback[kBINDINGS_BUFFER_SIZE];
  size_t buffer_fallback_size = 0;

  // first find binding rows that are capable of handling the site
  {
    size_t i = 0, len = bindings_in.count;
    for (; i < len; ++i) {
      binding_row r = bindings_in.entries[i];

      // Check if its a fallback worker
      if (r.is_fallback) {
        buffer_fallback[buffer_fallback_size] = r;
        buffer_fallback_size++;
        // check if its a dedicated worker
      } else if (site_name != NULL && (strcmp(site_name, r.site_name) == 0)) {
        buffer_dedicated[buffer_dedicated_size] = r;
        buffer_dedicated_size++;
      }
    }
  }

  // prioritise the list by prefering dedicated over fallback and
  // earlier ones over later ones
  qsort(buffer_dedicated, buffer_dedicated_size, sizeof(buffer_dedicated[0]),
        compare_bindings_row);
  qsort(buffer_fallback, buffer_fallback_size, sizeof(buffer_fallback[0]),
        compare_bindings_row);

  build_worker_list(workers, worker_count, buffer_dedicated,
                    buffer_dedicated_size, &output->dedicated,
                    output_dedicated_capacity);
  build_worker_list(workers, worker_count, buffer_fallback,
                    buffer_fallback_size, &output->fallback,
                    output_fallback_capacity);
}
