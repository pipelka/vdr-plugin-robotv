#/bin/sh

docker run \
    --rm \
    -ti \
    --cap-add=SYS_NICE \
    -e SATIP_NUMDEVICES=1 \
    -e DVBAPI_ENABLE=1 \
    -e DVBAPI_OFFSET=3 \
    -p 34892:34892 \
    --net=host \
    pipelka/robotv-server:latest
