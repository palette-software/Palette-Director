//
// Created by Miles Gibson on 03/08/16.
//

#include <stdio.h>
#include "mod_proxy.h"
#include "config-loader.h"
#include "palette-director-types.h"
/////////////////////////////////////////////////////////////////////////////

/*
	The config to use when we have no config
*/
static const bindings_setup empty_bindings_setup = { NULL, 0 };


/*
	Helper to read the configuration from a file.

	Returns a bindings_setup struct. Only the successfully loaded binding configs
	are added to the return config.

	If no fallback host is declared, use the first declared host as fallback.
*/
bindings_setup read_site_config_from(const char* path) {

    FILE *fp;
    int line_count = 1;
    // Set the default fallback to null
    const char* fallback_worker_host  = NULL;
    // Create a fixed size buffer for loading the entries into
    config_path buffer[512];
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
        char siteName[256], hostName[256];
        // and store it in the buffer
        config_path p;

        int ret = fscanf(fp, "%s -> %s", siteName, hostName);
        if (ret == 2) {
            ap_log_error(APLOG_MARK, APLOG_NOTICE, 0, ap_server_conf, "Loaded worker binding for site: '%s' bound to '%s'", siteName, hostName);
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
            ap_log_error(APLOG_MARK, APLOG_ERR, 0, ap_server_conf, "Scanf error: '%s' in line: %d of '%s'", strerror(errno), line_count, path);
            break;
        }
        else if (ret == EOF) {
            break;
        }
        else {
            ap_log_error(APLOG_MARK, APLOG_ERR, 0, ap_server_conf, "No match found in line: %d of '%s'", line_count, path);
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
        bindings_setup o = { (config_path*)malloc(loaded_sites_size), fallback_worker_host, loaded_site_count };
        memcpy(o.bindings, buffer, loaded_sites_size);
        return o;
    }
}

