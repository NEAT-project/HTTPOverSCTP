# Patches for HTTP over SCTP in Firefox

FirefoxSctp.Patch

* All-in-one patch for FreeBsd, Linux and Mac.
* It includes UDP encapsulation for FreeBsd and Mac
* based on revision 275952 of mozilla-central
* mozconfig has to be added with at least the contents:
  - ac_add_options --enable-sctp
  - ac_add_options --disable-webrtc
