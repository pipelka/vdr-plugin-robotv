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

#include "config/config.h"
#include "demuxerbundle.h"

DemuxerBundle::DemuxerBundle(TsDemuxer::Listener* listener) : m_listener(listener) {
}

DemuxerBundle::~DemuxerBundle() {
    clear();
}


void DemuxerBundle::clear() {
    for(auto i = begin(); i != end(); i++) {
        if((*i) != NULL) {
            DEBUGLOG("Deleting stream demuxer for pid=%i and type=%i", (*i)->getPid(), (*i)->getType());
            delete(*i);
        }
    }

    std::list<TsDemuxer*>::clear();
}

TsDemuxer* DemuxerBundle::findDemuxer(int Pid) const {
    for(auto i = begin(); i != end(); i++) {
        if((*i) != NULL && (*i)->getPid() == Pid) {
            return (*i);
        }
    }

    return NULL;
}

void DemuxerBundle::reorderStreams(int lang, StreamInfo::Type type) {
    std::map<uint32_t, TsDemuxer*> weight;

    // compute weights
    int i = 0;

    for(auto idx = begin(); idx != end(); idx++, i++) {
        TsDemuxer* stream = (*idx);

        if(stream == NULL) {
            continue;
        }

        // 32bit weight:
        // V0000000ASLTXXXXPPPPPPPPPPPPPPPP
        //
        // VIDEO (V):      0x80000000
        // AUDIO (A):      0x00800000
        // SUBTITLE (S):   0x00400000
        // LANGUAGE (L):   0x00200000
        // STREAMTYPE (T): 0x00100000 (only audio)
        // AUDIOTYPE (X):  0x000F0000 (only audio)
        // PID (P):        0x0000FFFF

#define VIDEO_MASK      0x80000000
#define AUDIO_MASK      0x00800000
#define SUBTITLE_MASK   0x00400000
#define LANGUAGE_MASK   0x00200000
#define STREAMTYPE_MASK 0x00100000
#define AUDIOTYPE_MASK  0x000F0000
#define PID_MASK        0x0000FFFF

        // last resort ordering, the PID
        uint32_t w = 0xFFFF - (stream->getPid() & PID_MASK);

        // stream type weights
        switch(stream->getContent()) {
            case StreamInfo::scVIDEO:
                w |= VIDEO_MASK;
                break;

            case StreamInfo::scAUDIO:
                w |= AUDIO_MASK;

                // weight of audio stream type
                w |= (stream->getType() == type) ? STREAMTYPE_MASK : 0;

                // weight of audio type
                w |= ((4 - stream->getAudioType()) << 16) & AUDIOTYPE_MASK;
                break;

            case StreamInfo::scSUBTITLE:
                w |= SUBTITLE_MASK;
                break;

            default:
                break;
        }

        // weight of language
        int streamLangIndex = I18nLanguageIndex(stream->getLanguage());
        w |= (streamLangIndex == lang) ? LANGUAGE_MASK : 0;

        // summed weight
        weight[w] = stream;
    }

    // reorder streams on weight
    int idx = 0;
    std::list<TsDemuxer*>::clear();

    for(std::map<uint32_t, TsDemuxer*>::reverse_iterator i = weight.rbegin(); i != weight.rend(); i++, idx++) {
        TsDemuxer* stream = i->second;
        DEBUGLOG("Stream : Type %s / %s Weight: %08X", stream->typeName(), stream->getLanguage(), i->first);
        push_back(stream);
    }
}

bool DemuxerBundle::isReady() const {
    for(auto i = begin(); i != end(); i++) {
        if(!(*i)->isParsed()) {
            DEBUGLOG("Stream with PID %i not parsed", (*i)->getPid());
            return false;
        }
    }

    return true;
}

void DemuxerBundle::updateFrom(StreamBundle* bundle) {
    StreamBundle old;

    // remove old demuxers
    for(auto i = begin(); i != end(); i++) {
        old.addStream(*(*i));
        delete *i;
    }

    std::list<TsDemuxer*>::clear();

    // create new stream demuxers
    for(auto i = bundle->begin(); i != bundle->end(); i++) {
        StreamInfo& infonew = i->second;
        StreamInfo& infoold = old[i->first];

        // reuse previous stream information
        if(infonew.getPid() == infoold.getPid() && infonew.getType() == infoold.getType()) {
            infonew = infoold;
        }

        TsDemuxer* dmx = new TsDemuxer(m_listener, infonew);

        if(dmx != NULL) {
            push_back(dmx);
            dmx->info();
        }
    }
}

bool DemuxerBundle::processTsPacket(uint8_t* packet) const {
    unsigned int ts_pid = TsPid(packet);
    TsDemuxer* demuxer = findDemuxer(ts_pid);

    if(demuxer == NULL) {
        return false;
    }

    return demuxer->processTsPacket(packet);
}

MsgPacket* DemuxerBundle::createStreamChangePacket(int protocolVersion) {
    MsgPacket* resp = new MsgPacket(ROBOTV_STREAM_CHANGE, ROBOTV_CHANNEL_STREAM);

    resp->put_U8(this->size());

    for(auto idx = begin(); idx != end(); idx++) {
        TsDemuxer* stream = (*idx);

        int streamid = stream->getPid();
        resp->put_U32(streamid);

        switch(stream->getContent()) {
            case StreamInfo::scAUDIO:
                resp->put_String(stream->typeName());
                resp->put_String(stream->getLanguage());
                resp->put_U32(stream->getChannels());
                resp->put_U32(stream->getSampleRate());
                resp->put_U32(stream->getBlockAlign());
                resp->put_U32(stream->getBitRate());
                resp->put_U32(stream->getBitsPerSample());

                break;

            case StreamInfo::scVIDEO:
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
                    resp->put_U8(length);

                    if(sps != NULL) {
                        resp->put_Blob(sps, length);
                    }

                    // put PPS
                    uint8_t* pps = stream->getVideoDecoderPps(length);
                    resp->put_U8(length);

                    if(pps != NULL) {
                        resp->put_Blob(pps, length);
                    }

                    // put VPS
                    uint8_t* vps = stream->getVideoDecoderVps(length);
                    resp->put_U8(length);

                    if(pps != NULL) {
                        resp->put_Blob(vps, length);
                    }
                }
                break;

            case StreamInfo::scSUBTITLE:
                resp->put_String(stream->typeName());
                resp->put_String(stream->getLanguage());
                resp->put_U32(stream->compositionPageId());
                resp->put_U32(stream->ancillaryPageId());
                break;

            case StreamInfo::scTELETEXT:
                resp->put_String(stream->typeName());
                break;

            default:
                break;
        }
    }

    return resp;
}

