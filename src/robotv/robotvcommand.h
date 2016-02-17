/*
 *      vdr-plugin-robotv - roboTV server plugin for VDR
 *
 *      Copyright (C) 2016 Alexander Pipelka
 *
 *      https://github.com/pipelka/vdr-plugin-robotv
 *
 *  This Program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2, or (at your option)
 *  any later version.
 *
 *  This Program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  the Free Software Foundation, 675 Mass Ave, Cambridge, MA 02139, USA.
 *  http://www.gnu.org/copyleft/gpl.html
 *
 */

#ifndef ROBOTV_COMMAND_H
#define ROBOTV_COMMAND_H

/** Current RoboTV Protocol Version number */
#define ROBOTV_PROTOCOLVERSION          7


/** Packet types */
#define ROBOTV_CHANNEL_REQUEST_RESPONSE 1
#define ROBOTV_CHANNEL_STREAM           2
#define ROBOTV_CHANNEL_STATUS           5
#define ROBOTV_CHANNEL_SCAN             6


/** Response packets operation codes */

/* OPCODE 1 - 19: RoboTV network functions for general purpose */
#define ROBOTV_LOGIN                 1
#define ROBOTV_GETTIME               2
#define ROBOTV_ENABLESTATUSINTERFACE 3
#define ROBOTV_PING                  7
#define ROBOTV_UPDATECHANNELS        8
#define ROBOTV_CHANNELFILTER         9

/* OPCODE 20 - 39: RoboTV network functions for live streaming */
#define ROBOTV_CHANNELSTREAM_OPEN    20
#define ROBOTV_CHANNELSTREAM_CLOSE   21
#define ROBOTV_CHANNELSTREAM_REQUEST 22
#define ROBOTV_CHANNELSTREAM_PAUSE   23
#define ROBOTV_CHANNELSTREAM_SIGNAL  24

/* OPCODE 40 - 59: RoboTV network functions for recording streaming */
#define ROBOTV_RECSTREAM_OPEN        40
#define ROBOTV_RECSTREAM_CLOSE       41
#define ROBOTV_RECSTREAM_GETBLOCK    42
#define ROBOTV_RECSTREAM_UPDATE      46
#define ROBOTV_RECSTREAM_GETPACKET   47
#define ROBOTV_RECSTREAM_SEEK        48

/* OPCODE 60 - 79: RoboTV network functions for channel access */
#define ROBOTV_CHANNELS_GETCOUNT     61
#define ROBOTV_CHANNELS_GETCHANNELS  63
#define ROBOTV_CHANNELGROUP_GETCOUNT 65
#define ROBOTV_CHANNELGROUP_LIST     66
#define ROBOTV_CHANNELGROUP_MEMBERS  67

/* OPCODE 80 - 99: RoboTV network functions for timer access */
#define ROBOTV_TIMER_GETCOUNT        80
#define ROBOTV_TIMER_GET             81
#define ROBOTV_TIMER_GETLIST         82
#define ROBOTV_TIMER_ADD             83
#define ROBOTV_TIMER_DELETE          84
#define ROBOTV_TIMER_UPDATE          85

/* OPCODE 100 - 119: RoboTV network functions for recording access */
#define ROBOTV_RECORDINGS_DISKSIZE     100
#define ROBOTV_RECORDINGS_GETCOUNT     101
#define ROBOTV_RECORDINGS_GETLIST      102
#define ROBOTV_RECORDINGS_RENAME       103
#define ROBOTV_RECORDINGS_DELETE       104
#define ROBOTV_RECORDINGS_SETPLAYCOUNT 105
#define ROBOTV_RECORDINGS_SETPOSITION  106
#define ROBOTV_RECORDINGS_GETPOSITION  107
#define ROBOTV_RECORDINGS_GETMARKS     108
#define ROBOTV_RECORDINGS_SETURLS      109

#define ROBOTV_ARTWORK_SET             110
#define ROBOTV_ARTWORK_GET             111

/* OPCODE 120 - 139: RoboTV network functions for epg access and manipulating */
#define ROBOTV_EPG_GETFORCHANNEL     120

/* OPCODE 140 - 159: RoboTV network functions for channel scanning */
#define ROBOTV_SCAN_SUPPORTED        140
#define ROBOTV_SCAN_GETSETUP         141
#define ROBOTV_SCAN_SETSETUP         142
#define ROBOTV_SCAN_START            143
#define ROBOTV_SCAN_STOP             144
#define ROBOTV_SCAN_GETSTATUS        145

/** Stream packet types (server -> client) */
#define ROBOTV_STREAM_CHANGE       1
#define ROBOTV_STREAM_STATUS       2
#define ROBOTV_STREAM_QUEUESTATUS  3
#define ROBOTV_STREAM_MUXPKT       4
#define ROBOTV_STREAM_SIGNALINFO   5
#define ROBOTV_STREAM_DETACH       7

/** Stream status codes */
#define ROBOTV_STREAM_STATUS_SIGNALLOST     111
#define ROBOTV_STREAM_STATUS_SIGNALRESTORED 112

/** Status packet types (server -> client) */
#define ROBOTV_STATUS_TIMERCHANGE      1
#define ROBOTV_STATUS_RECORDING        2
#define ROBOTV_STATUS_MESSAGE          3
#define ROBOTV_STATUS_CHANNELCHANGE    4
#define ROBOTV_STATUS_RECORDINGSCHANGE 5
#define ROBOTV_STATUS_CHANNELSCAN      6
#define ROBOTV_STATUS_CHANNELCHANGED   7

/** Packet return codes */
#define ROBOTV_RET_OK              0
#define ROBOTV_RET_RECRUNNING      1
#define ROBOTV_RET_ENCRYPTED       994
#define ROBOTV_RET_NOTSUPPORTED    995
#define ROBOTV_RET_DATAUNKNOWN     996
#define ROBOTV_RET_DATALOCKED      997
#define ROBOTV_RET_DATAINVALID     998
#define ROBOTV_RET_ERROR           999

#endif // ROBOTV_COMMAND_H
