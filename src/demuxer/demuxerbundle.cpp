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

#include "config/config.h"
#include "demuxerbundle.h"

#include <map>
#include <vdr/remux.h>

DemuxerBundle::DemuxerBundle(TsDemuxer::Listener* listener) : m_listener(listener) {
}

DemuxerBundle::~DemuxerBundle() {
    clear();
}


void DemuxerBundle::clear() {
    for(auto i = begin(); i != end(); i++) {
        if((*i) != NULL) {
            DEBUGLOG("Deleting stream demuxer for pid=%i and type=%i", (*i)->GetPID(), (*i)->GetType());
            delete(*i);
        }
    }

    std::list<TsDemuxer*>::clear();
}

TsDemuxer* DemuxerBundle::findDemuxer(int Pid) {
    for(auto i = begin(); i != end(); i++) {
        if((*i) != NULL && (*i)->GetPID() == Pid) {
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
        uint32_t w = 0xFFFF - (stream->GetPID() & PID_MASK);

        // stream type weights
        switch(stream->GetContent()) {
            case StreamInfo::scVIDEO:
                w |= VIDEO_MASK;
                break;

            case StreamInfo::scAUDIO:
                w |= AUDIO_MASK;

                // weight of audio stream type
                w |= (stream->GetType() == type) ? STREAMTYPE_MASK : 0;

                // weight of audio type
                w |= ((4 - stream->GetAudioType()) << 16) & AUDIOTYPE_MASK;
                break;

            case StreamInfo::scSUBTITLE:
                w |= SUBTITLE_MASK;
                break;

            default:
                break;
        }

        // weight of language
        int streamLangIndex = I18nLanguageIndex(stream->GetLanguage());
        w |= (streamLangIndex == lang) ? LANGUAGE_MASK : 0;

        // summed weight
        weight[w] = stream;
    }

    // reorder streams on weight
    int idx = 0;
    std::list<TsDemuxer*>::clear();

    for(std::map<uint32_t, TsDemuxer*>::reverse_iterator i = weight.rbegin(); i != weight.rend(); i++, idx++) {
        TsDemuxer* stream = i->second;
        DEBUGLOG("Stream : Type %s / %s Weight: %08X", stream->TypeName(), stream->GetLanguage(), i->first);
        push_back(stream);
    }
}

bool DemuxerBundle::isReady() {
    for(auto i = begin(); i != end(); i++) {
        if(!(*i)->IsParsed()) {
            DEBUGLOG("Stream with PID %i not parsed", (*i)->GetPID());
            return false;
        }
    }

    return true;
}

void DemuxerBundle::updateFrom(StreamBundle* bundle) {
    StreamBundle old;

    // remove old demuxers
    for(auto i = begin(); i != end(); i++) {
        old.AddStream(*(*i));
        delete *i;
    }

    std::list<TsDemuxer*>::clear();

    // create new stream demuxers
    for(auto i = bundle->begin(); i != bundle->end(); i++) {
        StreamInfo& infonew = i->second;
        StreamInfo& infoold = old[i->first];

        // reuse previous stream information
        if(infonew.GetPID() == infoold.GetPID() && infonew.GetType() == infoold.GetType()) {
            infonew = infoold;
        }

        TsDemuxer* dmx = new TsDemuxer(m_listener, infonew);

        if(dmx != NULL) {
            push_back(dmx);
            dmx->info();
        }
    }
}

bool DemuxerBundle::processTsPacket(uint8_t* packet) {
    unsigned int ts_pid = TsPid(packet);
    TsDemuxer* demuxer = findDemuxer(ts_pid);

    if(demuxer == NULL) {
        return false;
    }

    return demuxer->ProcessTSPacket(packet);
}

MsgPacket* DemuxerBundle::createStreamChangePacket(int protocolVersion) {
    MsgPacket* resp = new MsgPacket(ROBOTV_STREAM_CHANGE, ROBOTV_CHANNEL_STREAM);

    for(auto idx = begin(); idx != end(); idx++) {
        TsDemuxer* stream = (*idx);

        if(stream == NULL) {
            continue;
        }

        int streamid = stream->GetPID();
        resp->put_U32(streamid);

        switch(stream->GetContent()) {
            case StreamInfo::scAUDIO:
                resp->put_String(stream->TypeName());
                resp->put_String(stream->GetLanguage());

                if(protocolVersion >= 5) {
                    resp->put_U32(stream->GetChannels());
                    resp->put_U32(stream->GetSampleRate());
                    resp->put_U32(stream->GetBlockAlign());
                    resp->put_U32(stream->GetBitRate());
                    resp->put_U32(stream->GetBitsPerSample());
                }

                break;

            case StreamInfo::scVIDEO:
                // H265 is supported on protocol version 6 or higher, ...
                resp->put_String(stream->TypeName());
                resp->put_U32(stream->GetFpsScale());
                resp->put_U32(stream->GetFpsRate());
                resp->put_U32(stream->GetHeight());
                resp->put_U32(stream->GetWidth());
                resp->put_S64(stream->GetAspect() * 10000.0);

                // send decoder specific data SPS / PPS / VPS ... (Protocol Version 6)
                if(protocolVersion >= 6) {
                    int length = 0;

                    // put SPS
                    uint8_t* sps = stream->GetVideoDecoderSPS(length);
                    resp->put_U8(length);

                    if(sps != NULL) {
                        resp->put_Blob(sps, length);
                    }

                    // put PPS
                    uint8_t* pps = stream->GetVideoDecoderPPS(length);
                    resp->put_U8(length);

                    if(pps != NULL) {
                        resp->put_Blob(pps, length);
                    }

                    // put VPS
                    uint8_t* vps = stream->GetVideoDecoderVPS(length);
                    resp->put_U8(length);

                    if(pps != NULL) {
                        resp->put_Blob(vps, length);
                    }
                }

                break;

            case StreamInfo::scSUBTITLE:
                resp->put_String(stream->TypeName());
                resp->put_String(stream->GetLanguage());
                resp->put_U32(stream->CompositionPageId());
                resp->put_U32(stream->AncillaryPageId());
                break;

            case StreamInfo::scTELETEXT:
                resp->put_String(stream->TypeName());
                break;

            default:
                break;
        }
    }

    return resp;
}

