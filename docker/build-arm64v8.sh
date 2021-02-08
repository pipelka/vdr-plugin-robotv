#!/bin/sh

cd `dirname $0`
TOP=`pwd`
VERSION=`git describe`

usage() {
    echo "build script for the 'robotv' docker image"
    echo ""
    echo "./build-arm64v8.sh"
    echo "\t-h --help             show this help"
    echo ""
}

while [ "$1" != "" ]; do
    PARAM=`echo $1 | awk -F= '{print $1}'`
    VALUE=`echo $1 | awk -F= '{print $2}'`
    case $PARAM in
        -h | --help)
            usage
            exit
            ;;
        *)
            echo "ERROR: unknown parameter \"$PARAM\""
            usage
            exit 1
            ;;
    esac
    shift
done

rm -Rf opt

docker build \
    --force-rm \
    -t pipelka/robotv:arm64v8-${VERSION} \
    -f ${TOP}/Dockerfile.arm64v8 ${TOP}/..

docker tag pipelka/robotv:${VERSION} pipelka/robotv:arm64v8-latest
