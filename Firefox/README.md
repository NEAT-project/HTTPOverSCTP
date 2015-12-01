# Patches for HTTP over SCTP in Firefox

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
7. firefoxFreebsdBasic.patch
    * Thanks to Jan Beich for providing hints and patches
    * independent Patch for FreeBSD
    * needed to get Firefox compiled as described in [Simple Build](https://developer.mozilla.org/en-US/docs/Simple_Firefox_build)
    * no code for SCTP yet
