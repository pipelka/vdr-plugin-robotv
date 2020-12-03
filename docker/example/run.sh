#!/bin/sh

docker stop robotv-server > /dev/null 2>&1 || true
docker rm robotv-server > /dev/null 2>&1 || true

docker run \
	-t \
	--name=robotv-server \
	--detach \
	--restart=always \
	-e DVBAPI_ENABLE=1 \
	-e DVBAPI_HOST=192.168.16.10 \
	-e DVBAPI_PORT=2000 \
	-e ROBOTV_PICONSURL=http://192.168.16.10/picons \
	-v /srv/robotv:/data \
	-v /srv/video:/video \
	-p 34892:34892 \
	--device=/dev/dvb \
	pipelka/robotv-server:latest

