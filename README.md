## picobounce

The simplest IRC bouncer I could make, one that fits my own needs. It requires
SASL over TLS for the upstream server and for the connecting clients. It
accepts only a single client (me!) and forwards them to a single upstream
server. If you want to connect to multiple IRC networks then run multiple
instances of picobounce, each on a different port.

### Building

Builds on OpenBSD with no dependencies. All other platforms require POSIX and
[LibreSSL](https://www.libressl.org/). Additionally, non-BSD platforms require
libbsd-overlay.

The configure script will detect whatever dependencies are necessary on your
system and create config.mk with the customizations.

```sh
./configure
make
```

### Usage

The binary is currently hard-coded to look for the certificate files my.key and
my.crt in the working directory. You can make those files with the openssl
command line tool:

```sh
openssl req -nodes -new -x509 -newkey rsa:2048 -keyout my.key -out my.crt
```

Create a configuration file, e.g. pico.conf:

```ini
local_port=6697
local_user=foo
local_pass=bar
host=chat.freenode.net
port=6697
nick=your_freenode_nick
pass=your_freenode_pass
```

Now run the bouncer:

```sh
./picobounce pico.conf
```

Connect to the bouncer with your IRC client, using the local_user and
local_pass as SASL parameters. Some clients like weechat might not like your
self-signed cert. You may need to run the bouncer on a real server with certs
from a CA like letsencrypt.
