//
// Created by Miles Gibson on 03/08/16.
//

#ifndef HTTPD_CONFIG_LOADER_H
#define HTTPD_CONFIG_LOADER_H

#include "palette-director-types.h"

/*
	Helper to read the configuration from a file.

	Returns a bindings_setup struct. Only the successfully loaded binding configs
	are added to the return config.

	If no fallback host is declared, use the first declared host as fallback.
*/
binding_rows parse_csv_config(const char* path);



void find_matching_workers( const char* site_name, const binding_rows bindings_in, proxy_worker** workers, size_t worker_count, matched_workers_lists* output, size_t output_dedicated_capacity, size_t output_fallback_capacity );


#endif //HTTPD_CONFIG_LOADER_H
