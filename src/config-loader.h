//
// Created by Miles Gibson on 03/08/16.
//

#ifndef HTTPD_CONFIG_LOADER_H
#define HTTPD_CONFIG_LOADER_H

#include "palette-director-types.h"

/*
        Helper to read the configuration from a file.

        Returns a bindings_setup struct. Only the successfully loaded binding
   configs
        are added to the return config.

        If no fallback host is declared, use the first declared host as
   fallback.
*/
binding_rows parse_csv_config(const char* path);

/*
        Returns two lists of workers:
        - one with prefered instances
        - one with allowed (fallback) instances

        The slices in the return struct must be freed after use.
*/
matched_workers_lists find_matching_workers(
    const char* site_name, const binding_rows bindings_in,
    const proxy_worker_slice workers_in);

#endif  // HTTPD_CONFIG_LOADER_H
