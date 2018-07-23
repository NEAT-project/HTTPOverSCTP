#!/bin/sh
#export HTTP_USER_AGENT="phttpget yeah!"
#export HTTP_TIMEOUT=1 # seconds!
#export HTTP_TRANSPORT_PROTOCOL=TCP # TCP|SCTP
#export HTTP_SCTP_UDP_ENCAPS_PORT=9899
#export HTTP_USE_PIPELINING=NO
#export HTTP_DEBUG=LOG_DBG # LOG_PRG|LOG_ERR|LOG_INF|LOG_DBG
#export HTTP_SCTP_MAX_STREAMS=2
#export HTTP_PIPE=NO # use pipes (e.g. for pReplay)
#export HTTP_IP_PROTOCOL=6

export HTTP_TRANSPORT_PROTOCOL=TCP
./phttpget bsd3.nplab.de files/16M

export HTTP_TRANSPORT_PROTOCOL=SCTP
./phttpget bsd3.nplab.de files/16M

export HTTP_SCTP_UDP_ENCAPS_PORT=9899
export HTTP_TRANSPORT_PROTOCOL=SCTP
./phttpget bsd3.nplab.de files/16M
