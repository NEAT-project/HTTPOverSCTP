# Patches for HTTP over SCTP in Firefox

The patches 1 to 6 are incremental.
Patch 7 combines te patches 1 to 6.
Patch 9 introduces UDP encapsulation for SCTP on Mac.
Patch 8 is a basic patch for FreeBSD to allow an initial build.
Patches 10 and 11 contain the code for Linux, Mac and FreeBSD for SCTP and UDP tunneling.

1. nsprpubOhnePrsctp.patch
   * SCTP support and nrprpub
   * nsprpub/pr/include/prsctp.h has to be added
   * Add the following options to mozconfig:
     * ac_add_options --enable-debug
     * ac_add_options --enable-debug-symbols
     * ac_add_options --enable-optimize
     * ac_add_options –enable-sctp
     * ac_add_options --disable-webrtc
   * After changing configure.in a new configure script has to be created with autoconf
2. nsprpub2.patch
    * nsprpub/pr/src/nspr.def
    * nsprpub/pr/src/pthreads/ptio.c
3. SCTPtest.patch
    * netwerk/base/nsSocketTransport2.cpp
    * an SCTP socket is opened instead of a  TCP socket
    * nsprpub/pr/tests/sctp.c is added to test client/server communication
4. readPrefs.patch
    * Reading preferences cannot be initiated in BuildSocket, because it is not the main thread
    * Some new methods to get access to the preferences.
    * When disabling SCTP, there is still a compile error. To avoid it, edit netwerk/base/nsSocketTransport2.cpp.
      In the constructor nsSocketTransport(), enclose the last 3 lines (mNextSctpInputStreamId (-1) ..) in #ifdef 
      ENABLE_SCTP.
5. setStreams.patch
    * enable setting the stream id and increasing it
    * disable keepAlive
6. Pipelining.patch
    * Handling of the transaction/stream table
    * pipelining support
    * a lot of debug output (not cleaned up yet)
7. SctpFirefoxLinuxDarwin.patch
    * SCTP patch combining all the above Patches
    * to run the sctp test in nsprpub/pr/tests do:
      - change to nsprpub
      - ./configure --enable-sctp --enable-64bit (if run on a 64 bit machine)
      - make
      - cd pr/tests
      - make
      - set environment variable DYLD_LIBRARY_PATH to ../../dist/lib
      - ./sctp
8. firefoxFreebsdBasic.patch
    * Thanks to Jan Beich for providing hints and patches
    * independent Patch for FreeBSD
    * needed to get Firefox compiled as described in [Simple Build](https://developer.mozilla.org/en-US/docs/Simple_Firefox_build)
    * no code for SCTP yet
    * file icu-flags.mozbuild has to be copied to directory intl
9. MacUdpEncaps.patch
    * UDP encapsulation for MacOsX
    * The tunneling port can be set in the browser at about:config network.http.sctp.udp-tunneling-port
    * The default tunneling port is 0, meaning no tunneling.
10. SCTPFreeBSD.patch
    * Adding SCTP support for FreeBSD
    * Includes support for Linux and Darwin
    * Includes SctpFirefoxLinuxDarwin.patch
11. UDPTunnelingFreeBSDMac.patch
    * UDP encapsulation for FreeBSD
    * includes UDP encapsulation for Mac
