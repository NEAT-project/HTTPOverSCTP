#!/bin/sh


if [ "$#" -lt 1 ]; then
	export HTTP_TRANSPORT_PROTOCOL=TCP
else
	export HTTP_TRANSPORT_PROTOCOL=$1
fi
echo 'using' $HTTP_TRANSPORT_PROTOCOL

$2 ./phttpget 192.168.56.4 \
chunks/chunk-0-0.jpg \
chunks/chunk-0-1.jpg \
chunks/chunk-0-2.jpg \
chunks/chunk-0-3.jpg \
chunks/chunk-0-4.jpg \
chunks/chunk-0-5.jpg \
chunks/chunk-0-6.jpg \
chunks/chunk-0-7.jpg \
chunks/chunk-0-8.jpg \
chunks/chunk-0-9.jpg \
chunks/chunk-0-10.jpg \
chunks/chunk-0-11.jpg \
chunks/chunk-0-12.jpg \
chunks/chunk-0-13.jpg \
chunks/chunk-0-14.jpg \
chunks/chunk-0-15.jpg \
chunks/chunk-0-16.jpg \
chunks/chunk-0-17.jpg \
chunks/chunk-0-18.jpg \
chunks/chunk-0-19.jpg \
chunks/chunk-0-20.jpg \
chunks/chunk-0-21.jpg \
chunks/chunk-0-22.jpg \
chunks/chunk-0-23.jpg \
chunks/chunk-0-24.jpg \
chunks/chunk-0-25.jpg \
chunks/chunk-0-26.jpg \
chunks/chunk-0-27.jpg \
chunks/chunk-0-28.jpg \
chunks/chunk-0-29.jpg \
chunks/chunk-0-30.jpg \
chunks/chunk-0-31.jpg \
chunks/chunk-0-32.jpg \
chunks/chunk-0-33.jpg \
chunks/chunk-0-34.jpg \
chunks/chunk-0-35.jpg \
chunks/chunk-0-36.jpg \
chunks/chunk-0-37.jpg \
chunks/chunk-0-38.jpg \
chunks/chunk-0-39.jpg \
chunks/chunk-0-40.jpg \
chunks/chunk-0-41.jpg \
chunks/chunk-0-42.jpg \
chunks/chunk-0-43.jpg \
chunks/chunk-0-44.jpg \
chunks/chunk-0-45.jpg \
chunks/chunk-0-46.jpg \
chunks/chunk-0-47.jpg \
chunks/chunk-0-48.jpg \
chunks/chunk-0-49.jpg \
chunks/chunk-0-0.jpg \
chunks/chunk-0-1.jpg \
chunks/chunk-0-2.jpg \
chunks/chunk-0-3.jpg \
chunks/chunk-0-4.jpg \
chunks/chunk-0-5.jpg \
chunks/chunk-0-6.jpg \
chunks/chunk-0-7.jpg \
chunks/chunk-0-8.jpg \
chunks/chunk-0-9.jpg \
chunks/chunk-0-10.jpg \
chunks/chunk-0-11.jpg \
chunks/chunk-0-12.jpg \
chunks/chunk-0-13.jpg \
chunks/chunk-0-14.jpg \
chunks/chunk-0-15.jpg \
chunks/chunk-0-16.jpg \
chunks/chunk-0-17.jpg \
chunks/chunk-0-18.jpg \
chunks/chunk-0-19.jpg \
chunks/chunk-0-20.jpg \
chunks/chunk-0-21.jpg \
chunks/chunk-0-22.jpg \
chunks/chunk-0-23.jpg \
chunks/chunk-0-24.jpg \
chunks/chunk-0-25.jpg \
chunks/chunk-0-26.jpg \
chunks/chunk-0-27.jpg \
chunks/chunk-0-28.jpg \
chunks/chunk-0-29.jpg \
chunks/chunk-0-30.jpg \
chunks/chunk-0-31.jpg \
chunks/chunk-0-32.jpg \
chunks/chunk-0-33.jpg \
chunks/chunk-0-34.jpg \
chunks/chunk-0-35.jpg \
chunks/chunk-0-36.jpg \
chunks/chunk-0-37.jpg \
chunks/chunk-0-38.jpg \
chunks/chunk-0-39.jpg \
chunks/chunk-0-40.jpg \
chunks/chunk-0-41.jpg \
chunks/chunk-0-42.jpg \
chunks/chunk-0-43.jpg \
chunks/chunk-0-44.jpg \
chunks/chunk-0-45.jpg \
chunks/chunk-0-46.jpg \
chunks/chunk-0-47.jpg \
chunks/chunk-0-48.jpg \
chunks/chunk-0-49.jpg

rm *.jpg
