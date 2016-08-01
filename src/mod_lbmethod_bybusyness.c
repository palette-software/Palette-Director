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

module AP_MODULE_DECLARE_DATA lbmethod_bybusyness_module;
//module AP_MODULE_DECLARE_DATA palette_director_module;

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
/////////////////////////////////////////////////////////////////////////////

// Path pre & postfixes
// ====================

/* 
	Adds a pre and a postfix to the string while copying it to a newly alloced area.
	Each string must be free-d separately.
*/
const char* add_pre_and_postfixes(const char* str, const char* prefix, const char* postfix) {
	size_t alloc_len = strlen(prefix) + strlen(postfix) + strlen(str) + 1;
	// Alloc the output
	char* out_str = (char*)malloc(alloc_len);
	// Format the output and keep us to the max buffer size
	_snprintf(out_str, alloc_len, "%s%s%s", prefix, str, postfix);


	return out_str;
}

// Defines pre & postfixes to derive URI match patterns from a site name
typedef struct pre_and_postfix {
	const char* prefix;
	const char* postfix;
} pre_and_postfix;

// The pre & postfixes we'll use for generating the URI matchers
static const pre_and_postfix SITE_URI_MATCHER_PRE_AND_POSTFIXES[2] = {
	{"/t/", "*"},
	{"/vizql/t/", "*"},
};

// The number of matchers we'll have to manufacture for every site we
// want to match
static const int MATCHER_COUNT = sizeof(SITE_URI_MATCHER_PRE_AND_POSTFIXES) / sizeof(SITE_URI_MATCHER_PRE_AND_POSTFIXES[0]);


/////////////////////////////////////////////////////////////////////////////

// Worker matching
// ===============


/*
Returns 1 if the route config we passed matches the request_uri.
Returns 0 if the route does not match
*/
int try_to_match_uri_for_worker_host( const char* request_uri, proxy_worker* worker, const bindings_setup setup) {

	// convinience
	const config_path* route_configs = setup.bindings;
	const size_t configs_len = setup.binding_count;
	const char* worker_hostname = worker->s->hostname;

	size_t site_idx;
	for (site_idx = 0; site_idx < configs_len; ++site_idx) {
		int i;

		// The config for the current site
		const config_path* cfg = &route_configs[site_idx];

		// If the host for the worker does not match, no need to check the paths
		if (strcmp(worker_hostname, cfg->target_worker_host) != 0) {
			continue;
		}

		// for each matcher do this
		for (i = 0; i < MATCHER_COUNT; ++i) {

			// create the path expr
			pre_and_postfix p = SITE_URI_MATCHER_PRE_AND_POSTFIXES[i];
			const char* matcher_path = add_pre_and_postfixes(cfg->site_name, p.prefix, p.postfix);

			if (!ap_strcmp_match(request_uri, matcher_path)) {
				return 1;
			}

			// Dealloc the path expr (and avoid const errors)
			free((void*)matcher_path);
		}
	}
	return 0;
}




/*
Returns 1 if the route config we passed matches the request_uri.
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

		// for each matcher do this
		//for (i = 0; i < MATCHER_COUNT; ++i) {

			// If the site name matches with the desired site name, we are ok
			if (strcmp(cfg->site_name, site_name) == 0) {
				return 1;
			}

			//// create the path expr
			//pre_and_postfix p = SITE_URI_MATCHER_PRE_AND_POSTFIXES[i];
			//const char* matcher_path = add_pre_and_postfixes(cfg->site_name, p.prefix, p.postfix);

			//if (!ap_strcmp_match(request_uri, matcher_path)) {
			//	return 1;
			//}

			//// Dealloc the path expr (and avoid const errors)
			//free((void*)matcher_path);
		//}
	}
	return 0;
}




/////////////////////////////////////////////////////////////////////////////


// Samplle config
const config_path multi_config_path[] = {
	{ "Test", "localhost" },
	{ "Sample", "127.0.0.1" },
};


const bindings_setup empty_bindings_setup = { NULL, 0 };


bindings_setup read_site_config_from(const char* path) {

	//A server_rec pointer must be passed to ap_log_error() when called after startup.
	// This was always appropriate, but there are even more limitations with a NULL 
	// server_rec in 2.4 than in previous releases.Beginning with 2.3.12, the global
	// variable ap_server_conf can always be used as the server_rec parameter,
	// as it will be NULL only when it is valid to pass NULL to 
	// ap_log_error(). ap_server_conf should be used only when a more
	// appropriate server_rec is not available.
	FILE *fp;
	int line_count = 1;
	// Set the default fallback to null
	const char* fallback_worker_host  = NULL;

	// Create a fixed size buffer for loading the entries into
	config_path buffer[512];
	// THe number of records filled in buffer
	int loaded_site_count = 0;

	size_t loaded_sites_size = 0;

	bindings_setup o;

	if ((fp = fopen(path, "r")) == NULL) {
		ap_log_error(APLOG_MARK, APLOG_ERR, 0, ap_server_conf, "Cannot open worker bindings config file '%s'", path);
		return empty_bindings_setup;
	}


	while (1) {
		// Buffers for storing line data
		char siteName[256], hostName[256];
		// and store it in the buffer
		config_path p;

		int ret = fscanf(fp, "%s -> %s", siteName, hostName);
		if (ret == 2) {
			ap_log_error(APLOG_MARK, APLOG_ERR, 0, ap_server_conf, "Loaded worker binding for site: '%s' bound to '%s'", siteName, hostName);
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

		line_count++;
	}

	// copy the loaded data to a freshly allocated memory block
	loaded_sites_size = sizeof(config_path) * loaded_site_count;

	o.bindings = malloc(loaded_sites_size);
	o.fallback_worker_host = fallback_worker_host;
	o.binding_count = loaded_site_count;
	//bindings_setup o = { malloc(loaded_sites_size), fallback_worker_host, loaded_site_count };
	memcpy(o.bindings, buffer, loaded_sites_size);
	
	return o;
}


// Returns the site name for the request (if there is one), or NULL if no site available in the request
static const char* has_site(const request_rec* r, const bindings_setup* setup) {

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
	site_name = has_site(r, &site_bindings_setup);
	ap_log_error(APLOG_MARK, APLOG_ERR, 0, r->server, "Got site name  '%s' for uri '%s' and args '%s'", site_name, r->unparsed_uri, r->args);

	/* First try to see if we have available candidate */
	do {

		checking_standby = checked_standby = 0;
		while (!mycandidate && !checked_standby) {

			worker = (proxy_worker **)balancer->workers->elts;
			for (i = 0; i < balancer->workers->nelts; i++, worker++) {

				// Check if we have a site name in the request
				if (site_name == NULL) {
					const char* worker_host = (*worker)->s->hostname;
					// if we need the fallback, simply check if we have the correct host
					if (strcmp(worker_host, site_bindings_setup.fallback_worker_host) != 0) {
						continue;
					}
					ap_log_error(APLOG_MARK, APLOG_ERR, 0, r->server, "Using fallback worker '%s' for uri '%s' and args '%s'", worker_host, r->unparsed_uri, r->args);
				}
				else {

					
					


					// if we do not need to use a site-bound worker, check if the current one is the one we are
					// looking for
					if (try_to_match_site_name_for_worker_host(site_name, *worker, site_bindings_setup) != 1) {
						continue;
					}

					//if (try_to_match_uri_for_worker_host(r->uri, *worker, site_bindings_setup) != 1) {
					//	continue;
					//}
					ap_log_error(APLOG_MARK, APLOG_ERR, 0, r->server, "Using site worker '%s' for uri '%s' and args '%s'", (*worker)->s->hostname, r->unparsed_uri, r->args);
				}

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

static const proxy_balancer_method bybusyness =
{
	"bybusyness",
	&find_best_bybusyness,
	NULL,
	&reset,
	&age
};


static void register_hook(apr_pool_t *p)
{
	ap_register_provider(p, PROXY_LBMETHOD, "bybusyness", "0", &bybusyness);
}


// APACHE CONFIG FILE
// ==================


/* Handler for the "examplePath" directive */
const char *workerbinding_set_config_path(cmd_parms *cmd, void *cfg, const char *arg)
{
	// lazy-load the config file
	if (site_bindings_setup.binding_count == 0) {
		site_bindings_setup = read_site_config_from(arg);
	}

	ap_log_error(APLOG_MARK, APLOG_ERR, 0, ap_server_conf, "Loaded %d worker bindings from '%s'", site_bindings_setup.binding_count, arg);


	return NULL;
}

// Apache config directives
static const command_rec        workerbinding_directives[] =
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
