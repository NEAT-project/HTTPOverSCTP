#!/bin/sh

# set protocol: SCTP|TCP
export HTTP_TRANSPORT_PROTOCOL=TCP

# use named pipes
export HTTP_PIPE=SICHER_DOCH

./phttpget $1
