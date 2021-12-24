## picobounce

An attempt to make an IRC bouncer out of UNIXy tools connected by
[ringfifo](https://github.com/begriffs/ringfifo). The bouncer uses
netcat to do TLS upstream, and SSH to handle downstream auth.

Currently doesn't maintain enough state for clients to work properly.

### Building

Requires

* POSIX
* Flex
* Bison
* Netcat that supports TLS (OpenBSD's does)
* [libderp](https://github.com/begriffs/libderp)
* [ringfifo](https://github.com/begriffs/ringfifo)

The configure script will set up build flags for libderp and create config.mk
with customizations for the Makefile:

```sh
PKG_CONFIG_PATH=/usr/local/libderp-dev-0.1.0 ./configure
make
```

### Usage

The entrypoint is:

```sh
$ ./bounce
```

Edit the hardcoded username in that script. Store your password in a file
called `creds` for the `irc_auth` program to read.

The bouncer listens on port 3000. Eventually the goal is to make it bind to the
port for only local connections, and use SSH port forwarding to use it
remotely. It requires no client authentication.

### History

Earlier commits had this program as a single monolithic binary. The bouncer
handled TLS itself with Libressl. However, after discovering the
[pounce](https://git.causal.agency/pounce/about/) bouncer, there was no need to
continue this project. I experimented instead with the netcat and named pipes
thing, but lack motivation to finish the functionality.
