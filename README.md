Place APR & APR-UTIL into libs


# Configuring Vizql-Worker-Bindings (httpd.conf)


## httpd.conf

After the module is loaded into apache:

```
LoadModule lbmethod_bybusyness_module modules/mod_lbmethod_bybusyness_palette_director.so
```

Add the location of the binding configuration file:

```
WorkerBindingConfigPath "d:/config/workerbinding.conf"
```

## Binding config file

This file has the following format:

```
Marketing -> marketing.local
QA -> qa.local
* -> fallback.local
```

This file has an entry per line in the following format:

```SiteName -> WorkerHostName```

This line maps the site with the name `SiteName` to the worker(s)
available on the host identified by `WorkerHostName`.

The fallback line:

```* -> FallbackHostName```

Directs traffic for any other site to the provided host.




# Status page

To allow the module to serve status pages, add the
`palette-director-status` handler to a suitable
location:

```
<Location /worker-bindings>
  SetHandler palette-director-status
</Location>
```

After a restart, the status page will be available under:

```http://localhost/worker-bindings```

Different formatted versions are available:

* `http://localhost/worker-bindings` is the full-featured version

* `http://localhost/worker-bindings/html` is the plain HTML (without the
  CSS file linked, so it can be simply loaded into the serverstatus
  document)

* `http://localhost/worker-bindings/json` is the JSON version if further
  processing of the status is needed.

