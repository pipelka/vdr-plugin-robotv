/*
 *      vdr-plugin-robotv - RoboTV server plugin for VDR
 *
 *      Copyright (C) 2015 Alexander Pipelka
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
#include <map>
#include <vdr/i18n.h>
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

#include "livestreamer.h"
#include "livequeue.h"
#include "channelcache.h"

LiveStreamer::LiveStreamer(RoboTVClient* parent, const cChannel* channel, int priority, bool rawPTS)
    : cThread("LiveStreamer stream processor")
    , cRingBufferLinear(MEGABYTE(10), TS_SIZE, true)
    , cReceiver(NULL, priority)
    , m_demuxers(this)
    , m_scanTimeout(10)
    , m_parent(parent) {
    m_device          = NULL;
    m_queue           = NULL;
    m_startup         = true;
    m_signalLost      = false;
    m_langStreamType  = StreamInfo::stMPEG2AUDIO;
    m_languageIndex   = -1;
    m_uid             = CreateChannelUID(channel);
    m_protocolVersion = ROBOTV_PROTOCOLVERSION;
    m_waitForKeyFrame   = false;
    m_rawPTS          = rawPTS;

    m_requestStreamChange = false;

    if(m_scanTimeout == 0) {
        m_scanTimeout = RoboTVServerConfig::instance().stream_timeout;
    }

    // create send queue
    m_queue = new LiveQueue(m_parent->GetSocket());
    m_queue->Start();

    SetTimeouts(0, 10);
    Start();
}

LiveStreamer::~LiveStreamer() {
    DEBUGLOG("Started to delete live streamer");

    cTimeMs t;

    DEBUGLOG("Stopping streamer thread ...");
    Cancel(5);
    DEBUGLOG("Done.");

    DEBUGLOG("Detaching");

    if(IsAttached()) {
        detach();
    }

    m_demuxers.clear();

    delete m_queue;

    m_uid = 0;
    m_device = NULL;

    DEBUGLOG("Finished to delete live streamer (took %llu ms)", t.Elapsed());
}

void LiveStreamer::setTimeout(uint32_t timeout) {
    m_scanTimeout = timeout;
}

void LiveStreamer::setProtocolVersion(uint32_t protocolVersion) {
    m_protocolVersion = protocolVersion;
}

void LiveStreamer::setWaitForKeyFrame(bool waitforiframe) {
    m_waitForKeyFrame = waitforiframe;
}

void LiveStreamer::requestStreamChange() {
    m_requestStreamChange = true;
}

void LiveStreamer::tryChannelSwitch() {
    std::lock_guard<std::mutex> lock(m_mutex);

    // we're already attached to receiver ?  
    if(IsAttached()) {
        return;
    }

    // find channel from uid
    const cChannel* channel = FindChannelByUID(m_uid);

    // try to switch channel
    int rc = switchChannel(channel);

    // succeeded -> exit
    if(rc == ROBOTV_RET_OK) {
        return;
    }

    // time limit not exceeded -> relax & exit
    if(m_lastTick.Elapsed() < (uint64_t)(m_scanTimeout * 1000)) {
        cCondWait::SleepMs(10);
        return;
    }

    // push notification after timeout
    switch(rc) {
        case ROBOTV_RET_ENCRYPTED:
            ERRORLOG("Unable to decrypt channel %i - %s", channel->Number(), channel->Name());
            m_parent->StatusMessage(tr("Unable to decrypt channel"));
            break;

        case ROBOTV_RET_DATALOCKED:
            ERRORLOG("Can't get device for channel %i - %s", channel->Number(), channel->Name());
            m_parent->StatusMessage(tr("All tuners busy"));
            break;

        case ROBOTV_RET_RECRUNNING:
            ERRORLOG("Active recording blocking channel %i - %s", channel->Number(), channel->Name());
            m_parent->StatusMessage(tr("Blocked by active recording"));
            break;

        case ROBOTV_RET_ERROR:
            ERRORLOG("Error switching to channel %i - %s", channel->Number(), channel->Name());
            m_parent->StatusMessage(tr("Failed to switch"));
            break;
    }

    m_lastTick.Set(0);
}

void LiveStreamer::Action(void) {
    int size = 0;
    unsigned char* buf = NULL;
    m_startup = true;

    // reset timer
    m_lastTick.Set(0);

    INFOLOG("streamer thread started.");

    while(Running()) {
        size = 0;
        buf = Get(size);

        // try to switch channel if we aren't attached yet
<<<<<<< HEAD
        TryChannelSwitch();
=======
        if(!IsAttached()) {
            tryChannelSwitch();
        }
>>>>>>> [transition] XVDR -> RoboTV - Part 4

        if(!isStarting() && (m_lastTick.Elapsed() > (uint64_t)(m_scanTimeout * 1000)) && !m_signalLost) {
            INFOLOG("timeout. signal lost!");
            sendStatus(ROBOTV_STREAM_STATUS_SIGNALLOST);
            m_signalLost = true;
        }

        // not enough data
        if(buf == NULL) {
            continue;
        }

        while(size >= TS_SIZE) {
            if(!Running()) {
                break;
            }

            m_demuxers.processTsPacket(buf);

            buf += TS_SIZE;
            size -= TS_SIZE;
            Del(TS_SIZE);
        }
    }

    INFOLOG("streamer thread ended.");
}

int LiveStreamer::switchChannel(const cChannel* channel) {
    std::lock_guard<std::mutex> lock(m_mutex);

    if(channel == NULL) {
        return ROBOTV_RET_ERROR;
    }

    if(IsAttached()) {
        detach();
    }

    // check if any device is able to decrypt the channel - code taken from VDR
    int NumUsableSlots = 0;

    if(channel->Ca() >= CA_ENCRYPTED_MIN) {
        for(cCamSlot* CamSlot = CamSlots.First(); CamSlot; CamSlot = CamSlots.Next(CamSlot)) {
            if(CamSlot->ModuleStatus() == msReady) {
                if(CamSlot->ProvidesCa(channel->Caids())) {
                    if(!ChannelCamRelations.CamChecked(channel->GetChannelID(), CamSlot->SlotNumber())) {
                        NumUsableSlots++;
                    }
                }
            }
        }

        if(!NumUsableSlots) {
            return ROBOTV_RET_ENCRYPTED;
        }
    }

    // get device for this channel
    m_device = cDevice::GetDevice(channel, LIVEPRIORITY, false);

    if(m_device == NULL) {
        // return status "recording running" if there is an active timer
        time_t now = time(NULL);

        for(cTimer* ti = Timers.First(); ti; ti = Timers.Next(ti)) {
            if(ti->Recording() && ti->Matches(now)) {
                return ROBOTV_RET_RECRUNNING;
            }
        }

        return ROBOTV_RET_DATALOCKED;
    }

    INFOLOG("Found available device %d", m_device->DeviceNumber() + 1);

    if(!m_device->SwitchChannel(channel, false)) {
        ERRORLOG("Can't switch to channel %i - %s", channel->Number(), channel->Name());
        return ROBOTV_RET_ERROR;
    }

    // get cached demuxer data
    ChannelCache& cache = ChannelCache::instance();
    StreamBundle bundle = cache.GetFromCache(m_uid);

    // channel already in cache
    if(bundle.size() != 0) {
        INFOLOG("Channel information found in cache");
    }
    // channel not found in cache -> add it from vdr
    else {
        INFOLOG("adding channel to cache");
        cache.AddToCache(channel);
        bundle = cache.GetFromCache(m_uid);
    }

    // recheck cache item
    StreamBundle currentitem = StreamBundle::FromChannel(channel);

    if(!currentitem.ismetaof(bundle)) {
        INFOLOG("current channel differs from cache item - updating");
        bundle = currentitem;
        cache.AddToCache(m_uid, bundle);
    }

    if(bundle.size() != 0) {
        INFOLOG("Creating demuxers");
        createDemuxers(&bundle);
    }

    requestStreamChange();

    INFOLOG("Successfully switched to channel %i - %s", channel->Number(), channel->Name());

    if(m_waitForKeyFrame) {
        INFOLOG("Will wait for first I-Frame ...");
    }

    // clear cached data
    Clear();
    m_queue->Cleanup();

    m_uid = CreateChannelUID(channel);

    if(!attach()) {
        INFOLOG("Unable to attach receiver !");
        return ROBOTV_RET_DATALOCKED;
    }

    INFOLOG("done switching.");
    return ROBOTV_RET_OK;
}

bool LiveStreamer::attach(void) {
    if(m_device == NULL) {
        return false;
    }

    return m_device->AttachReceiver(this);
}

void LiveStreamer::detach(void) {
    if(m_device) {
        m_device->Detach(this);
    }
}

void LiveStreamer::sendStreamPacket(StreamPacket* pkt) {
    bool bReady = m_demuxers.isReady();

    if(!bReady || pkt == NULL || pkt->size == 0) {
        return;
    }

    // Send stream information as the first packet on startup
    if(isStarting() && bReady) {
        // wait for AV frames (we start with an audio or video packet)
        if(!(pkt->content == StreamInfo::scAUDIO || pkt->content == StreamInfo::scVIDEO)) {
            return;
        }

        INFOLOG("streaming of channel started");
        m_lastTick.Set(0);
        m_requestStreamChange = true;
        m_startup = false;
    }

    // send stream change on demand
    if(m_requestStreamChange) {
        sendStreamChange();
    }

    // wait for first I-Frame (if enabled)
    if(m_waitForKeyFrame && pkt->frametype != StreamInfo::ftIFRAME) {
        return;
    }

    m_waitForKeyFrame = false;

    // if a audio or video packet was sent, the signal is restored
    if(m_signalLost && (pkt->content == StreamInfo::scVIDEO || pkt->content == StreamInfo::scAUDIO)) {
        INFOLOG("signal restored");
        sendStatus(ROBOTV_STREAM_STATUS_SIGNALRESTORED);
        m_signalLost = false;
        m_requestStreamChange = true;
        m_lastTick.Set(0);
        return;
    }

    if(m_signalLost) {
        return;
    }

    // initialise stream packet
    MsgPacket* packet = new MsgPacket(ROBOTV_STREAM_MUXPKT, ROBOTV_CHANNEL_STREAM);
    packet->disablePayloadCheckSum();

    // write stream data
    packet->put_U16(pkt->pid);

    if(m_rawPTS) {
        packet->put_S64(pkt->rawpts);
        packet->put_S64(pkt->rawdts);
    }
    else {
        packet->put_S64(pkt->pts);
        packet->put_S64(pkt->dts);
    }

    if(m_protocolVersion >= 5) {
        packet->put_U32(pkt->duration);
    }

    // write frame type into unused header field clientid
    packet->setClientID((uint16_t)pkt->frametype);

    // write payload into stream packet
    packet->put_U32(pkt->size);
    packet->put_Blob(pkt->data, pkt->size);

    m_queue->Add(packet, pkt->content);
    m_lastTick.Set(0);
}

void LiveStreamer::sendDetach() {
    INFOLOG("sending detach message");
    MsgPacket* resp = new MsgPacket(ROBOTV_STREAM_DETACH, ROBOTV_CHANNEL_STREAM);
    m_parent->QueueMessage(resp);
}

void LiveStreamer::sendStreamChange() {
    DEBUGLOG("sendStreamChange");

    StreamBundle cache;
    INFOLOG("Stored channel information in cache:");

    for(auto i = m_demuxers.begin(); i != m_demuxers.end(); i++) {
        cache.AddStream(*(*i));
        (*i)->info();
    }

    ChannelCache::instance().AddToCache(m_uid, cache);

    // reorder streams as preferred
    m_demuxers.reorderStreams(m_languageIndex, m_langStreamType);

    MsgPacket* resp = m_demuxers.createStreamChangePacket(m_protocolVersion);
    m_queue->Add(resp, StreamInfo::scSTREAMINFO);

    m_requestStreamChange = false;
}

void LiveStreamer::sendStatus(int status) {
    MsgPacket* packet = new MsgPacket(ROBOTV_STREAM_STATUS, ROBOTV_CHANNEL_STREAM);
    packet->put_U32(status);
    m_parent->QueueMessage(packet);
}

void LiveStreamer::requestSignalInfo() {
    if(!Running() || m_device == NULL) {
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

    if(!getTimeShiftMode()) {
        Strength = m_device->SignalStrength();
        Quality = m_device->SignalQuality();
    }

    resp->put_String(*cString::sprintf("%s #%d - %s",
#if VDRVERSNUM < 10728
#warning "VDR versions < 1.7.28 do not support all features"
                                       "Unknown",
                                       DeviceNumber,
                                       "Unknown"));
#else
                                       (const char*)m_device->DeviceType(),
                                       DeviceNumber,
                                       (const char*)m_device->DeviceName()));
#endif

    // Quality:
    // 4 - NO LOCK
    // 3 - NO SYNC
    // 2 - NO VITERBI
    // 1 - NO CARRIER
    // 0 - NO SIGNAL

    if(getTimeShiftMode()) {
        resp->put_String("TIMESHIFT");
    }
    else if(Quality == -1) {
        resp->put_String("UNKNOWN (Incompatible device)");
        Quality = 0;
    }
    else
        resp->put_String(*cString::sprintf("%s:%s:%s:%s:%s",
                                           (Quality > 4) ? "LOCKED" : "-",
                                           (Quality > 0) ? "SIGNAL" : "-",
                                           (Quality > 1) ? "CARRIER" : "-",
                                           (Quality > 2) ? "VITERBI" : "-",
                                           (Quality > 3) ? "SYNC" : "-"));

    resp->put_U32((Strength << 16) / 100);
    resp->put_U32((Quality << 16) / 100);
    resp->put_U32(0);
    resp->put_U32(0);

    // get provider & service information
    const cChannel* channel = FindChannelByUID(m_uid);

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
    m_queue->Add(resp, StreamInfo::scNONE);
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

    return m_queue->IsPaused();
}

bool LiveStreamer::getTimeShiftMode() {
    if(m_queue == NULL) {
        return false;
    }

    return m_queue->TimeShiftMode();
}

void LiveStreamer::pause(bool on) {
    if(m_queue == NULL) {
        return;
    }

    m_queue->Pause(on);
}

void LiveStreamer::requestPacket() {
    if(m_queue == NULL) {
        return;
    }

    m_queue->Request();
}

#if VDRVERSNUM < 20300
void LiveStreamer::Receive(uchar* Data, int Length)
#else
void LiveStreamer::Receive(const uchar* Data, int Length)
#endif
{
    int p = Put(Data, Length);

    if(p != Length) {
        ReportOverflow(Length - p);
    }
}

<<<<<<< HEAD
void cLiveStreamer::ChannelChange(const cChannel* channel) {
    std::lock_guard<std::mutex> lock(m_mutex);

=======
void LiveStreamer::channelChange(const cChannel* channel) {
>>>>>>> [transition] XVDR -> RoboTV - Part 4
    if(CreateChannelUID(channel) != m_uid || !Running()) {
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
        dmx->info();
        AddPid(dmx->GetPID());
    }
}
