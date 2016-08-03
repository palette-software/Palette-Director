/* Licensed to the Apache Software Foundation (ASF) under one or more
* contributor license agreements.  See the NOTICE file distributed with
* this work for additional information regarding copyright ownership.
* The ASF licenses this file to You under the Apache License, Version 2.0
* (the "License"); you may not use this file except in compliance with
* the License.  You may obtain a copy of the License at
*
*     http://www.apache.org/licenses/LICENSE-2.0
*
* Unless required by applicable law or agreed to in writing, software
* distributed under the License is distributed on an "AS IS" BASIS,
* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
* See the License for the specific language governing permissions and
* limitations under the License.
*/

#include "mod_proxy.h"
#include "scoreboard.h"
#include "ap_mpm.h"
#include "apr_version.h"
#include "ap_hooks.h"

#include "stdio.h"

// FWD
// ===
static int status_page_http_handler(request_rec* r);


// MODULE DEFINITIONS
// ==================

module AP_MODULE_DECLARE_DATA lbmethod_bybusyness_module;


static const char* PALETTE_DIRECTOR_MODULE_NAME = "lbmethod_bybusyness_module";

// The handler name we'll use to display the status pages
static const char* PALETTE_DIRECTOR_STATUS_HANDLER = "palette-director-status";


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



// The configuration
static bindings_setup site_bindings_setup = { NULL, 0 };

/*
	The config to use when we have no config
*/
static const bindings_setup empty_bindings_setup = { NULL, 0 };

/*
	The HTTP path where the worker bindings status will be accessible.
*/
static const char* status_page_path = "/worker-bindings/status";




/////////////////////////////////////////////////////////////////////////////

// Worker matching
// ===============




/*
Returns 1 if the route config we passed matches the site.
Returns 0 if the route does not match
*/
static int try_to_match_site_name_for_worker_host( const char* site_name, proxy_worker* worker, const bindings_setup setup) {

	// convinience
	const config_path* route_configs = setup.bindings;
	const size_t configs_len = setup.binding_count;
	const char* worker_hostname = worker->s->hostname;

	size_t site_idx;
	for (site_idx = 0; site_idx < configs_len; ++site_idx) {
		/*int i;*/

		// The config for the current site
		const config_path* cfg = &route_configs[site_idx];

		// If the host for the worker does not match, no need to check the paths
		if (strcmp(worker_hostname, cfg->target_worker_host) != 0) {
			continue;
		}

		// If the site name matches with the desired site name, we are ok
		if (strcmp(cfg->site_name, site_name) == 0) {
			return 1;
		}

	}
	return 0;
}




/////////////////////////////////////////////////////////////////////////////


/*
	Helper to read the configuration from a file.

	Returns a bindings_setup struct. Only the successfully loaded binding configs
	are added to the return config.

	If no fallback host is declared, use the first declared host as fallback.
*/
static bindings_setup read_site_config_from(const char* path) {

	FILE *fp;
	int line_count = 1;
	// Set the default fallback to null
	const char* fallback_worker_host  = NULL;
	// Create a fixed size buffer for loading the entries into
	config_path buffer[512];
	// The number of records filled in buffer
	int loaded_site_count = 0;


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


// Returns the site name for the request (if there is one), or NULL if no site available in the request
static const char* get_site_name(const request_rec* r, const bindings_setup* setup) {

	char* val = NULL;
	char* key = NULL;
	char* site_name = NULL;

	
	size_t i;
	char* data = r->args;

	while(*data && (val = ap_getword(r->pool, &data, '&'))) {
		key = ap_getword(r->pool, &val, '=');
		ap_unescape_url((char *)key);
		ap_unescape_url((char *)val);

		// If the key is ':site', then we have a site
		if ( strcmp(key, ":site") == 0) {
			site_name = val;
			break;
		}

	}

	// Check if we can actually match this site


	if (site_name != NULL) {
		for (i = 0; i < setup->binding_count; ++i) {
			// if we have a handler, return the site name
			if (strcmp(setup->bindings[i].site_name, site_name) == 0) {
				return site_name;
			}
		}
	}

	// if no handler or even no name, return NULL
	return NULL;
}


/////////////////////////////////////////////////////////////////////////////

// Load Balancer code
// ==================

static int(*ap_proxy_retry_worker_fn)(const char *proxy_function,
	proxy_worker *worker, server_rec *s) = NULL;

static proxy_worker *find_best_bybusyness(proxy_balancer *balancer,
	request_rec *r)
{

	int i;
	proxy_worker **worker;
	proxy_worker *mycandidate = NULL;
	int cur_lbset = 0;
	int max_lbset = 0;
	int checking_standby;
	int checked_standby;

	int total_factor = 0;
	const char* site_name = NULL;

	//int needs_fallback = 0;

	if (!ap_proxy_retry_worker_fn) {
		ap_proxy_retry_worker_fn =
			APR_RETRIEVE_OPTIONAL_FN(ap_proxy_retry_worker);
		if (!ap_proxy_retry_worker_fn) {
			/* can only happen if mod_proxy isn't loaded */
			return NULL;
		}
	}

	ap_log_error(APLOG_MARK, APLOG_DEBUG, 0, r->server, APLOGNO(01211)
		"proxy: Entering bybusyness for BALANCER (%s)",
		balancer->s->name);

	// check if we need to use the fallback handler
	//needs_fallback = needs_fallback_host(r->uri, &site_bindings_setup);
	
	// get the site name
	site_name = get_site_name(r, &site_bindings_setup);
	ap_log_error(APLOG_MARK, APLOG_DEBUG, 0, r->server, "Got site name  '%s' for uri '%s' and args '%s'", site_name, r->unparsed_uri, r->args);

	/* First try to see if we have available candidate */
	do {

		checking_standby = checked_standby = 0;
		while (!mycandidate && !checked_standby) {

			worker = (proxy_worker **)balancer->workers->elts;
			for (i = 0; i < balancer->workers->nelts; i++, worker++) {

				// PALETTE DIRECTOR ADDITIONS
				// ==========================

				// Check if we have a site name in the request
				if (site_name == NULL) {
					const char* worker_host = (*worker)->s->hostname;
					// if we need the fallback, simply check if we have the correct host
					if (strcmp(worker_host, site_bindings_setup.fallback_worker_host) != 0) {
						continue;
					}
					// Log that we are using a fallback worker.
					ap_log_error(APLOG_MARK, APLOG_NOTICE, 0, r->server, "Using fallback worker '%s' for uri '%s' and args '%s'", worker_host, r->unparsed_uri, r->args);
				}
				else {
					// if we do not need to use a site-bound worker, check if the current one is the one we are
					// looking for
					if (try_to_match_site_name_for_worker_host(site_name, *worker, site_bindings_setup) != 1) {
						continue;
					}
					// log that we have matched the worker
					ap_log_error(APLOG_MARK, APLOG_NOTICE, 0, r->server, "Using site worker '%s' for uri '%s' and args '%s'", (*worker)->s->hostname, r->unparsed_uri, r->args);
				}


				// ORIGINAL LB_BYBUSYNESS METHOD
				// =============================

				if (!checking_standby) {    /* first time through */
					if ((*worker)->s->lbset > max_lbset)
						max_lbset = (*worker)->s->lbset;
				}
				if (
					((*worker)->s->lbset != cur_lbset) ||
					(checking_standby ? !PROXY_WORKER_IS_STANDBY(*worker) : PROXY_WORKER_IS_STANDBY(*worker)) ||
					(PROXY_WORKER_IS_DRAINING(*worker))
					) {
					continue;
				}

				/* If the worker is in error state run
				* retry on that worker. It will be marked as
				* operational if the retry timeout is elapsed.
				* The worker might still be unusable, but we try
				* anyway.
				*/
				if (!PROXY_WORKER_IS_USABLE(*worker)) {
					ap_proxy_retry_worker_fn("BALANCER", *worker, r->server);
				}

				/* Take into calculation only the workers that are
				* not in error state or not disabled.
				*/
				if (PROXY_WORKER_IS_USABLE(*worker)) {

					(*worker)->s->lbstatus += (*worker)->s->lbfactor;
					total_factor += (*worker)->s->lbfactor;

					if (!mycandidate
						|| (*worker)->s->busy < mycandidate->s->busy
						|| ((*worker)->s->busy == mycandidate->s->busy && (*worker)->s->lbstatus > mycandidate->s->lbstatus))
						mycandidate = *worker;

				}

			}

			checked_standby = checking_standby++;

		}

		cur_lbset++;

	} while (cur_lbset <= max_lbset && !mycandidate);

	if (mycandidate) {
		mycandidate->s->lbstatus -= total_factor;
		ap_log_error(APLOG_MARK, APLOG_DEBUG, 0, r->server, APLOGNO(01212)
			"proxy: bybusyness selected worker \"%s\" : busy %" APR_SIZE_T_FMT " : lbstatus %d",
			mycandidate->s->name, mycandidate->s->busy, mycandidate->s->lbstatus);

	}

	return mycandidate;

}

/* assumed to be mutex protected by caller */
static apr_status_t reset(proxy_balancer *balancer, server_rec *s) {
	int i;
	proxy_worker **worker;
	worker = (proxy_worker **)balancer->workers->elts;
	for (i = 0; i < balancer->workers->nelts; i++, worker++) {
		(*worker)->s->lbstatus = 0;
		(*worker)->s->busy = 0;
	}
	return APR_SUCCESS;
}

static apr_status_t age(proxy_balancer *balancer, server_rec *s) {
	return APR_SUCCESS;
}

// Declare the lbmethod

static const proxy_balancer_method bybusyness =
{
	"bybusyness",
	&find_best_bybusyness,
	NULL,
	&reset,
	&age
};


// MAIN ENTRY POINT FOR INIT
// =========================

static void register_hook(apr_pool_t *p)
{
	// Register the LBMethod
	ap_register_provider(p, PROXY_LBMETHOD, "bybusyness", "0", &bybusyness);
	// Register the status page hook
	ap_hook_handler(status_page_http_handler, NULL, NULL, APR_HOOK_FIRST );
}


// STATUS PAGE HANDLER
// ===================

// Prints a single cell in a status table
static void status_table_cell(request_rec* r, const int isChecked) {
	// print the cell preamble
	ap_rprintf(r, "<td class='tb-data-grid-separator-row ng-scope'><div class='tb-lr-padded-wide tb-worker-statuses-container'><span class='ng-scope ng-isolate-scope'><span class='tb-data-grid-icon'>");

	// Add the active marker if there is a marker
	if (isChecked) {
		ap_rprintf(r, "<span title='Active' class='tb-icon-process-status tb-icon-process-status-active'></span>");
	} 

	// close the cell
	ap_rprintf(r, "</span></span></div></td>");	
}

/*
	Builds an HTML status page.

	If add_style is not 0, then add the styling css and other style data.

*/
static void status_page_html(request_rec* r, const bindings_setup* b, const int add_style) {
	const size_t binding_count = b->binding_count;

	ap_set_content_type(r, "text/html");

	ap_rprintf(r, "<html>");

	// head
	ap_rprintf(r, "<head>");

	// add style if needed
	if (add_style) {
		ap_rprintf(r, "<title>Palette Director Workerbinding Status</title>");
		ap_rprintf(r, "<link rel='stylesheet' type='text/css' href='/vizportal.css' />");
	}

	// body
	ap_rprintf(r, "</head><body>");

	// table
	ap_rprintf(r, "<table class='tb-static-grid-table tb-static-grid-table-settings-min-width'>");

	// HOSTS TABLE HEADER
	// ------------------

	ap_rprintf(r, "<thead>");
	// list the sites as header
	{
		size_t i;
		ap_rprintf(r, "<tr><th></th>");
		for(i=0; i < binding_count; ++i) {
			const config_path p = b->bindings[i];
			ap_rprintf(r, "<th class='tb-data-grid-headers-line tb-data-grid-headers-line-multiline'><div class='tb-data-grid-header-text'>%s</div></th>", p.target_worker_host);
		}

		// add the fallback host
		ap_rprintf(r, "<th class='tb-data-grid-headers-line tb-data-grid-headers-line-multiline'><div class='tb-data-grid-header-text'>%s</div></th>", b->fallback_worker_host);
		ap_rprintf(r, "</tr>");
	}
	ap_rprintf(r, "</thead>");
	ap_rprintf(r, "<tbody>");

	
	// HOSTS TABLE BODY
	// ----------------
	{
		size_t row, col;
		
		// output normal hosts
		for (row=0; row < binding_count; ++row) {
			ap_rprintf(r, "<tr>");
			ap_rprintf(r, "<td class='tb-data-grid-separator-row'><span class='tb-data-grid-cell-text tb-lr-padded-wide ng-scope'>%s</span></td>", b->bindings[row].site_name);

			for (col=0; col < binding_count; ++col)	status_table_cell(r, row == col );

			// put the fallback cell as false
			status_table_cell(r, 0);
			ap_rprintf(r, "</tr>");
		}
		

		// output Fallback host
		ap_rprintf(r, "<tr>");
		ap_rprintf(r, "<td class='tb-data-grid-separator-row'><span class='tb-data-grid-cell-text tb-lr-padded-wide ng-scope'>Everything else</span></td>");


		for (col=0; col < binding_count; ++col) {
			status_table_cell(r, 0 );
		}
		status_table_cell(r, 1 );
		ap_rprintf(r, "</tr>");

	}

	ap_rprintf(r, "</tbody>");
	ap_rprintf(r, "</table>");
	ap_rprintf(r, "</body>");
	ap_rprintf(r, "</html>");
}






/* Builds an JSON status page*/
static void status_page_json(request_rec* r, const bindings_setup* b) {
	ap_set_content_type(r, "application/json");
	ap_rprintf(r, "{\"fallback\": \"%s\", \"sites\":{", b->fallback_worker_host);
	{
		size_t i;
		for(i=0; i < b->binding_count; ++i) {
			const config_path p = b->bindings[i];
			// Add a comma before all but the first
			if (i != 0)	{ ap_rprintf(r, ", "); }
			// and print the pair
			ap_rprintf(r, "\"%s\": \"%s\"", p.site_name, p.target_worker_host);
		}
	}
	ap_rprintf(r, "}}");
}



// convinience function to match part of a url and map the result to TRUE/FALSE
static int uri_matches(const request_rec* r, const char* pattern) {
	return (ap_strcmp_match(r->uri, pattern) == 0) ? TRUE : FALSE;
}


/*
	Handler that returns the status page html.
*/
static int status_page_http_handler(request_rec* r) {

	// check if we are the handler needed
	if (!r->handler || strcmp(r->handler, PALETTE_DIRECTOR_STATUS_HANDLER)) { return (DECLINED); }

	// if we handle it
	{
		// We need style if our url arg is simply 'with-style'
		const int requires_style = (r->args != NULL) && (strcmp(r->args, "with-style") == 0);

		// Check for content-types.
		// Start with HTML with optional style
		if (uri_matches(r, "*html")) status_page_html(r, &site_bindings_setup, requires_style);
		// JSON
		else if (uri_matches(r, "*json")) status_page_json(r, &site_bindings_setup, requires_style);
		// The fallback is HTML without style for now
		else status_page_html(r, &site_bindings_setup, TRUE);
				
		return OK;
	}
}

// APACHE CONFIG FILE
// ==================


/* Handler for the "WorkerBindingConfigPath" directive */
static const char *workerbinding_set_config_path(cmd_parms *cmd, void *cfg, const char *arg)
{
	// Check if we have a loaded config already.
	if (site_bindings_setup.binding_count == 0) {
		site_bindings_setup = read_site_config_from(arg);
		ap_log_error(APLOG_MARK, APLOG_NOTICE, 0, ap_server_conf, "Loaded %d worker bindings from '%s'", site_bindings_setup.binding_count, arg);
	} else {
		// if yes, log the fact that we tried to add to the config
		ap_log_error(APLOG_MARK, APLOG_ERR, 0, ap_server_conf, "Duplicate worker bindings config files: config already loaded at the  WorkerBindingConfigPath '%s'  directive", arg);
	}
	return NULL;
}


// Apache config directives.
static const command_rec workerbinding_directives[] =
{
	AP_INIT_TAKE1("WorkerBindingConfigPath", workerbinding_set_config_path, NULL, RSRC_CONF, "The path to the workerbinding config"),
	{ NULL }
};

// MODULE DECLARATION
// ==================

AP_DECLARE_MODULE(lbmethod_bybusyness) = {
	STANDARD20_MODULE_STUFF,
	NULL,       /* create per-directory config structure */
	NULL,       /* merge per-directory config structures */
	NULL,       /* create per-server config structure */
	NULL,       /* merge per-server config structures */
	workerbinding_directives,       /* Any directives we may have for httpd */
	register_hook /* register hooks */
};
