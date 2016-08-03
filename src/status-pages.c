#include "status-pages.h"

#include "mod_proxy.h"

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
void status_page_html(request_rec* r, const bindings_setup* b, const int add_style) {
    const size_t binding_count = b->binding_count;

    ap_set_content_type(r, "text/html");

    // add style and html wrap if needed
    if (add_style) {

        ap_rprintf(r, "<html>");

        // head
        ap_rprintf(r, "<head>");


        ap_rprintf(r, "<title>Palette Director Workerbinding Status</title>");
        ap_rprintf(r, "<link rel='stylesheet' type='text/css' href='/vizportal.css' />");

        // body
        ap_rprintf(r, "</head><body class='tb-app'>");
    }

    ap_rprintf(r, "<div><h3>VizQL worker bindings</h3></div>");
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

    // close style and html wrap if needed
    if (add_style) {
        ap_rprintf(r, "</body>");
        ap_rprintf(r, "</html>");
    }
}






/* Builds an JSON status page*/
void status_page_json(request_rec* r, const bindings_setup* b) {
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
