# roboTV VDR server plugin

roboTV is a Android TV based frontend for VDR.
This is the VDR server part.

## Build requirements

* VDR 2.4.x
* GCC 4.9 (or higher)

## Repository Branches

* master -> Sources for VDR 2.4.x
* dev -> Development Branch based on "master"

# roboTV docker image

The "robotv" docker image is a turn-key solution to deploy a headless [VDR](http://www.tvdr.de) server and all required plugins to connect [roboTV](https://github.com/pipelka/robotv) clients.

## Prerequisites

- Docker installation is required, see the official installation [docs](https://docs.docker.com/engine/installation/)
- DVB card or SAT>IP server needed

## Running roboTV Server

roboTV can run with various configurations. This sample configuration uses local DVB devices and the dvbapi plugin to access your smartcard but can be configured completely different (see examples below). It also sets an URL clients will use to fetch channel icons (Enigma Picons). The roboTV TCP/IP port must always be exposed.

- Create the data directories on your server

```
sudo mkdir -p /srv/robotv
sudo mkdir -p /srv/video
```

- Start the robotv container

```
docker run --rm -ti \
    --cap-add=SYS_NICE \
    -e DVBAPI_ENABLE=1 \
    -e DVBAPI_HOST=192.168.100.200 \
    -e DVBAPI_PORT=2222 \
    -e ROBOTV_PICONSURL=http://192.168.16.10/picons \
    -v /srv/robotv:/data \
    -v /srv/video:/video \
    -p 34892:34892 \
    --device=/dev/dvb \
    pipelka/robotv
```

## Docker Volumes to store data

| Host Location | Container | Description |
| --- | --- | --- |
| /srv/robotv | /data | VDR cache data and configuration |
| /srv/video | /video | Space for recordings |

You can change these directories to meet your requirements.

##Configuration variables

| Variable | Default | Description |
| --- | --- | ---------- |
| DVBAPI_ENABLE | 0 | enable / disable the dvbapi plugin |
| DVBAPI_HOST | 127.0.0.1 | dvbapi host |
| DVBAPI_PORT | 2000 | dvbapi host port |
| DVBAPI_OFFSET | 0 | dvbapi device offset |
| SATIP_ENABLE | 0 | 0 - disable / 1 - enable SAT>IP plugin |
| SATIP_ENABLEEITSCAN | 1 | 0 - disable / 1 - enable EIT scan |
| SATIP_NUMDEVICES | 2 | number of dvb devices to open on the server |
| SATIP_SERVER | | SAT>IP server configuration |
| ROBOTV_MAXTIMESHIFTSIZE | 4000000000 | Maximum timeshift ringbuffer size in bytes |
| ROBOTV_PICONSURL |  | URL for the enigma channel icons |
| ROBOTV_SERIESFOLDER | Serien | Folder for TV shows |
| ROBOTV_EPGIMAGEURL | | URL for EPG images |
| VDR_LOGLEVEL | 2 | 0 = no logging, 1 = errors only, 2 = errors and info, 3 = errors, info and debug |
| VDR_UPDATECHANNELS | 5 | 0 = disables, 1 = channel names only, 2 = pids only, 3 = channels names and pids, 4 = add new channels, 5 = add new transponders |
| VDR_DISEQC | 0 | 0 = DisEqC disabled | 1 = enabled |
| VDR_EPGSCANTIMEOUT | 5 | The time (in hours) of user inactivity after which the DVB card in a single card system starts scanning channels to keep the EPG up-to-date. A value of '0' completely turns off scanning on both single and multiple card systems. 
| TZ | Europe/Vienna | Timezone to use |

## Ports in use

| Port | Description |
| --- | --- |
| 34892 | roboTV port for client communication (must always be connected) |
| 6419 | VDR svdrp port |

## Examples

- connect to SAT>IP server (autodetect)

```
docker run --rm -ti \
    --cap-add=SYS_NICE \
    -e SATIP_ENABLE = 1 \
    -v /srv/vdr:/data \
    -v /srv/video:/video \
    --net=host \
    pipelka/robotv
```

- connect to SAT>IP server with 4 devices

```
docker run --rm -ti \
    --cap-add=SYS_NICE \
    -e SATIP_ENABLE = 1 \
    -e SATIP_SERVER="192.168.100.201|DVBS2-4|Triax SatIP Converter" \
    -e SATIP_NUMDEVICES=4 \
    -v /srv/vdr:/data \
    -v /srv/video:/video \
    --net=host \
    pipelka/robotv
```

- enable dvbapi with server 192.168.100.200 on port 2222 and pass dvb devices to the container

```
docker run --rm -ti \
    --cap-add=SYS_NICE \
    -e DVBAPI_ENABLE=1 \
    -e DVBAPI_HOST=192.168.100.200 \
    -e DVBAPI_PORT=2222 \
    -v /srv/vdr:/data \
    -v /srv/video:/video \
    -p 34892:34892 \
    --device=/dev/dvb \
    pipelka/robotv
```

- use SAT>IP with dvbapi and set picons url

```
docker run --rm -ti \
    --cap-add=SYS_NICE \
    -e DVBAPI_ENABLE=1 \
    -e DVBAPI_HOST=192.168.100.200 \
    -e DVBAPI_PORT=2222 \
    -e SATIP_ENABLE = 1 \
    -e SATIP_SERVER="192.168.100.202|DVBS2-2|minisatip" \
    -e ROBOTV_PICONS=http://192.168.100.202/picons \
    -v /srv/vdr:/data \
    -v /srv/video:/video \
    --net=host \
    pipelka/robotv
```

## Building the image

To keep the generated docker image as small as possible it is built upon alpine linux.
The dockerfile contains 2 stages for building the image

- build the VDR and plugin binaries in a development "robotv-server-build" image
- install the generated binaries in an alpine:3.6 production environment

To build the image you just need to:

```
./docker/build.sh
```

The script will generate the image:
```
REPOSITORY                    TAG                 IMAGE ID            CREATED             SIZE
pipelka/robotv                0.14.0              ed28e2a9b390        2 minutes ago       19.6MB
pipelka/robotv                latest              ed28e2a9b390        2 minutes ago       19.6MB
```
