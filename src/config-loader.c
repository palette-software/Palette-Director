//
// Created by Miles Gibson on 03/08/16.
//

#include <stdio.h>
#include <stdlib.h>

#include <mod_proxy.h>
#include <sys/stat.h>
#include "config-loader.h"
#include "csv/csv.h"

/////////////////////////////////////////////////////////////////////////////

enum { RESOLVE_HOSTNAMES = FALSE };

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
#ifdef WINDOWS
const char* strndup(const char* str, size_t num_chars) {
  // clip the length
  const size_t copied_chars = min(num_chars, strlen(str));
  char* o = (char*)malloc(copied_chars + 1);

  strncpy(o, str, copied_chars);

  // null terminate it
  o[copied_chars] = '\0';

  return o;
}
#endif

/////////////////////////////////////////////////////////////////

// the indices of the read fields
enum {
  kiDX_SITE_NAME = 0,
  kIDX_WORKER_HOST_NAME = 1,
  kIDX_BINDING_KIND = 2,

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

  // The line number so we can skip the first one
  size_t line_count;

} config_loader_state;

static int put_comma = 0;

// Handler on fields
void on_csv_cell(void* s, size_t i, void* p) {
  // get the state
  config_loader_state* state = (config_loader_state*)p;
  int state_idx = state->state;
  // clone the string, beacause all steps require a null-terminated string
  char* tmp = (char*)strndup((char*)s, i);

  // Skip the first line and dont even try to process it
  if (state->line_count == 0) {
    return;
  }

  // check which field we are at and handle it
  switch (state_idx) {
    case kiDX_SITE_NAME:
      // if note, dupe the site name
      state->current_row.site_name = tmp;
      break;

    case kIDX_WORKER_HOST_NAME:
      state->current_row.worker_host = tmp;
      break;

    case kIDX_BINDING_KIND: {
      // The kind of binding.
      int binding_kind = kBINDING_ALLOW;
      //  check the kind
      if (strcmp("prefer", tmp) == 0) {
        binding_kind = kBINDING_PREFER;
      } else if (strcmp("forbid", tmp) == 0) {
        binding_kind = kBINDING_FORBID;
      }
      // update the binding from the register
      state->current_row.binding_kind = binding_kind;
    }

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

  // Add the row to the state if its not the first one
  if (state->line_count > 0) {
    state->rows[state->row_count] = state->current_row;
    state->row_count += 1;

    ap_log_error(APLOG_MARK, APLOG_ERR, 0, ap_server_conf,
                 "Loaded binding: site:'%s' to: '%s'",
                 state->current_row.site_name, state->current_row.worker_host);
  }

  // increment the line count so we wont skip the line next time
  state->line_count++;

  // reset the state
  state->state = kiDX_SITE_NAME;
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

  printf("cmp: ==== %d\n", strcmp("", ""));

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

    // dont resolve for now
    if (RESOLVE_HOSTNAMES) {
      size_t i = 0;
      ip_resolver_table rt = {0, 0, 0};
      for (i = 0; i < state.row_count; ++i) {
        const char* hostname = state.rows[i].worker_host;
        printf("============= %s -> %s\n", hostname,
               ip_resolver_lookup(&rt, hostname));
      }
    }

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

////////////////////////////////////////////////////////////////////////////

static binding_kind_t binding_kind_for(const binding_rows bindings,
                                       const char* site_name,
                                       const char* worker_host) {
  size_t i, len = bindings.count;
  // No site name? This site is allowed for this worker, thats for sure
  if (site_name == NULL) return kBINDING_ALLOW;

  for (i = 0; i < len; ++i) {
    binding_row b = bindings.entries[i];

    // if the site name and worker host match, return the kind
    if (strcasecmp(b.site_name, site_name) == 0 &&
        strcasecmp(b.worker_host, worker_host) == 0) {
      return b.binding_kind;
    }
  }
  // if no entries match, return an 'allow'
  return kBINDING_ALLOW;
}

typedef struct worker_hostname_filter_state {
  const char* site_name;
  binding_kind_t kind;

  binding_rows bindings_in;
} worker_hostname_filter_state;

static int worker_by_hostname_filter_fn(const proxy_worker** w, void* state) {
  worker_hostname_filter_state* s = (worker_hostname_filter_state*)state;
  binding_kind_t b =
      binding_kind_for(s->bindings_in, s->site_name, (*w)->s->hostname);
  return b == s->kind;
}

proxy_worker_slice get_handling_workers_for(const binding_rows bindings_in,
                                            const proxy_worker_slice workers_in,
                                            const char* site_name,
                                            const binding_kind_t with_kind) {
  worker_hostname_filter_state filter_state = {site_name, with_kind};
  // TODO: somehow fix this issue of const binding_rows -> binding_rows
  filter_state.bindings_in = bindings_in;
  return proxy_worker_slice_filter(workers_in, worker_by_hostname_filter_fn,
                                   kBINDINGS_BUFFER_SIZE, &filter_state);
}
