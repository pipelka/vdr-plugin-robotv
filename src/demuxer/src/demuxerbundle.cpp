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

#include <string.h>

#include "robotvdmx/demuxerbundle.h"
#include "robotvdmx/pes.h"

DemuxerBundle::DemuxerBundle(TsDemuxer::Listener* listener) : m_listener(listener) {
}

DemuxerBundle::~DemuxerBundle() {
    clear();
}


void DemuxerBundle::clear() {
    for(auto i = begin(); i != end(); i++) {
        if((*i) != NULL) {
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

void DemuxerBundle::reorderStreams(const char* lang, StreamInfo::Type type) {
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
            case StreamInfo::Content::VIDEO:
                w |= VIDEO_MASK;
                break;

            case StreamInfo::Content::AUDIO:
                w |= AUDIO_MASK;

                // weight of audio stream type
                w |= (stream->getType() == type) ? STREAMTYPE_MASK : 0;

                // weight of audio type
                w |= ((4 - stream->getAudioType()) << 16) & AUDIOTYPE_MASK;
                break;

            case StreamInfo::Content::SUBTITLE:
                w |= SUBTITLE_MASK;
                break;

            default:
                break;
        }

        // weight of language
        w |= (strcmp(stream->getLanguage(), lang) == 0) ? LANGUAGE_MASK : 0;

        // summed weight
        weight[w] = stream;
    }

    // reorder streams on weight
    int idx = 0;
    std::list<TsDemuxer*>::clear();

    for(std::map<uint32_t, TsDemuxer*>::reverse_iterator i = weight.rbegin(); i != weight.rend(); i++, idx++) {
        TsDemuxer* stream = i->second;
        push_back(stream);
    }
}

bool DemuxerBundle::isReady() const {
    if(empty()) {
        return false;
    }

    for(auto i = begin(); i != end(); i++) {
        if(!(*i)->isParsed()) {
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

        push_back(dmx);
    }
}

bool DemuxerBundle::processTsPacket(uint8_t* packet) const {
    int pid = TsPid(packet);
    TsDemuxer* demuxer = findDemuxer(pid);

    if(demuxer == NULL) {
        return false;
    }

    return demuxer->processTsPacket(packet);
}
