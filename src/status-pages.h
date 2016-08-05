#ifndef HTTPD_STATUS_PAGES_H
#define HTTPD_STATUS_PAGES_H

#include "palette-director-types.h"

typedef struct request_rec request_rec;

/*
        Builds an HTML status page.

        If add_style is not 0, then add the styling css and other style data.

*/
void status_page_html(request_rec* r, const binding_rows* b,
                      const int add_style);

/*
 * Builds a JSON status page
 */
void status_page_json(request_rec* r, const binding_rows* b);

#endif  // HTTPD_STATUS_PAGES_H
