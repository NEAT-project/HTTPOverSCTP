# HTTPOverSCTP
Playground for HTTP over SCTP experiments

The Firefox directory contains patches and files necessary for HTTP over SCTP support in firefox.
The patches were tested on Ubuntu.
They are based on the work of Jonathan Leighton and Preethi Natarajan from the University of Delaware.

The file `packet-http.c.patch` add support for HTTP/SCTP dissection to the current developer
sources of wireshark.

A port of the web server [thttpd](http://acme.com/software/thttpd/) adding SCTP support
is available at [nplab/thttpd](https://github.com/nplab/thttpd). It runs on FreeBSD,
Linux, MacOS X, and Solaris using a kernel SCTP stack and support IPv4 and IPv6.
On FreeBSD and Mac OS X it supports also UDP encapsulation.
An instance running on FreeBSD is reachable at [bsd10](http://bsd10.fh-muenster.de).
It currently supports neither persistent connections nor pipelining.

A port of [phttpget.c](http://svnweb.freebsd.org/base/head/usr.sbin/portsnap/phttpget/phttpget.c)
adding SCTP is provided in the file `phttpget.c`.
It runs on FreeBSD and supports pipelining.
