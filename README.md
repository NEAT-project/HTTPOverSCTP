# HTTPOverSCTP
Playground for HTTP over SCTP experiments

The Firefox directory contains patches and files necessary for HTTP over SCTP support in firefox.
The patches were tested on Ubuntu.
They are based on the work of Jonathan Leighton and Preethi Natarajan from the University of Delaware.

The file `packet-http.c.patch` add support for HTTP/SCTP dissection to the current developer
sources of wireshark.
