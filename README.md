# HTTPOverSCTP

Playground for running HTTP over SCTP experiments.

## Clients

### Firefox Nightly
The Firefox directory contains patches and files necessary for HTTP over SCTP support in firefox.
The patches were tested on Ubuntu.
They are based on the work of Jonathan Leighton and Preethi Natarajan from the University of Delaware.
The original patches are available at [~leighton](https://www.eecis.udel.edu/~leighton/firefox.html).

### phttpget
A port of [phttpget.c](http://svnweb.freebsd.org/base/head/usr.sbin/portsnap/phttpget/phttpget.c)
adding SCTP support is provided in the file `phttpget.c`.
It runs on FreeBSD, Linux, MacOS X (using the SCTP NKE), and Solaris. It uses HTTP 1.1 and supports pipelining.

The transport protocol can be selected by setting the `HTTP_TRANSPORT_PROTOCOL` environment variable.
Supported values are `TCP` and `SCTP`. If the variable is not set, TCP is used.

The remote UDP encapsulation port can be configured by setting the `HTTP_SCTP_UDP_ENCAPS_PORT` environment
variable. Supported values are `0`, ..., `65535`. If it is unset or set to `0`, no UDP encapsulation
is used. Please note that for using UDP encapsulation, the local UDP encapsulation port must also be set
to a non-zero value. You can use `sudo sysctl -w net.inet.sctp.udp_tunneling_port=9899` on FreeBSD.
Please note that UDP encapsulation is only supported on FreeBSD and MacOS X (with the SCTP NKE).
The following should work on FreeBSD using a tcsh:
```
env HTTP_TRANSPORT_PROTOCOL=SCTP HTTP_SCTP_UDP_ENCAPS_PORT=9899 phttpget bsd10.fh-muenster.de index.html
```

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

