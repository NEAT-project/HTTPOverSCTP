#!/bin/sh
export HTTP_TRANSPORT_PROTOCOL=TCP
echo $HTTP_TRANSPORT_PROTOCOL
valgrind ./phttpget bsd10.nplab.de files/2048kByte.bin
