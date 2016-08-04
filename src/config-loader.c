//
// Created by Miles Gibson on 03/08/16.
//

#include <stdio.h>
#include <mod_proxy.h>
#include "mod_proxy.h"
#include "config-loader.h"

/////////////////////////////////////////////////////////////////////////////

/*
	The config to use when we have no config
*/
static const bindings_setup empty_bindings_setup = {NULL,
                                                    0};


/*
	Helper to read the configuration from a file.

	Returns a bindings_setup struct. Only the successfully loaded binding configs
	are added to the return config.

	If no fallback host is declared, use the first declared host as fallback.
*/
bindings_setup read_site_config_from(const char *path) {

    FILE *fp;
    int line_count = 1;
    // Set the default fallback to null
    const char *fallback_worker_host = NULL;
    // Create a fixed size buffer for loading the entries into
    config_path buffer[kBINDINGS_BUFFER_SIZE];
    // The number of records filled in buffer
    size_t loaded_site_count = 0;


    // Try to open the config file
    if ((fp = fopen(path, "r")) == NULL) {
        ap_log_error(APLOG_MARK, APLOG_ERR, 0, ap_server_conf, "Cannot open worker bindings config file '%s'", path);
        return empty_bindings_setup;
    }

    // Read all lines from the config file
    while (1) {
        // Buffers for storing line data
        char siteName[kCONFIG_MAX_STRING_SIZE], hostName[kCONFIG_MAX_STRING_SIZE];
        // and store it in the buffer
        config_path p;

        int ret = fscanf(fp, "%s -> %[^\n]", siteName, hostName);
        if (ret == 2) {
            ap_log_error(APLOG_MARK, APLOG_NOTICE, 0, ap_server_conf,
                         "Loaded worker binding for site: '%s' bound to '%s'", siteName, hostName);
            // store the site config

            // check if its a fallback route
            if (strcmp(siteName, "*") == 0) {
                // store the fallback host path
                fallback_worker_host = strdup(hostName);
            }
                // if not, add it to the routes
            else {
                // we have to make sure we have a fallback host
                if (fallback_worker_host == NULL) {
                    fallback_worker_host = strdup(hostName);
                }

                // and store it in the buffer
                p.site_name = strdup(siteName);
                p.target_worker_host = strdup(hostName);

                buffer[loaded_site_count] = p;
                loaded_site_count++;
            }

            // incerement the loaded count
        }
        else if (errno != 0) {
            ap_log_error(APLOG_MARK, APLOG_ERR, 0, ap_server_conf, "Scanf error: '%s' in line: %d of '%s'",
                         strerror(errno), line_count, path);
            break;
        }
        else if (ret == EOF) {
            break;
        }
        else {
            ap_log_error(APLOG_MARK, APLOG_ERR, 0, ap_server_conf, "No match found in line: %d of '%s'", line_count,
                         path);
        }
        // next line
        line_count++;
    }

    // Close the file after use
    fclose(fp);


    // copy the loaded data to a freshly allocated memory block
    {
        size_t loaded_sites_size = sizeof(config_path) * loaded_site_count;
        // create the output struct
        bindings_setup o = {(config_path *) malloc(loaded_sites_size), fallback_worker_host, loaded_site_count};
        memcpy(o.bindings, buffer, loaded_sites_size);
        return o;
    }
}


static int compare_bindings_row(const void *s1, const void *s2) {
    binding_row *e1 = (binding_row *) s1;
    binding_row *e2 = (binding_row *) s2;

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
static int worker_matches_binding(const binding_row *b, const proxy_worker *w) {
    return (strcmp(b->worker_host, w->s->hostname) == 0) ? TRUE : FALSE;
}


size_t find_matching_workers(const char *site_name, const binding_rows bindings_in, proxy_worker **workers,
                             size_t worker_count, proxy_worker **output, size_t output_capacity) {

    // a large enough stack buffer for any matching rows
    binding_row buffer[kBINDINGS_BUFFER_SIZE];
    size_t buffer_size = 0;

    // first find binding rows that are capable of handling the site
    {
        size_t i = 0, len = bindings_in.count;
        for (; i < len; ++i) {
            binding_row r = bindings_in.entries[i];

            // check the site name. Fallbacks are alright for now,
            // will prioritise later
            if (r.is_fallback || (site_name != NULL && (strcmp(site_name, r.site_name) == 0))) {
                buffer[buffer_size] = r;
                buffer_size++;
            }
        }
    }

    // prioritise the list by prefering dedicated over fallback and
    // earlier ones over later ones
    qsort(buffer, buffer_size, sizeof(buffer[0]), compare_bindings_row);

    {
        // the output buffer for the workers we (may) match
        proxy_worker *worker_buffer[kWORKERS_BUFFER_SIZE];
        size_t worker_buffer_size = 0;

        // go throght the prioritised list
        size_t i = 0;
        for (i = 0; i < buffer_size; ++i) {
            binding_row *b = &buffer[i];
            size_t worker_idx = 0;

            ap_log_error(APLOG_MARK, APLOG_DEBUG, 0, ap_server_conf,
                         " ---- Route:  %s -> %s (pri: %d, #%lu, %d)", b->site_name, b->worker_host, b->priority,
                         b->row_id, b->is_fallback);

            // loop all workers to find any matching ones for the current
            // binding.
            for (; worker_idx < worker_count; ++worker_idx) {
                proxy_worker *worker = workers[worker_idx];

                // if the worker is ok, add it to the output list.
                // If multiple workers are on the same host, this will
                // add all of them (but keep the original ordering
                // from the desired priority list)
                if (worker_matches_binding(b, worker)) {
                    worker_buffer[worker_buffer_size] = worker;
                    worker_buffer_size++;

                    ap_log_error(APLOG_MARK, APLOG_DEBUG, 0, ap_server_conf,
                                 "   Matched worker : %s -> %s (idx: %d)", worker->s->hostname, worker->s->name,
                                 worker->s->index);
                }
            }
        }


        // Now that we know how large the output should be, copy it there
        if (worker_buffer_size >= output_capacity) {

            ap_log_error(APLOG_MARK, APLOG_ERR, 0, ap_server_conf,
                         "Not enough space for complete matching worker list. Needed: %lu entries, only have room for %lu",
                         worker_buffer_size, output_capacity);

            // cap the workers count to the size of the output array
            // TODO: handle this case somehow
            worker_buffer_size = output_capacity;
        }

        // Copy the pointers to the workers we matched
        memcpy(output, worker_buffer, sizeof(worker_buffer[0]) * worker_buffer_size);

        // Return the number of workers copied
        return worker_buffer_size;
    }


}
