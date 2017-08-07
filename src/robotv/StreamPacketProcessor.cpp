/*
 *      vdr-plugin-robotv - roboTV server plugin for VDR
 *
 *      Copyright (C) 2017 Alexander Pipelka
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

#include "StreamPacketProcessor.h"
#include "robotvcommand.h"

StreamPacketProcessor::StreamPacketProcessor() : m_demuxers(this) {
    m_requestStreamChange = true;
    m_patVersion = -1;
    m_pmtVersion = -1;
}

StreamBundle StreamPacketProcessor::createFromPatPmt(const cPatPmtParser* patpmt) {
    StreamBundle item;
    int patVersion = 0;
    int pmtVersion = 0;

    if(!patpmt->GetVersions(patVersion, pmtVersion)) {
        return item;
    }

    // add video stream
    int vpid = patpmt->Vpid();
    int vtype = patpmt->Vtype();

    item.addStream(StreamInfo(vpid,
                              vtype == 0x02 ? StreamInfo::Type::MPEG2VIDEO :
                              vtype == 0x1b ? StreamInfo::Type::H264 :
                              vtype == 0x24 ? StreamInfo::Type::H265 :
                              StreamInfo::Type::NONE));

    // add (E)AC3 streams
    for(int i = 0; patpmt->Dpid(i) != 0; i++) {
        int dtype = patpmt->Dtype(i);
        item.addStream(StreamInfo(patpmt->Dpid(i),
                                  dtype == 0x6A ? StreamInfo::Type::AC3 :
                                  dtype == 0x7A ? StreamInfo::Type::EAC3 :
                                  StreamInfo::Type::NONE,
                                  patpmt->Dlang(i)));
    }

    // add audio streams
    for(int i = 0; patpmt->Apid(i) != 0; i++) {
        int atype = patpmt->Atype(i);
        item.addStream(StreamInfo(patpmt->Apid(i),
                                  atype == 0x04 ? StreamInfo::Type::MPEG2AUDIO :
                                  atype == 0x03 ? StreamInfo::Type::MPEG2AUDIO :
                                  atype == 0x0f ? StreamInfo::Type::AAC :
                                  atype == 0x11 ? StreamInfo::Type::LATM :
                                  StreamInfo::Type::NONE,
                                  patpmt->Alang(i)));
    }

    // add subtitle streams
    for(int i = 0; patpmt->Spid(i) != 0; i++) {
        StreamInfo stream(patpmt->Spid(i), StreamInfo::Type::DVBSUB, patpmt->Slang(i));

        stream.setSubtitlingDescriptor(
                patpmt->SubtitlingType(i),
                patpmt->CompositionPageId(i),
                patpmt->AncillaryPageId(i));

        item.addStream(stream);
    }

    return item;
}

bool StreamPacketProcessor::putTsPacket(uint8_t *data, int64_t position) {
    if(m_parser.ParsePatPmt(data, TS_SIZE)) {
        int pmtVersion = 0;
        int patVersion = 0;

        if(m_parser.GetVersions(patVersion, pmtVersion)) {
            if(pmtVersion > m_pmtVersion) {
                isyslog("found new PAT/PMT version (%i/%i)", patVersion, pmtVersion);

                cleanupQueue();
                m_demuxers.clear();

                m_pmtVersion = pmtVersion;
                m_patVersion = patVersion;
                m_requestStreamChange = true;

                // update demuxers from new PMT
                isyslog("updating demuxers");
                StreamBundle streamBundle = createFromPatPmt(&m_parser);
                m_demuxers.updateFrom(&streamBundle);
            }
        }
    }

    // put packets into demuxer
    return m_demuxers.processTsPacket(data, position);
}

void StreamPacketProcessor::cleanupQueue() {
    // cleanup pre-queue
    MsgPacket* p = NULL;

    while(m_preQueue.size() > 0) {
        p = m_preQueue.front();
        m_preQueue.pop_front();
        delete p;
    }
}

void StreamPacketProcessor::reset() {
    // reset parser
    m_parser.Reset();
    m_demuxers.clear();
    m_requestStreamChange = true;
    m_patVersion = -1;
    m_pmtVersion = -1;

    cleanupQueue();
}

void StreamPacketProcessor::onStreamPacket(TsDemuxer::StreamPacket *p) {
    // skip empty packets
    if(p == nullptr || p->size == 0 || p->data == nullptr) {
        return;
    }

    // stream change needed / requested
    if(m_requestStreamChange && m_demuxers.isReady()) {

        isyslog("demuxers ready");

        for(auto i : m_demuxers) {
            isyslog("%s", i->info().c_str());
        }

        isyslog("create streamchange packet");
        m_requestStreamChange = false;

        // push streamchange into queue
        MsgPacket* packet = createStreamChangePacket(m_demuxers);
        onPacket(packet);

        // push pre-queued packets
        dsyslog("processing %lu pre-queued packets", m_preQueue.size());

        while(!m_preQueue.empty()) {
            packet = m_preQueue.front();
            m_preQueue.pop_front();
            onPacket(packet);
        }
    }

    // initialise stream packet
    MsgPacket* packet = new MsgPacket(ROBOTV_STREAM_MUXPKT, ROBOTV_CHANNEL_STREAM);
    packet->disablePayloadCheckSum();

    // write stream data
    packet->put_U16((uint16_t)p->pid);

    packet->put_S64(p->pts);
    packet->put_S64(p->dts);
    packet->put_U32((uint32_t)p->duration);

    // write frame type into unused header field clientid
    packet->setClientID((uint16_t)p->frameType);

    // write payload into stream packet
    packet->put_U32((uint32_t)p->size);
    packet->put_Blob(p->data, (uint32_t)p->size);

    // add timestamp (wallclock time in ms)
    packet->put_S64(getCurrentTime(p));

    // pre-queue packet
    if(!m_demuxers.isReady()) {
        if(m_preQueue.size() > 200) {
            esyslog("pre-queue full - skipping packet");
            delete packet;
            return;
        }

        m_preQueue.push_back(packet);
        return;
    }

    onPacket(packet);
}

void StreamPacketProcessor::onStreamChange() {
    if(!m_requestStreamChange) {
        isyslog("stream change requested");
    }

    m_requestStreamChange = true;
}

MsgPacket *StreamPacketProcessor::createStreamChangePacket(const DemuxerBundle &bundle) {
    MsgPacket* resp = new MsgPacket(ROBOTV_STREAM_CHANGE, ROBOTV_CHANNEL_STREAM);

    resp->put_U8((uint8_t)bundle.size());

    for(auto stream: bundle) {
        int streamId = stream->getPid();
        resp->put_U32((uint32_t)streamId);

        switch(stream->getContent()) {
            case StreamInfo::Content::AUDIO:
                resp->put_String(stream->typeName());
                resp->put_String(stream->getLanguage());
                resp->put_U32(stream->getChannels());
                resp->put_U32(stream->getSampleRate());
                resp->put_U32(0); // UNUSED - BINARY COMPATIBILITY
                resp->put_U32(stream->getBitRate());
                resp->put_U32(0); // UNUSED - BINARY COMPATIBILITY

                break;

            case StreamInfo::Content::VIDEO:
                resp->put_String(stream->typeName());
                resp->put_U32(stream->getFpsScale());
                resp->put_U32(stream->getFpsRate());
                resp->put_U32(stream->getHeight());
                resp->put_U32(stream->getWidth());
                resp->put_S64(stream->getAspect());

                {
                    int length = 0;

                    // put SPS
                    uint8_t* sps = stream->getVideoDecoderSps(length);
                    resp->put_U8((uint8_t)length);

                    if(sps != NULL) {
                        resp->put_Blob(sps, (uint8_t)length);
                    }

                    // put PPS
                    uint8_t* pps = stream->getVideoDecoderPps(length);
                    resp->put_U8((uint8_t)length);

                    if(pps != NULL) {
                        resp->put_Blob(pps, (uint8_t)length);
                    }

                    // put VPS
                    uint8_t* vps = stream->getVideoDecoderVps(length);
                    resp->put_U8((uint8_t)length);

                    if(pps != NULL) {
                        resp->put_Blob(vps, (uint8_t)length);
                    }
                }
                break;

            case StreamInfo::Content::SUBTITLE:
                resp->put_String(stream->typeName());
                resp->put_String(stream->getLanguage());
                resp->put_U32(stream->compositionPageId());
                resp->put_U32(stream->ancillaryPageId());
                break;

            case StreamInfo::Content::TELETEXT:
                resp->put_String(stream->typeName());
                break;

            default:
                break;
        }
    }

    return resp;
}
