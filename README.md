# Building using CMAKE

You'll need an installed apache (with the 'include' folder containing the Apache, APR, and APR-Util
exported headers).


# Configuring Vizql-Worker-Bindings (httpd.conf)


## httpd.conf

After the module is loaded into apache:

```
LoadModule lbmethod_bybusyness_module modules/mod_palette_director.so
```

Add the location of the binding configuration file:

```
WorkerBindingConfigPath "d:/config/workerbinding.conf"
AuthoringBindingConfigPath "d:/config/authoringbinding.conf"
BackgrounderBindingConfigPath "d:/config/backgrounder-binding.conf"
```

Note: the location of the background is given only for displaying it
on the status UI, otherwise this configuration isnt used by the apache
module.

## Binding config file

The format of the configuration file is identical to the [Background Worker Binding Configuration](https://github.com/palette-software/palette-director/wiki/Worker-Binding-Configuration), except for one important detail:

The host names must be the same as they are for the ProxyPass directives in the apache configuration file (`httpd.conf`). This means that if a host is declared by name (like TABLEAU-PRIMARY1 or tableau-primary.local) then the exact same name must be used in the configuration file. If `httpd.conf` has an IP for a worker, that IP must be used.

So if your HTTPD conf has the following setup:

```
<Proxy balancer://A>
	BalancerMember http://qa.local/QA route=QA loadfactor=25
	BalancerMember http://marketing.local/Marketing route=Marketing loadfactor=25
  BalancerMember http://ceo.local/ceo route=ceo loadfactor=25
	BalancerMember http://fallback.local/fallback route=fallback loadfactor=25
	ProxySet stickysession=ROUTEID
</Proxy>
```

Then the configuration file may look like this:

```csv
site,host,binding
Marketing,marketing.local,prefer
Marketing,ceo.local,forbid
QA,qa.local,prefer
QA,ceo.local,forbid
QA,fallback.local,forbid
CEO,ceo.local,prefer
CEO,qa.local,forbid
Default,fallback.local,prefer
CEO,marketing.local,forbid
```

And if your config has IPs:

```
<Proxy balancer://A>
	BalancerMember http://192.168.0.2/QA route=QA loadfactor=25
	BalancerMember http://192.168.0.3/Marketing route=Marketing loadfactor=25
  BalancerMember http://192.168.0.4/ceo route=ceo loadfactor=25
	BalancerMember http://192.168.0.1/fallback route=fallback loadfactor=25
	ProxySet stickysession=ROUTEID
</Proxy>
```

Then the configuration file may look like this:

```csv
site,host,binding
Marketing,192.168.0.3,prefer
Marketing,192.168.0.4,forbid
QA,192.168.0.2,prefer
QA,192.168.0.4,forbid
QA,192.168.0.1,forbid
CEO,192.168.0.4,prefer
CEO,192.168.0.2,forbid
Default,192.168.0.1,prefer
CEO,192.168.0.3,forbid
```



# Status page

To allow the module to serve status pages, add the
`palette-director-status` handler to a suitable
location:

```
<Location /palette-director-status>
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

