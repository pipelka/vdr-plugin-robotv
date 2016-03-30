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

#include <stdlib.h>
#include <sys/ioctl.h>
#include <time.h>
#include <string.h>
#include <vdr/remux.h>
#include <vdr/channels.h>
#include <vdr/timers.h>

#ifdef __FreeBSD__
#include <sys/endian.h>
#else
#include <endian.h>
#endif

#include "config/config.h"
#include "net/msgpacket.h"
#include "robotv/robotvcommand.h"
#include "robotv/robotvclient.h"
#include "tools/hash.h"
#include "tools/time.h"

#include "livestreamer.h"
#include "livequeue.h"
#include "channelcache.h"

#include <chrono>

#define MIN_PACKET_SIZE (128 * 1024)

using namespace std::chrono;

LiveStreamer::LiveStreamer(RoboTvClient* parent, const cChannel* channel, int priority)
    : cReceiver(NULL, priority)
    , m_demuxers(this)
    , m_parent(parent) {
    m_uid = createChannelUid(channel);

    // create send queue
    m_queue = new LiveQueue(m_parent->getSocket());
}

LiveStreamer::~LiveStreamer() {
    // wait for pending channelchange
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        detach();
        m_demuxers.clear();
        delete m_queue;
    }

    std::this_thread::yield();

    m_uid = 0;
    m_device = NULL;
    delete m_streamPacket;

    INFOLOG("live streamer terminated");
}

void LiveStreamer::setWaitForKeyFrame(bool waitforiframe) {
    m_waitForKeyFrame = waitforiframe;
}

void LiveStreamer::requestStreamChange() {
    m_requestStreamChange = true;
}

int LiveStreamer::switchChannel(const cChannel* channel) {

    if(channel == NULL) {
        ERRORLOG("unknown channel !");
        return ROBOTV_RET_ERROR;
    }

    detach();

    // get device for this channel
    m_device = cDevice::GetDevice(channel, LIVEPRIORITY, false);

    if(m_device == NULL) {
        // return status "recording running" if there is an active timer
        time_t now = time(NULL);

        for(cTimer* ti = Timers.First(); ti; ti = Timers.Next(ti)) {
            if(ti->Recording() && ti->Matches(now)) {
                ERRORLOG("Recording running !");
                return ROBOTV_RET_RECRUNNING;
            }
        }

        ERRORLOG("No device available !");
        return ROBOTV_RET_DATALOCKED;
    }

    INFOLOG("Found available device %d", m_device->DeviceNumber() + 1);

    if(!m_device->SwitchChannel(channel, false)) {
        ERRORLOG("Can't switch to channel %i - %s", channel->Number(), channel->Name());
        return ROBOTV_RET_ERROR;
    }

    // get cached demuxer data
    ChannelCache& cache = ChannelCache::instance();
    StreamBundle bundle = cache.lookup(m_uid);

    // channel already in cache
    if(bundle.size() != 0) {
        INFOLOG("Channel information found in cache");
    }
    // channel not found in cache -> add it from vdr
    else {
        INFOLOG("adding channel to cache");
        bundle = cache.add(channel);
    }

    // recheck cache item
    StreamBundle currentitem = StreamBundle::createFromChannel(channel);

    if(!currentitem.isMetaOf(bundle)) {
        INFOLOG("current channel differs from cache item - updating");
        bundle = currentitem;
        cache.add(m_uid, bundle);
    }

    if(bundle.size() != 0) {
        INFOLOG("Creating demuxers");
        createDemuxers(&bundle);
    }

    requestStreamChange();

    INFOLOG("Successfully switched to channel %i - %s", channel->Number(), channel->Name());

    if(m_waitForKeyFrame) {
        INFOLOG("Will wait for first key frame ...");
    }

    m_uid = createChannelUid(channel);

    if(!attach()) {
        return ROBOTV_RET_DATALOCKED;
    }

    INFOLOG("done switching.");
    return ROBOTV_RET_OK;
}

bool LiveStreamer::attach(void) {
    if(m_device == NULL) {
        return false;
    }

    if(IsAttached()) {
        return true;
    }

    if(m_device->AttachReceiver(this)) {
        INFOLOG("device attached to receiver");
        return true;
    }

    ERRORLOG("failed to attach receiver !");
    return false;
}

void LiveStreamer::detach(void) {
    if(m_device == NULL) {
        return;
    }

    if(!IsAttached()) {
        return;
    }

    m_device->Detach(this);
    INFOLOG("device detached");
}

void LiveStreamer::sendStreamPacket(StreamPacket* pkt) {
    // skip empty packets
    if(pkt == NULL || pkt->size == 0) {
        return;
    }

    // skip non audio / video packets
    if(!(pkt->content == StreamInfo::scAUDIO || pkt->content == StreamInfo::scVIDEO)) {
        return;
    }

    // send stream change on demand
    if(m_requestStreamChange && m_demuxers.isReady()) {
        sendStreamChange();
    }

    // wait for first I-Frame (if enabled)
    if(m_waitForKeyFrame && pkt->frameType != StreamInfo::ftIFRAME) {
        return;
    }

    m_waitForKeyFrame = false;

    // initialise stream packet
    MsgPacket* packet = new MsgPacket(ROBOTV_STREAM_MUXPKT, ROBOTV_CHANNEL_STREAM);
    packet->disablePayloadCheckSum();

    // write stream data
    packet->put_U16(pkt->pid);
    packet->put_S64(pkt->rawPts);
    packet->put_S64(pkt->rawDts);
    packet->put_U32(pkt->duration);

    // write frame type into unused header field clientid
    packet->setClientID((uint16_t)pkt->frameType);

    // write payload into stream packet
    packet->put_U32(pkt->size);
    packet->put_Blob(pkt->data, pkt->size);

    // add timestamp (wallclock time in ms)
    packet->put_S64(roboTV::currentTimeMillis().count());

    m_queue->queue(packet, pkt->content, pkt->rawPts);
}

void LiveStreamer::sendDetach() {
    INFOLOG("sending detach message");
    MsgPacket* resp = new MsgPacket(ROBOTV_STREAM_DETACH, ROBOTV_CHANNEL_STREAM);
    m_parent->queueMessage(resp);
}

void LiveStreamer::sendStreamChange() {
    INFOLOG("stream change notification");

    StreamBundle cache;

    for(auto i = m_demuxers.begin(); i != m_demuxers.end(); i++) {
        cache.addStream(*(*i));
    }

    ChannelCache::instance().add(m_uid, cache);

    // reorder streams as preferred
    m_demuxers.reorderStreams(m_languageIndex, m_langStreamType);

    MsgPacket* resp = m_demuxers.createStreamChangePacket();
    m_queue->queue(resp, StreamInfo::scSTREAMINFO);

    m_requestStreamChange = false;
}

void LiveStreamer::sendStatus(int status) {
    MsgPacket* packet = new MsgPacket(ROBOTV_STREAM_STATUS, ROBOTV_CHANNEL_STREAM);
    packet->put_U32(status);
    m_parent->queueMessage(packet);
}

void LiveStreamer::requestSignalInfo() {
    if(m_device == NULL || !IsAttached()) {
        return;
    }

    // do not send (and pollute the client with) signal information
    // if we are paused
    if(isPaused()) {
        return;
    }

    MsgPacket* resp = new MsgPacket(ROBOTV_STREAM_SIGNALINFO, ROBOTV_CHANNEL_STREAM);

    int DeviceNumber = m_device->DeviceNumber() + 1;
    int Strength = 0;
    int Quality = 0;

    Strength = m_device->SignalStrength();
    Quality = m_device->SignalQuality();

    resp->put_String(*cString::sprintf(
                         "%s #%d - %s",
                         (const char*)m_device->DeviceType(),
                         DeviceNumber,
                         (const char*)m_device->DeviceName()));

    // Quality:
    // 4 - NO LOCK
    // 3 - NO SYNC
    // 2 - NO VITERBI
    // 1 - NO CARRIER
    // 0 - NO SIGNAL

    if(Quality == -1) {
        resp->put_String("UNKNOWN (Incompatible device)");
        Quality = 0;
    }
    else {
        resp->put_String(*cString::sprintf("%s:%s:%s:%s:%s",
                                           (Quality > 4) ? "LOCKED" : "-",
                                           (Quality > 0) ? "SIGNAL" : "-",
                                           (Quality > 1) ? "CARRIER" : "-",
                                           (Quality > 2) ? "VITERBI" : "-",
                                           (Quality > 3) ? "SYNC" : "-"));
    }

    resp->put_U32((Strength << 16) / 100);
    resp->put_U32((Quality << 16) / 100);
    resp->put_U32(0);
    resp->put_U32(0);

    // get provider & service information
    const cChannel* channel = findChannelByUid(m_uid);

    if(channel != NULL) {
        // put in provider name
        resp->put_String(channel->Provider());

        // what the heck should be the service name ?
        // using PortalName for now
        resp->put_String(channel->PortalName());
    }
    else {
        resp->put_String("");
        resp->put_String("");
    }

    DEBUGLOG("RequestSignalInfo");
    m_queue->queue(resp, StreamInfo::scNONE);
}

void LiveStreamer::setLanguage(int lang, StreamInfo::Type streamtype) {
    if(lang == -1) {
        return;
    }

    m_languageIndex = lang;
    m_langStreamType = streamtype;
}

bool LiveStreamer::isPaused() {
    if(m_queue == NULL) {
        return false;
    }

    return m_queue->isPaused();
}

void LiveStreamer::pause(bool on) {
    if(m_queue == NULL) {
        return;
    }

    m_queue->pause(on);
}

MsgPacket* LiveStreamer::requestPacket(bool keyFrameMode) {
    std::lock_guard<std::mutex> lock(m_mutex);

    auto sendStartPosition = [&]() {
        MsgPacket* resp = new MsgPacket(ROBOTV_STREAM_POSITIONS, ROBOTV_CHANNEL_STREAM);
        resp->put_S64(m_queue->getTimeshiftStartPosition());
        m_parent->queueMessage(resp);
    };

    // create payload packet
    if(m_streamPacket == NULL) {
        m_streamPacket = new MsgPacket();
        m_streamPacket->disablePayloadCheckSum();
    }

    // request packet from queue
    MsgPacket* p = NULL;

    while(p = m_queue->read(keyFrameMode)) {

        // send timeshift start position on every keyframe
        if(p->getClientID() == StreamInfo::FrameType::ftIFRAME) {
            sendStartPosition();
        }

        // add data
        m_streamPacket->put_U16(p->getMsgID());
        m_streamPacket->put_U16(p->getClientID());

        // add payload
        uint8_t* data = p->getPayload();
        int length = p->getPayloadLength();
        m_streamPacket->put_Blob(data, length);

        delete p;

        // send payload packet if it's big enough
        if(m_streamPacket->getPayloadLength() >= MIN_PACKET_SIZE) {
            MsgPacket* result = m_streamPacket;
            m_streamPacket = NULL;

            return result;
        }
    }

    if(m_queue->isPaused()) {
        sendStartPosition();
    }

    return NULL;
}

#if VDRVERSNUM < 20300
void LiveStreamer::Receive(uchar* Data, int Length)
#else
void LiveStreamer::Receive(const uchar* Data, int Length)
#endif
{
    m_demuxers.processTsPacket(Data);
}

void LiveStreamer::processChannelChange(const cChannel* channel) {
    std::lock_guard<std::mutex> lock(m_mutex);

    if(!IsAttached()) {
        return;
    }

    if(createChannelUid(channel) != m_uid) {
        return;
    }

    INFOLOG("ChannelChange()");
    switchChannel(channel);
}

void LiveStreamer::createDemuxers(StreamBundle* bundle) {
    // update demuxers
    m_demuxers.updateFrom(bundle);

    // update pids
    SetPids(NULL);

    for(auto i = m_demuxers.begin(); i != m_demuxers.end(); i++) {
        TsDemuxer* dmx = *i;
        AddPid(dmx->getPid());
    }
}

int64_t LiveStreamer::seek(int64_t wallclockPositionMs) {
    return m_queue->seek(wallclockPositionMs);
}
