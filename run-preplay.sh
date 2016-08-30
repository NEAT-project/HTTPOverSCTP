#!/bin/sh

# protocol: SCTP|TCP
export HTTP_TRANSPORT_PROTOCOL=TCP

# allow pipelining (TCP)
export HTTP_USE_PIPELINING=NO

# use named pipes, uncomment for STDIN
export HTTP_PIPE=YES

# debug level - LOG_PRG|LOG_ERR|LOG_INF|LOG_DBG
export HTTP_DEBUG=LOG_PRG

$2 ./phttpget $1
