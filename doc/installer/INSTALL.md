# Install instructions for customizing Palette Director

### Step 1

Modify tableau's apache configuration to include the `config/httpd.mod-palette-director.conf` file:

```
Include C:\PATH_OF_PALETTE_DIRECTOR\config\httpd.mod-palette-director.conf
```

### Step 2

Edit your worker bindings configuration in `config/workers.csv`. More
information about the format of this file can be found in the `HOWTO.md`
file in the

```
Include C:\PATH_OF_PALETTE_DIRECTOR\config\httpd.mod-palette-director.conf
``


