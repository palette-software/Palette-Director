```
palette-director
Copyright (C) 2016 brilliant-data.com

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
```

1. Connect to the Tableau repository PostgreSQL database `workgroup` with user `tblwgadmin`.
    * The repository runs on one of Tableau server hosts, not necessarily the primary.
    * The `tblwgadmin` user's password can be found in the workgroup.yml configuration file in the `data\tabsvc\config` directory.
    * The repository listens by default on port 8060 but this setting can also be found in `workgroup.yml`.

1. Create a `backgrounder` role that will be used by the backgrounder

        CREATE ROLE backgrounder WITH LOGIN PASSWORD '88ba3d556a71e13460fbb18bbf67a27fd33d1e7e' IN ROLE rails;

2. Change the backgrounder role on all nodes in `tabsvc\config\backgrounder.properties`

    Replace this line: `jdbc.username=rails` with `jdbc.username=backgrounder`
    Also replace the password at line `jdbc.password`

3. Restart Tableau Server

4. Create a binding config file, e.g. `C:\workers.csv` that can be read by Tableau's Postgres

5. Create a foreign table for the config:

        CREATE SCHEMA palette;

        GRANT ALL ON SCHEMA palette TO PUBLIC;

        CREATE EXTENSION file_fdw;

        CREATE SERVER bg_wb_config FOREIGN DATA WRAPPER file_fdw;

        CREATE TYPE palette.worker_binding_enum AS ENUM('prefer', 'allow', 'forbid');

        CREATE FOREIGN TABLE palette.worker_binding (site text, host inet, binding palette.worker_binding_enum) SERVER bg_wb_config OPTIONS (filename 'C:\\workers.csv', format 'csv', header 'true');

        GRANT SELECT ON palette.worker_binding TO PUBLIC;

5. Create a facade for the `background_jobs` table like so:

        CREATE VIEW palette.worker_binding_id AS
          SELECT s.id AS site_id, wb.host, wb.binding
          FROM palette.worker_binding wb
          JOIN public._sites s ON s.name = wb.site;

        CREATE VIEW palette.worker_binding_id_active_preferred AS
          SELECT *
          FROM palette.worker_binding_id
          WHERE host IN (SELECT DISTINCT client_addr FROM pg_catalog.pg_stat_activity WHERE usename = 'backgrounder')
            AND binding = 'prefer';

        CREATE VIEW palette.background_jobs AS
          SELECT bj.*
          FROM public.background_jobs bj
          LEFT JOIN palette.worker_binding_id wb ON bj.site_id = wb.site_id AND inet_client_addr() = wb.host
          WHERE
            CASE
              WHEN wb.binding = 'forbid' THEN false
              WHEN wb.binding = 'prefer' THEN true
              WHEN bj.site_id IS NULL THEN true
              ELSE bj.site_id NOT IN (SELECT site_id FROM palette.worker_binding_id_active_preferred)
            END;

        GRANT SELECT ON palette.worker_binding_id TO PUBLIC;
        GRANT SELECT ON palette.worker_binding_id_active_preferred TO PUBLIC;
        GRANT ALL ON palette.background_jobs TO PUBLIC;

        CREATE RULE "_INSERT" AS ON INSERT TO palette.background_jobs DO INSTEAD INSERT INTO public.background_jobs VALUES ((NEW).*);

        CREATE RULE "_UPDATE" AS ON UPDATE TO palette.background_jobs DO INSTEAD UPDATE public.background_jobs SET id = new.id, job_type = new.job_type, progress = new.progress, args = new.args, notes = new.notes, updated_at = new.updated_at, created_at = new.created_at, completed_at = new.completed_at, started_at = new.started_at, job_name = new.job_name, finish_code = new.finish_code, priority = new.priority, title = new.title, created_on_worker = new.created_on_worker, processed_on_worker = new.processed_on_worker, link = new.link, lock_version = new.lock_version, backgrounder_id = new.backgrounder_id, serial_collection_id = new.serial_collection_id, site_id = new.site_id, subtitle = new.subtitle, language = new.language, locale = new.locale, correlation_id = new.correlation_id WHERE background_jobs.id = old.id;

        CREATE RULE "_DELETE" AS ON DELETE TO palette.background_jobs DO INSTEAD DELETE FROM public.background_jobs WHERE id = OLD.id;

        ALTER ROLE backgrounder SET search_path = palette, public;

6. Insert some lines into the configuration file (refer to the instructions [[here|Worker-Binding-Configuration]])
7. Test that it is working. Run a background_jobs query from one of the workers and verify the results:

        SELECT id, title, job_name, backgrounder_id, updated_at FROM background_jobs ORDER BY 5 desc;
8. ???
9. PROFIT!
