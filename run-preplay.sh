#!/bin/sh

# set protocol: SCTP|TCP
export HTTP_TRANSPORT_PROTOCOL=SCTP

# use named pipes
export HTTP_PIPE=SICHER_DOCH

$2 ./phttpget $1
