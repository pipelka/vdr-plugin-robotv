#!/bin/sh

# check if the container has the needed terminal devices

[ ! -c /dev/console ] && mknod /dev/console c 5 1 > /dev/null 2>&1
[ -L /dev/ptmx ] && rm -f /dev/ptmx > /dev/null 2>&1
[ ! -c /dev/ptmx ] && mknod /dev/ptmx c 5 2 > /dev/null 2>&1

export LANG="en_US.UTF-8"

CONFDIR=/data/etc

mkdir -p ${CONFDIR}/conf.d
echo "0.0.0.0/0" > ${CONFDIR}/svdrphosts.conf 

[ ! -f ${CONFDIR}/channels.conf ] && cp /opt/templates/channels.conf ${CONFDIR}/
[ ! -f ${CONFDIR}/sources.conf ] && cp /opt/templates/sources.conf ${CONFDIR}/
[ ! -f ${CONFDIR}/diseqc.conf ] && cp /opt/templates/diseqc.conf ${CONFDIR}/
[ ! -f ${CONFDIR}/scr.conf ] && cp /opt/templates/scr.conf ${CONFDIR}/

# DiSeqC

echo "DiSEqC = ${VDR_DISEQC}" > ${CONFDIR}/setup.conf

# UpdateChannels

echo "UpdateChannels = ${VDR_UPDATECHANNELS}" >> ${CONFDIR}/setup.conf

# EPGScanType

echo "EPGScanType = ${VDR_EPGSCANTYPE}" >> ${CONFDIR}/setup.conf

# DVBAPI configuration

echo "dvbapi.LogLevel = 2" >> ${CONFDIR}/setup.conf
echo "dvbapi.OSCamHost = ${DVBAPI_HOST}" >> ${CONFDIR}/setup.conf
echo "dvbapi.OSCamPort = ${DVBAPI_PORT}" >> ${CONFDIR}/setup.conf

# ENABLE / DISABLE DVBAPI

rm -f ${CONFDIR}/conf.d/40-dvbapi.conf

if [ "${DVBAPI_ENABLE}" = "1" ] ; then
    echo "[dvbapi]" > ${CONFDIR}/conf.d/40-dvbapi.conf
    echo "-o ${DVBAPI_OFFSET}" >> ${CONFDIR}/conf.d/40-dvbapi.conf
else
    rm -f ${CONFDIR}/conf.d/40-dvbapi.conf
fi


# SATIP configuration

if [ "${SATIP_ENABLE}" = "1" ] ; then
    mkdir -p ${CONFDIR}/plugins/satip
    echo "[satip]" > ${CONFDIR}/conf.d/50-satip.conf

    if [ ! -z "${SATIP_NUMDEVICES}" ] ; then
        echo "-d ${SATIP_NUMDEVICES}" >> ${CONFDIR}/conf.d/50-satip.conf
    fi

    if [ ! -z "${SATIP_SERVER}" ] ; then
        echo "-s ${SATIP_SERVER}" >> ${CONFDIR}/conf.d/50-satip.conf
    fi

    echo "satip.EnableEITScan = ${SATIP_ENABLEEITSCAN}" >> ${CONFDIR}/setup.conf
else
    [ -f ${CONFDIR}/conf.d/50-satip.conf ] && rm -f ${CONFDIR}/conf.d/50-satip.conf
fi

# VDR configuration

echo "[vdr]" > ${CONFDIR}/conf.d/00-vdr.conf
echo "--chartab=ISO-8859-9" >> ${CONFDIR}/conf.d/00-vdr.conf
echo "--port=6419" >> ${CONFDIR}/conf.d/00-vdr.conf
echo "--watchdog=60" >> ${CONFDIR}/conf.d/00-vdr.conf
echo "--log=${VDR_LOGLEVEL}" >> ${CONFDIR}/conf.d/00-vdr.conf


# EPGSearch configuration

echo "[epgsearch]" > ${CONFDIR}/conf.d/50-epgsearch.conf


# streamdev-server configuration

mkdir -p ${CONFDIR}/plugins/streamdev-server

echo "[streamdev-server]" > ${CONFDIR}/conf.d/50-streamdev-server.conf
echo "0.0.0.0/0" > ${CONFDIR}/plugins/streamdev-server/streamdevhosts.conf
echo "streamdev-server.AllowSuspend = 1" >> ${CONFDIR}/setup.conf


# streamdev-client configuration

if [ "${STREAMDEV_CLIENT_ENABLE}" = "1" ] ; then 
    echo "[streamdev-client]" > ${CONFDIR}/conf.d/50-streamdev-client.conf
    echo "streamdev-client.StreamFilters = 1" >> ${CONFDIR}/setup.conf
    echo "streamdev-client.RemoteIp = ${STREAMDEV_CLIENT_REMOTE}" >> ${CONFDIR}/setup.conf
    echo "streamdev-client.RemotePort = ${STREAMDEV_CLIENT_PORT}" >> ${CONFDIR}/setup.conf
    echo "streamdev-client.StartClient = 1" >> ${CONFDIR}/setup.conf
else
    rm -f ${CONFDIR}/conf.d/50-streamdev-client.conf
fi

# RoboTV configuration

mkdir -p ${CONFDIR}/plugins/robotv
echo "[robotv]" > ${CONFDIR}/conf.d/50-robotv.conf

echo "0.0.0.0/0" > ${CONFDIR}/plugins/robotv/allowed_hosts.conf

echo "TimeShiftDir = ${ROBOTV_TIMESHIFTDIR}" > ${CONFDIR}/plugins/robotv/robotv.conf
echo "MaxTimeShiftSize = ${ROBOTV_MAXTIMESHIFTSIZE}" >> ${CONFDIR}/plugins/robotv/robotv.conf
echo "SeriesFolder = ${ROBOTV_SERIESFOLDER}" >> ${CONFDIR}/plugins/robotv/robotv.conf

if [ ! -z "${ROBOTV_PICONSURL}" ] ; then
    echo "PiconsURL = ${ROBOTV_PICONSURL}" >> ${CONFDIR}/plugins/robotv/robotv.conf
fi

if [ ! -z "${ROBOTV_EPGIMAGEURL}" ] ; then
    echo "EpgImageUrl = ${ROBOTV_EPGIMAGEURL}" >> ${CONFDIR}/plugins/robotv/robotv.conf
fi

/opt/vdr/bin/vdr
