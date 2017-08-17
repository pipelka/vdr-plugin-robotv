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
#include <vdr/remux.h>
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
#include <robotv/StreamPacketProcessor.h>

#define MIN_PACKET_SIZE (128 * 1024)

using namespace std::chrono;

LiveStreamer::LiveStreamer(RoboTvClient* parent, int priority)
    : cReceiver(nullptr, priority)
    , m_parent(parent)
    , m_uid(0) {
    // create send queue
    m_queue = new LiveQueue(m_parent->getSocket());
}

LiveStreamer::~LiveStreamer() {
    cDevice * device = Device();

    if(device != nullptr) {
        cCamSlot *camSlot = device->CamSlot();

        if (camSlot != nullptr) {
            isyslog("camslot detached");
            ChannelCamRelations.ClrChecked(ChannelID(), camSlot->SlotNumber());
        }

        Detach();
    }

    reset();
    delete m_queue;
    delete m_streamPacket;

    isyslog("live streamer terminated");
}

int LiveStreamer::switchChannel(const cChannel* channel) {
    if(channel == nullptr) {
        esyslog("unknown channel !");
        return ROBOTV_RET_ERROR;
    }

    // get device for this channel
    cDevice* device = cDevice::GetDevice(channel, LIVEPRIORITY, false);

    // maybe an encrypted channel that cannot be handled
    // lets try if a device can decrypt it on it's own (without a CAM slot)
    if(device == nullptr) {
        device = cDevice::GetDeviceForTransponder(channel, LIVEPRIORITY);
    }

    // maybe all devices busy
    if(device == nullptr) {
        // return status "recording running" if there is an active timer
        time_t now = time(nullptr);

        for(cTimer* ti = Timers.First(); ti; ti = Timers.Next(ti)) {
            if(ti->Recording() && ti->Matches(now)) {
                esyslog("Recording running !");
                return ROBOTV_RET_RECRUNNING;
            }
        }

        esyslog("No device available !");
        return ROBOTV_RET_DATALOCKED;
    }

    isyslog("Found available device %d", device->DeviceNumber() + 1);

    if(!device->SwitchChannel(channel, false)) {
        esyslog("Can't switch to channel %i - %s", channel->Number(), channel->Name());
        return ROBOTV_RET_ERROR;
    }

    m_uid = createChannelUid(channel);

    StreamBundle currentItem = createFromChannel(channel);

    // get cached demuxer data
    ChannelCache &cache = ChannelCache::instance();
    StreamBundle cacheItem = cache.lookup(m_uid);

    // channel already in cache
    if (!cacheItem.empty()) {
        isyslog("Channel information found in cache");
    }
    // channel not found in cache -> add it from vdr
    else {
        isyslog("adding channel to cache");
        cacheItem = currentItem;
        cache.add(m_uid, cacheItem);
    }

    // recheck cache item
    if (!currentItem.isMetaOf(cacheItem)) {
        isyslog("current channel differs from cache item - updating");
        cacheItem = currentItem;
        cache.add(m_uid, cacheItem);
    }

    if(cacheItem.empty()) {
        esyslog("channel %i - %s doesn't have any stream information", channel->Number(), channel->Name());
        return false;
    }

    isyslog("Creating demuxers");
    createDemuxers(&cacheItem);

    onStreamChange();

    isyslog("Successfully switched to channel %i - %s", channel->Number(), channel->Name());

    // fool device to not start the decryption timer
    int priority = Priority();
    SetPriority(MINPRIORITY);

    /// attach receiver
    if (device->AttachReceiver(this) == false) {
        esyslog("failed to attach receiver !");
        return false;
    }

    // start decrypting manually
    cCamSlot* slot = device->CamSlot();

    if(slot) {
        slot->StartDecrypting();
    }

    SetPriority(priority);

    isyslog("done switching.");
    return ROBOTV_RET_OK;
}

MsgPacket *LiveStreamer::createStreamChangePacket(DemuxerBundle &bundle) {
    StreamBundle cache;

    for(auto i = bundle.begin(); i != bundle.end(); i++) {
        cache.addStream(*(*i));
    }

    ChannelCache::instance().add(m_uid, cache);

    // reorder streams as preferred
    bundle.reorderStreams(m_language.c_str(), m_langStreamType);

    return StreamPacketProcessor::createStreamChangePacket(bundle);
}

void LiveStreamer::sendStatus(int status) {
    MsgPacket* packet = new MsgPacket(ROBOTV_STREAM_STATUS, ROBOTV_CHANNEL_STREAM);
    packet->put_U32(status);
    m_parent->queueMessage(packet);
}

void LiveStreamer::requestSignalInfo() {
    cDevice* device = Device();

    if(device == nullptr || !IsAttached()) {
        return;
    }

    // do not send (and pollute the client with) signal information
    // if we are paused
    if(isPaused()) {
        return;
    }

    MsgPacket* resp = new MsgPacket(ROBOTV_STREAM_SIGNALINFO, ROBOTV_CHANNEL_STREAM);

    int DeviceNumber = device->DeviceNumber() + 1;
    int Strength = 0;
    int Quality = 0;

    Strength = device->SignalStrength();
    Quality = device->SignalQuality();

    resp->put_String(*cString::sprintf(
                         "%s #%d - %s",
                         (const char*)device->DeviceType(),
                         DeviceNumber,
                         (const char*)device->DeviceName()));

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

    if(channel != nullptr) {
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

    dsyslog("RequestSignalInfo");
    m_queue->queue(resp, StreamInfo::Content::NONE);
}

void LiveStreamer::setLanguage(const char* lang, StreamInfo::Type streamtype) {
    if(lang == nullptr) {
        return;
    }

    m_language = lang;
    m_langStreamType = streamtype;
}

bool LiveStreamer::isPaused() {
    if(m_queue == nullptr) {
        return false;
    }

    return m_queue->isPaused();
}

void LiveStreamer::pause(bool on) {
    if(m_queue == nullptr) {
        return;
    }

    m_queue->pause(on);
}

MsgPacket* LiveStreamer::requestPacket() {
    std::lock_guard<std::mutex> lock(m_mutex);

    // create payload packet
    if(m_streamPacket == nullptr) {
        m_streamPacket = new MsgPacket();
        m_streamPacket->put_S64(m_queue->getTimeshiftStartPosition());
        m_streamPacket->put_S64(roboTV::currentTimeMillis().count());
        m_streamPacket->disablePayloadCheckSum();
    }

    // request packet from queue
    MsgPacket* p = nullptr;

    while((p = m_queue->read()) != nullptr) {

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
            m_streamPacket = nullptr;
            return result;
        }
    }

    if(m_queue->isPaused()) {
        MsgPacket* result = m_streamPacket;
        m_streamPacket = nullptr;
        return result;
    }

    return nullptr;
}

#if VDRVERSNUM < 20300
void LiveStreamer::Receive(uchar* packet, int length)
#else
void LiveStreamer::Receive(const uchar* packet, int length)
#endif
{
    putTsPacket(packet, roboTV::currentTimeMillis().count());
}

void LiveStreamer::processChannelChange(const cChannel* channel) {
    if(createChannelUid(channel) != m_uid) {
        return;
    }

    isyslog("ChannelChange()");

    Detach();
    flush();
    switchChannel(channel);
}

void LiveStreamer::createDemuxers(StreamBundle* bundle) {
    DemuxerBundle& demuxers = getDemuxers();

    // update demuxers
    demuxers.updateFrom(bundle);

    // update pids
    SetPids(nullptr);

    for(auto i = demuxers.begin(); i != demuxers.end(); i++) {
        TsDemuxer* dmx = *i;
        AddPid(dmx->getPid());
    }
}

int64_t LiveStreamer::seek(int64_t wallclockPositionMs) {
    std::lock_guard<std::mutex> lock(m_mutex);

    // remove pending packet
    delete m_streamPacket;
    m_streamPacket = nullptr;

    // seek
    return m_queue->seek(wallclockPositionMs);
}

StreamBundle LiveStreamer::createFromChannel(const cChannel* channel) {
    StreamBundle item;

    // add video stream
    int vpid = channel->Vpid();
    int vtype = channel->Vtype();

    item.addStream(StreamInfo(vpid,
                              vtype == 0x02 ? StreamInfo::Type::MPEG2VIDEO :
                              vtype == 0x1b ? StreamInfo::Type::H264 :
                              vtype == 0x24 ? StreamInfo::Type::H265 :
                              StreamInfo::Type::NONE));

    // add (E)AC3 streams
    for(int i = 0; channel->Dpid(i) != 0; i++) {
        int dtype = channel->Dtype(i);
        item.addStream(StreamInfo(channel->Dpid(i),
                                  dtype == 0x6A ? StreamInfo::Type::AC3 :
                                  dtype == 0x7A ? StreamInfo::Type::EAC3 :
                                  StreamInfo::Type::NONE,
                                  channel->Dlang(i)));
    }

    // add audio streams
    for(int i = 0; channel->Apid(i) != 0; i++) {
        int atype = channel->Atype(i);
        item.addStream(StreamInfo(channel->Apid(i),
                                  atype == 0x04 ? StreamInfo::Type::MPEG2AUDIO :
                                  atype == 0x03 ? StreamInfo::Type::MPEG2AUDIO :
                                  atype == 0x0f ? StreamInfo::Type::AAC :
                                  atype == 0x11 ? StreamInfo::Type::LATM :
                                  StreamInfo::Type::NONE,
                                  channel->Alang(i)));
    }

    // add subtitle streams
    for(int i = 0; channel->Spid(i) != 0; i++) {
        StreamInfo stream(channel->Spid(i), StreamInfo::Type::DVBSUB, channel->Slang(i));

        stream.setSubtitlingDescriptor(
                channel->SubtitlingType(i),
                channel->CompositionPageId(i),
                channel->AncillaryPageId(i));

        item.addStream(stream);
    }

    return item;
}

int64_t LiveStreamer::getCurrentTime(TsDemuxer::StreamPacket *p) {
    return p->streamPosition;
}

void LiveStreamer::onPacket(MsgPacket *p, StreamInfo::Content content, int64_t pts) {
    m_queue->queue(p, content, pts);
}
