#!/bin/sh

TOP=`dirname $0`
cd $TOP
PWD=`pwd`

mkdir -p mounts/data
mkdir -p mounts/cache
mkdir -p mounts/video

docker run \
	-ti \
	--rm \
	--name=robotv-server \
	--cap-add=SYS_NICE \
	--cap-add=NET_ADMIN \
	-e DVBAPI_ENABLE=1 \
	-e DVBAPI_HOST=192.168.16.11 \
	-e DVBAPI_PORT=2000 \
	-e ROBOTV_PICONSURL=http://192.168.16.10/picons \
	-e ROBOTV_TIMESHIFTDIR=/timeshift \
	-e ROBOTV_MAXTIMESHIFTSIZE=100000000 \
	-e VDR_LOGLEVEL=3 \
	-e VDR_UPDATECHANNELS=3 \
	-v $PWD/mounts/data:/data \
	-v $PWD/mounts/cache:/data/cache \
	-v $PWD/mounts/video:/video \
	-v $PWD/mounts/timeshift:/timeshift \
	-e SATIP_NUMDEVICES=2 \
	--net=host \
	-p 34892:34892 \
	pipelka/robotv:latest

