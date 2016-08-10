# Configuration format for Palette Director

You can configure which worker does what jobs in a Tableau Server cluster.

These are the existing Tableau job types Palette Director knows about:
* Site-bound interactor sessions
* Site-bound background jobs
* Non-site-bound background jobs

Non-site-bound background jobs are maintenance tasks that will continue to run on all workers. Site-bound jobs can however be assigned to worker hosts via Palette Director.

## Format
Configuration goes in a UTF-8 encoded CSV file \<where?\> with the header `site,host,binding`

Explanation of fields:
* `site`: The name of the site to bind.
* `host`: IP address of a Tableau worker host.
* `binding`: The binding status of the listed site-host pair. Can be `prefer`, `allow`, or `forbid`.
    * `prefer` lines take precedence over `allow` lines. One of the preferred hosts will be chosen if possible.
    * `allow` is the default binding. An allowed host will be chosen if no preferred hosts are available.
    * `forbid` means the site's jobs are never allowed to run on the host.

Note: If a site-host pair is omitted from the configuration, its binding is `allow` by default. This makes an `allow` line purely informative.

## Use cases
### Bind a site to a worker with fallback
Include the following line in the configuration:
```
SuperSite,worker0.host,prefer
```
This will cause all SuperSite related server activities to happen on worker0.host when it is available.
### Bind a site to a worker without fallback
Forbid the site to access all hosts it is not allowed to run on.
```
SuperSite,worker1.host,forbid
```


## Examples

The following examples have three sites available on the server:

- __CEO__
- __QA__
- __Marketing__

And four worker machines with the following hostnames:

- `qa.local`
- `marketing.local`
- `ceo.local`
- `fallback.local`

If we want the CEO to have a dedicated worker machine, but keep that machine as a fallback when the cluster is overloaded:

```csv
site,host,binding
CEO,ceo.local,prefer
```

If we want the CEO to have a dedicated worker machine, which only runs his reports at all times:

```csv
site,host,binding
Marketing,ceo.local,forbid
QA,ceo.local,forbid
CEO,ceo.local,prefer
```

If we want the CEO to have a dedicated worker machine, and dont want the CEO reports to run on any other machine (even if the CEO machine is overloaded):

```csv
site,host,binding
Marketing,ceo.local,forbid
QA,ceo.local,forbid
CEO,ceo.local,prefer
CEO,marketing.local,forbid
CEO,qa.local,forbid
CEO,fallback.local,forbid
```

If we want to have the QA machine as a fallback for Marketing, but not the other way around (QA should not add extra load to machines dedicated to Marketing, but QA should offer up its capacity when the Marketing machine is down), and dedicate a machine to the CEO, which can still be used as a fallback on cluster overload:

```csv
site,host,binding
Marketing,marketing.local,prefer
Marketing,ceo.local,forbid
QA,qa.local,prefer
QA,ceo.local,forbid
QA,marketing.local,forbid
CEO,ceo.local,prefer
```
