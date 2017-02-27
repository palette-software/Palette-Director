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

/*
        Helper to read the configuration from a file.

        Returns a bindings_setup struct. Only the successfully loaded binding
   configs
        are added to the return config.
*/
binding_rows parse_csv_config(const char* path);

/*
        Returns a list of workers where the bindings configuration has with_kind
        set for kind.

        The slices in the return struct must be freed after use.
*/
proxy_worker_slice get_handling_workers_for(const binding_rows bindings_in,
                                            const proxy_worker_slice workers_in,
                                            const char* site_name,
                                            const binding_kind_t with_kind);
