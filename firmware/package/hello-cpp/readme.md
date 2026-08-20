# hello-cpp

`BR2_PACKAGE_HELLO_CPP` is the C++ form of the Makefile example. It demonstrates
multiple C++ source modules, the target C++ runtime, and reading a simple
configuration file. It installs:

```text
/usr/bin/hello-cpp
/etc/hello-cpp.ini
```

Run it without arguments:

```sh
hello-cpp
```

The package requires C++ support in the Buildroot toolchain. Like `hello-c`, it
uses the unified board DTB and does not enable optional storage, networking, or
display drivers.
