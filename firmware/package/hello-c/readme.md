# hello-c

`BR2_PACKAGE_HELLO_C` is a small C and Makefile example. It builds separate
configuration, arithmetic, and time modules, then installs:

```text
/usr/bin/hello-c
/etc/hello-c.ini
```

Run it without arguments:

```sh
hello-c
```

The program prints an addition result, the current time, and selected values
from `/etc/hello-c.ini`. It uses the unified board DTB and the common minimal
kernel profile; peripheral drivers are added only by packages that require
them.
