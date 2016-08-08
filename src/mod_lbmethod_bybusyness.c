#include "mod_proxy.h"

#include "palette-director-types.h"
#include "status-pages.h"

#include "config-loader.h"

// FWD
// ===
static int status_page_http_handler(request_rec* r);

// MODULE DEFINITIONS
// ==================

module AP_MODULE_DECLARE_DATA lbmethod_bybusyness_module;

static const char* PALETTE_DIRECTOR_MODULE_NAME = "lbmethod_bybusyness_module";

/////////////////////////////////////////////////////////////////////////////

// Worker matching
// ===============

// Returns the site name for the request (if there is one), or NULL if no site
// available in the request
static const char* get_site_name(const request_rec* r,
                                 const binding_rows* setup) {
  const char* val = NULL;
  const char* key = NULL;
  const char* site_name = NULL;
  const char* data = r->args;

  if (r->args == NULL) {
    return NULL;
  }

  while (*data && (val = ap_getword(r->pool, &data, '&'))) {
    key = ap_getword(r->pool, &val, '=');
    ap_unescape_url((char*)key);
    ap_unescape_url((char*)val);

    // If the key is ':site', then we have a site
    if (strcmp(key, ":site") == 0) {
      return val;
    }
  }

  // Check if we can actually match this site

  // if no handler or even no name, return NULL
  return NULL;
}

/////////////////////////////////////////////////////////////////////////////

static binding_rows workerbinding_configuration = {0, 0};

// Load Balancer code
// ==================

static int (*ap_proxy_retry_worker_fn)(const char* proxy_function,
                                       proxy_worker* worker,
                                       server_rec* s) = NULL;

static proxy_worker* find_best_bybusyness_from_list(
    proxy_balancer* balancer, request_rec* r,

    const char* site_name,

    proxy_worker_slice workers_matched
    // size_t matched_worker_count
    ) {
  size_t i, workers_matched_count = workers_matched.count;
  proxy_worker** worker;
  proxy_worker* mycandidate = NULL;
  int cur_lbset = 0;
  int max_lbset = 0;
  int checking_standby;
  int checked_standby;

  int total_factor = 0;

  /* First try to see if we have available candidate */
  do {
    checking_standby = checked_standby = 0;
    while (!mycandidate && !checked_standby) {
      worker = workers_matched.entries;
      for (i = 0; i < workers_matched_count; i++, worker++) {
        // ORIGINAL LB_BYBUSYNESS METHOD
        // =============================

        if (!checking_standby) { /* first time through */
          if ((*worker)->s->lbset > max_lbset) max_lbset = (*worker)->s->lbset;
        }
        if (((*worker)->s->lbset != cur_lbset) ||
            (checking_standby ? !PROXY_WORKER_IS_STANDBY(*worker)
                              : PROXY_WORKER_IS_STANDBY(*worker)) ||
            (PROXY_WORKER_IS_DRAINING(*worker))) {
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

          if (!mycandidate || (*worker)->s->busy < mycandidate->s->busy ||
              ((*worker)->s->busy == mycandidate->s->busy &&
               (*worker)->s->lbstatus > mycandidate->s->lbstatus))
            mycandidate = *worker;
        }
      }

      checked_standby = checking_standby++;
    }

    cur_lbset++;

  } while (cur_lbset <= max_lbset && !mycandidate);

  if (mycandidate) {
    mycandidate->s->lbstatus -= total_factor;
    ap_log_error(APLOG_MARK, APLOG_DEBUG, 0, r->server,
                 APLOGNO(01212) "proxy: bybusyness selected worker \"%s\" : "
                                "busy %" APR_SIZE_T_FMT " : lbstatus %d",
                 mycandidate->s->name, mycandidate->s->busy,
                 mycandidate->s->lbstatus);
  }

  return mycandidate;
}

/*
        Load balancer entry point.
*/
static proxy_worker* find_best_bybusyness(proxy_balancer* balancer,
                                          request_rec* r) {
  const char* site_name = NULL;
  size_t i;

  // The matched workers list
  proxy_worker_slice prefered_workers, allowed_workers;

  // create a slice of workers
  proxy_worker_slice workers_available = {
      (proxy_worker**)balancer->workers->elts,
      (size_t)balancer->workers->nelts};

  // The output
  proxy_worker* candidate = NULL;

  // Check if we can actually handle this request
  if (!ap_proxy_retry_worker_fn) {
    ap_proxy_retry_worker_fn = APR_RETRIEVE_OPTIONAL_FN(ap_proxy_retry_worker);
    if (!ap_proxy_retry_worker_fn) {
      /* can only happen if mod_proxy isn't loaded */
      return NULL;
    }
  }

  ap_log_error(APLOG_MARK, APLOG_DEBUG, 0, r->server,
               APLOGNO(01211) "proxy: Entering bybusyness for BALANCER (%s)",
               balancer->s->name);

  // get the site name
  site_name = get_site_name(r, &workerbinding_configuration);
  if (site_name == NULL) {
    ap_log_error(APLOG_MARK, APLOG_DEBUG, 0, r->server,
                 "Cannot find site name for uri: '%s'  -- with args '%s' ",
                 r->unparsed_uri, r->args);

  } else {
    ap_log_error(APLOG_MARK, APLOG_DEBUG, 0, r->server,
                 "Got site name  '%s' for uri '%s' and args '%s'", site_name,
                 r->unparsed_uri, r->args);
  }

  // Filter the workers list down
  //////////////////////////////////////////////////////////////

  // filter the workers list down
  prefered_workers =
      get_handling_worker_for(workerbinding_configuration, workers_available,
                              site_name, kBINDING_PREFER);
  allowed_workers =
      get_handling_worker_for(workerbinding_configuration, workers_available,
                              site_name, kBINDING_ALLOW);

  for (i = 0; i < prefered_workers.count; ++i) {
    ap_log_error(APLOG_MARK, APLOG_DEBUG, 0, r->server,
                 "== Preferes worker'%s'",
                 prefered_workers.entries[i]->s->hostname);
  }

  for (i = 0; i < allowed_workers.count; ++i) {
    ap_log_error(APLOG_MARK, APLOG_DEBUG, 0, r->server, "-- Allows worker'%s'",
                 allowed_workers.entries[i]->s->hostname);
  }

  ap_log_error(APLOG_MARK, APLOG_DEBUG, 0, r->server,
               APLOGNO(01211) "found %lu dedicated, %lu fallback workers to "
                              "handle te request",
               prefered_workers.count, prefered_workers.count);

  // Look for actual candidates
  //////////////////////////////////////////////////////////////

  // Check the prefered candidates
  if (prefered_workers.count > 0) {
    // Check dedicated workers first
    candidate = find_best_bybusyness_from_list(balancer, r, site_name,
                                               prefered_workers);
  }

  // if no prefered candidates, go to the allowed ones
  if (candidate == NULL && allowed_workers.count > 0) {
    // fallback workers
    candidate =
        find_best_bybusyness_from_list(balancer, r, site_name, allowed_workers);
  }

  // Free the allocated data
  free_proxy_worker_slice(&prefered_workers);
  free_proxy_worker_slice(&allowed_workers);
  return candidate;
}

/* assumed to be mutex protected by caller */
static apr_status_t reset(proxy_balancer* balancer, server_rec* s) {
  int i;
  proxy_worker** worker;
  worker = (proxy_worker**)balancer->workers->elts;
  for (i = 0; i < balancer->workers->nelts; i++, worker++) {
    (*worker)->s->lbstatus = 0;
    (*worker)->s->busy = 0;
  }
  return APR_SUCCESS;
}

static apr_status_t age(proxy_balancer* balancer, server_rec* s) {
  return APR_SUCCESS;
}

// Declare the lbmethod

static const proxy_balancer_method bybusyness = {
    "bybusyness", &find_best_bybusyness, NULL, &reset, &age};

// MAIN ENTRY POINT FOR INIT
// =========================

static void register_hook(apr_pool_t* p) {
  // Register the LBMethod
  ap_register_provider(p, PROXY_LBMETHOD, "bybusyness", "0", &bybusyness);
  // Register the status page hook
  ap_hook_handler(status_page_http_handler, NULL, NULL, APR_HOOK_FIRST);
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
  if (!r->handler || strcmp(r->handler, PALETTE_DIRECTOR_STATUS_HANDLER)) {
    return (DECLINED);
  }

  // if we handle it
  {
    // We need style if our url arg is simply 'with-style'
    const int requires_style =
        (r->args != NULL) && (strcmp(r->args, "with-style") == 0);

    // Check for content-types.
    // Start with HTML with optional style
    if (uri_matches(r, "*html"))
      status_page_html(r, &workerbinding_configuration, requires_style);
    // JSON
    else if (uri_matches(r, "*json"))
      status_page_json(r, &workerbinding_configuration);
    // The fallback is HTML without style for now
    else
      status_page_html(r, &workerbinding_configuration, TRUE);

    return OK;
  }
}

// APACHE CONFIG FILE
// ==================

/* Handler for the "WorkerBindingConfigPath" directive */
static const char* workerbinding_set_config_path(cmd_parms* cmd, void* cfg,
                                                 const char* arg) {
  // Check if we have a loaded config already.
  if (workerbinding_configuration.count == 0) {
    // site_bindings_setup = read_site_config_from(arg);
    workerbinding_configuration = parse_csv_config(arg);
    ap_log_error(APLOG_MARK, APLOG_NOTICE, 0, ap_server_conf,
                 "Loaded %lu worker bindings from '%s'",
                 workerbinding_configuration.count, arg);
  } else {
    // if yes, log the fact that we tried to add to the config
    ap_log_error(APLOG_MARK, APLOG_ERR, 0, ap_server_conf,
                 "Duplicate worker bindings config files: config already "
                 "loaded at the  WorkerBindingConfigPath '%s'  directive",
                 arg);
  }
  return NULL;
}

// Apache config directives.
static const command_rec workerbinding_directives[] = {
    AP_INIT_TAKE1("WorkerBindingConfigPath", workerbinding_set_config_path,
                  NULL, RSRC_CONF, "The path to the workerbinding config"),
    {NULL}};

// MODULE DECLARATION
// ==================

AP_DECLARE_MODULE(lbmethod_bybusyness) = {
    STANDARD20_MODULE_STUFF,
    NULL,                     /* create per-directory config structure */
    NULL,                     /* merge per-directory config structures */
    NULL,                     /* create per-server config structure */
    NULL,                     /* merge per-server config structures */
    workerbinding_directives, /* Any directives we may have for httpd */
    register_hook             /* register hooks */
};
