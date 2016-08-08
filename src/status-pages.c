#include "status-pages.h"

#include "mod_proxy.h"

// STATUS PAGE HANDLER
// ===================

enum { kNO_MAPPING_AS_PRIORITY = -123456 };

// Prints a single cell in a status table
static void status_table_cell(request_rec* r, const int prio) {
  // print the cell preamble
  ap_rprintf(r,
             "<td class='tb-data-grid-separator-row ng-scope'><div "
             "class='tb-lr-padded-wide tb-worker-statuses-container'><span "
             "class='ng-scope ng-isolate-scope'><span "
             "class='tb-data-grid-icon'>");

  // Add the active marker if there is a marker
  if (prio != kNO_MAPPING_AS_PRIORITY) {
    ap_rprintf(r,
               "<span title='Active' class='tb-icon-process-status "
               "tb-icon-process-status-active'><small style='margin-left: "
               "25px;'><em>priority: </em><b>%d</b></small></span>",
               prio);
  }

  // close the cell
  ap_rprintf(r, "</span></span></div></td>");
}

static void add_style_header(request_rec* r) {
  ap_rprintf(r, "<html>");

  // head
  ap_rprintf(r, "<head>");

  ap_rprintf(r, "<title>Palette Director Workerbinding Status</title>");
  ap_rprintf(r,
             "<link rel='stylesheet' type='text/css' href='/vizportal.css' />");

  // body
  ap_rprintf(r, "</head><body class='tb-app'>");
}

static void add_style_footer(request_rec* r) {
  ap_rprintf(r, "</body>");
  ap_rprintf(r, "</html>");
}

// Sorting helper that forwards to strcmp
static int string_qsort_compare(const void* p1, const void* p2) {
  return strcmp(*(char* const*)p1, *(char* const*)p2);
}

typedef const char* (*key_extractor_fn)(const void* o);

static const char* extract_host_fn(const void* o) {
  return ((const binding_row*)o)->worker_host;
}

static const char* extract_site_name_fn(const void* o) {
  return ((const binding_row*)o)->site_name;
}

static void find_all_hosts(const binding_rows* b, key_extractor_fn extractor_fn,
                           const char** output, size_t* output_count) {
  size_t i;

  const char* buffer[kCONFIG_MAX_STRING_SIZE];

  for (i = 0; i < b->count; ++i) {
    buffer[i] = extractor_fn(&b->entries[i]);
  }

  // sort the list of strings so we can unique them
  qsort(buffer, b->count, sizeof(buffer[0]), string_qsort_compare);

  {
    const char* current_str = NULL;
    size_t output_idx = 0;

    for (i = 0; i < b->count; ++i) {
      const char* s = buffer[i];
      // if the string differs, add it to the array
      if (current_str == NULL || strcmp(current_str, s) != 0) {
        output[output_idx] = s;
        current_str = s;
        output_idx++;
      }
    }

    *output_count = output_idx;
  }
}
//
//// Return 1 if there is a mapping for this site-host combo
// static int mapping_priority_for(const binding_rows* b, const char* site_name,
//                                const char* worker_host) {
//  size_t i, len = b->count;
//  for (i = 0; i < len; ++i) {
//    const binding_row br = b->entries[i];
//    if (strcmp(br.site_name, site_name) == 0 &&
//        strcmp(br.worker_host, worker_host) == 0) {
//      return br.priority;
//    }
//  }
//  return kNO_MAPPING_AS_PRIORITY;
//}

/*
        Builds an HTML status page.

        If add_style is not 0, then add the styling css and other style data.

*/
void status_page_html(request_rec* r, const binding_rows* b,
                      const int add_style) {
  const char* host_buf[256];
  size_t host_buf_size = 0;

  const char* sites_buf[256];
  size_t sites_buf_size = 0;

  const size_t binding_count = b->count;

  // find all host names
  find_all_hosts(b, extract_site_name_fn, sites_buf, &sites_buf_size);
  find_all_hosts(b, extract_host_fn, host_buf, &host_buf_size);

  ap_set_content_type(r, "text/html");

  // add style and html wrap if needed
  if (add_style) {
    add_style_header(r);
  }

  ap_rprintf(r, "<div><h3>VizQL worker bindings</h3></div>");
  // table
  ap_rprintf(r,
             "<table class='tb-static-grid-table "
             "tb-static-grid-table-settings-min-width'>");

  // HOSTS TABLE HEADER
  // ------------------

  ap_rprintf(r, "<thead>");
  // list the sites as header
  {
    size_t i;

    ap_rprintf(r, "<tr><th></th>");
    for (i = 0; i < host_buf_size; ++i) {
      // const binding_row p = b->entries[i];
      ap_rprintf(r,
                 "<th class='tb-data-grid-headers-line "
                 "tb-data-grid-headers-line-multiline'><div "
                 "class='tb-data-grid-header-text'>%s</div></th>",
                 host_buf[i]);
    }
    ap_rprintf(r, "</tr>");
  }
  ap_rprintf(r, "</thead>");
  ap_rprintf(r, "<tbody>");

  // HOSTS TABLE BODY
  // ----------------
  {
    size_t row, col;

    // output normal hosts
    for (row = 0; row < sites_buf_size; ++row) {
      ap_rprintf(r, "<tr>");
      ap_rprintf(r,
                 "<td class='tb-data-grid-separator-row'><span "
                 "class='tb-data-grid-cell-text tb-lr-padded-wide "
                 "ng-scope'>%s</span></td>",
                 sites_buf[row]);

      // for (col = 0; col < host_buf_size; ++col) {
      //  status_table_cell(
      //      r, mapping_priority_for(b, sites_buf[row], host_buf[col]));
      //}

      ap_rprintf(r, "</tr>");
    }
  }

  ap_rprintf(r, "</tbody>");
  ap_rprintf(r, "</table>");

  // close style and html wrap if needed
  if (add_style) {
    add_style_footer(r);
  }
}

/* Builds an JSON status page*/
void status_page_json(request_rec* r, const binding_rows* b) {
  return;

  // ap_set_content_type(r, "application/json");
  // ap_rprintf(r, "{\"fallback\": \"%s\", \"sites\":{",
  // b->fallback_worker_host);
  //{
  //    size_t i;
  //    for(i=0; i < b->binding_count; ++i) {
  //        const config_path p = b->bindings[i];
  //        // Add a comma before all but the first
  //        if (i != 0)	{ ap_rprintf(r, ", "); }
  //        // and print the pair
  //        ap_rprintf(r, "\"%s\": \"%s\"", p.site_name, p.target_worker_host);
  //    }
  //}
  // ap_rprintf(r, "}}");
}
