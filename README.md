# HTTPOverSCTP

Playground for running HTTP over SCTP experiments.

## Clients

The Firefox directory contains patches and files necessary for HTTP over SCTP support in firefox.
The patches were tested on Ubuntu.
They are based on the work of Jonathan Leighton and Preethi Natarajan from the University of Delaware.
The original patches are available at [~leighton](https://www.eecis.udel.edu/~leighton/firefox.html).

A port of [phttpget.c](http://svnweb.freebsd.org/base/head/usr.sbin/portsnap/phttpget/phttpget.c)
adding SCTP support is provided in the file `phttpget.c`.
It runs on FreeBSD and supports pipelining.

## Server

A port of the web server [thttpd](http://acme.com/software/thttpd/) adding SCTP support
is available at [nplab/thttpd](https://github.com/nplab/thttpd). It runs on FreeBSD,
Linux, MacOS X, and Solaris using a kernel SCTP stack and support IPv4 and IPv6.
On FreeBSD and Mac OS X it supports also UDP encapsulation.
An instance running on FreeBSD is reachable at [bsd10](http://bsd10.fh-muenster.de).
It currently supports neither persistent connections nor pipelining.

## Support Tools

The file `packet-http.c.patch` add support for HTTP/SCTP dissection to the current developer
sources of wireshark.

## Links and Papers

* [draft-natarajan-http-over-sctp-02](https://tools.ietf.org/html/draft-natarajan-http-over-sctp-02).
* [Mean Response Time Estimation for HTTP over SCTP in Wireless Environments](http://cs.ou.edu/~netlab/Pub/HTTP-over-SCTP-ICC06.pdf).
* [Multistreamed Web Transport for Developing Regions](http://www.dritte.org/nsdr08/files/papers/s4p2.pdf).

