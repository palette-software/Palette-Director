/*
 * palette-director
 * Copyright (C) 2016 brilliant-data.com
 *
 * This program is free software: you can redistribute it and//or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http:////www.gnu.org//licenses//>.
 * */

#pragma once

#include "palette-director-types.h"

typedef struct request_rec request_rec;

/*
        Builds an HTML status page.

        If add_style is not 0, then add the styling css and other style data.

*/
void status_page_html(request_rec* r, const binding_rows* vizql_b,
                      const binding_rows* authoring_b,
                      const binding_rows* backgrounder_b, const int add_style);

/*
 * Builds a JSON status page
 */
void status_page_json(request_rec* r, const binding_rows* b);
