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
	-e DVBAPI_OFFSET=4 \
	-e DVBAPI_HOST=192.168.16.12 \
	-e DVBAPI_PORT=2000 \
	-e ROBOTV_PICONSURL=http://192.168.16.12/picons \
	-e ROBOTV_TIMESHIFTDIR=/timeshift \
	-e ROBOTV_MAXTIMESHIFTSIZE=100000000 \
	-e STREAMDEV_CLIENT_ENABLE=0 \
	-e VDR_LOGLEVEL=3 \
	-e VDR_UPDATECHANNELS=3 \
	-e VDR_EPGSCANTYPE=1 \
	-v $PWD/mounts/data:/data \
	-v $PWD/mounts/cache:/data/cache \
	-v $PWD/mounts/video:/video \
	-v $PWD/mounts/timeshift:/timeshift \
	-e SATIP_ENABLE=1 \
	-e SATIP_SERVER="192.168.16.2|DVBS2-8|Kathrein:0x45" \
	-e SATIP_NUMDEVICES=2 \
	-e SATIP_TRANSPORTMODE=0 \
	-e SATIP_PORTRANGE="23000-23007" \
	-e SATIP_ENABLEFRONTENDREUSE=0 \
	-p 3000:3000 \
	-p 2004:2004 \
	-p 6419:6419 \
	-p 34892:34892 \
	-p 23000-23007:23000-23007/udp \
	pipelka/robotv:latest

