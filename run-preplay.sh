#!/bin/sh

# set protocol: SCTP|TCP
export HTTP_TRANSPORT_PROTOCOL=SCTP

# use named pipes, uncomment for STDIN
export HTTP_PIPE=SICHER_DOCH

# debug level - LOG_PRG|LOG_ERR|LOG_INF|LOG_DBG
export HTTP_DEBUG=LOG_DBG

$2 ./phttpget $1
