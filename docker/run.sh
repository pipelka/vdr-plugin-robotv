#!/bin/sh

TOP=`dirname $0`
cd $TOP
PWD=`pwd`

docker stop robotv-server > /dev/null 2>&1 || true
docker rm robotv-server > /dev/null 2>&1 || true

mkdir -p mounts/data
mkdir -p mounts/cache
mkdir -p mounts/video

#    -e ROBOTV_MAXTIMESHIFTSIZE=5000000000 \

docker run \
    -ti \
    --rm \
    --name=robotv \
    --privileged \
    --net=host \
    -e STREAMDEV_CLIENT_ENABLE=1 \
    -e STREAMDEV_CLIENT_REMOTE=192.168.16.10 \
    -e ROBOTV_PICONSURL=http://192.168.16.10/picons \
    -e ROBOTV_TIMESHIFTDIR=/timeshift \
    -e ROBOTV_MAXTIMESHIFTSIZE=100000000 \
    -e VDR_LOGLEVEL=3 \
    -e VDR_UPDATECHANNELS=5 \
    -v $PWD/mounts/data:/data \
    -v $PWD/mounts/cache:/data/cache \
    -v $PWD/mounts/video:/video \
    -v $PWD/mounts/timeshift:/timeshift \
    -p 34892:34892 \
    -p 6419:6419 \
    -p 3000:3000 \
    -p 2004:2004 \
    --device=/dev/dvb \
    pipelka/robotv:latest
