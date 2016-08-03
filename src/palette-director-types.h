#ifndef HTTPD_PALETTE_DIRECTOR_TYPES_H
#define HTTPD_PALETTE_DIRECTOR_TYPES_H

#include <stddef.h>

// Types shared by palette director

/*
Config to define matching a http path to a worker route.
*/
typedef struct config_path {
    /* We try to match this path on the request URI */
    const char* site_name;

    /* If the match suceeded, set the route to this */
    const char* target_worker_host;
} config_path;




/*
	A list of bindings with a count.
*/
typedef struct bindings_setup {
    config_path* bindings;
    const char* fallback_worker_host;
    size_t binding_count;
} bindings_setup;



// The handler name we'll use to display the status pages
static const char* PALETTE_DIRECTOR_STATUS_HANDLER = "palette-director-status";

#endif //HTTPD_PALETTE_DIRECTOR_TYPES_H
