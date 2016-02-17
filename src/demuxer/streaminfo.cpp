/*
 *      vdr-plugin-robotv - RoboTV server plugin for VDR
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

#include "streaminfo.h"
#include "config/config.h"
#include <string.h>

static const char* contentnames[] = {
    "NONE", "VIDEO", "AUDIO", "SUBTITLE", "TELETEXT"
};

static const char* typenames[] = {
    "NONE", "MPEG2AUDIO", "AC3", "EAC3", "AAC", "AAC", "MPEG2VIDEO", "H264", "DVBSUB", "TELETEXT", "H265"
};


StreamInfo::StreamInfo() {
    Initialize();
}

StreamInfo::StreamInfo(int pid, Type type, const char* lang) {
    Initialize();

    m_pid = pid;
    m_type = type;
    m_parsed = false;

    if(lang != NULL) {
        strncpy(m_language, lang, 4);
    }

    memset(m_sps, 0, sizeof(m_sps));
    memset(m_pps, 0, sizeof(m_pps));
    memset(m_vps, 0, sizeof(m_vps));

    setContent();
}

StreamInfo::~StreamInfo() {
}

void StreamInfo::Initialize() {
    m_language[0]       = 0;
    m_audioType         = 0;
    m_fpsScale          = 0;
    m_fpsRate           = 0;
    m_height            = 0;
    m_width             = 0;
    m_aspect            = 0.0f;
    m_channels          = 0;
    m_sampleRate        = 0;
    m_bitRate           = 0;
    m_bitsPerSample     = 0;
    m_blockAlign        = 0;
    m_subTitlingType    = 0;
    m_compositionPageId = 0;
    m_ancillaryPageId   = 0;
    m_pid               = 0;
    m_type              = stNONE;
    m_content           = scNONE;
    m_parsed            = false;
    m_spsLength         = 0;
    m_ppsLength         = 0;
    m_vpsLength         = 0;
}

bool StreamInfo::operator ==(const StreamInfo& rhs) const {
    // general comparison
    if(!isMetaOf(rhs)) {
        return false;
    }

    switch(m_content) {
        case scNONE:
            return false;

        case scAUDIO:
            return
                (strcmp(m_language, rhs.m_language) == 0) &&
                (m_audioType == rhs.m_audioType) &&
                (m_channels == rhs.m_channels) &&
                (m_sampleRate == rhs.m_sampleRate);

        case scVIDEO:
            return
                (m_width == rhs.m_width) &&
                (m_height == rhs.m_height) &&
                (m_aspect == rhs.m_aspect) &&
                (m_fpsScale == rhs.m_fpsScale) &&
                (m_fpsRate == rhs.m_fpsRate);

        case scSUBTITLE:
            return
                (strcmp(m_language, rhs.m_language) == 0) &&
                (m_subTitlingType == rhs.m_subTitlingType) &&
                (m_compositionPageId == rhs.m_compositionPageId) &&
                (m_ancillaryPageId == rhs.m_ancillaryPageId);

        case scTELETEXT:
            return true;

        case scSTREAMINFO:
            return false;
    }

    return false;
}

bool StreamInfo::isMetaOf(const StreamInfo& rhs) const {
    if(m_content != rhs.m_content) {
        return false;
    }

    if(m_type != rhs.m_type && (m_type != stAC3 && rhs.m_type != stEAC3)) {
        return false;
    }

    return (m_pid == rhs.m_pid);
}

bool StreamInfo::operator !=(const StreamInfo& rhs) const {
    return !((*this) == rhs);
}

void StreamInfo::setContent() {
    m_content = getContent(m_type);
}

const StreamInfo::Content StreamInfo::getContent(Type type) {
    if(type == stMPEG2AUDIO || type == stAC3 || type == stEAC3  || type == stAAC || type == stLATM) {
        return scAUDIO;
    }
    else if(type == stMPEG2VIDEO || type == stH264 || type == stH265) {
        return scVIDEO;
    }
    else if(type == stDVBSUB) {
        return scSUBTITLE;
    }
    else if(type == stTELETEXT) {
        return scTELETEXT;
    }

    return scNONE;
}

const char* StreamInfo::typeName() {
    return typeName(m_type);
}

const char* StreamInfo::typeName(const StreamInfo::Type& type) {
    return typenames[type];
}

const char* StreamInfo::contentName(const StreamInfo::Content& content) {
    return contentnames[content];
}

void StreamInfo::info() const {
    char buffer[100];
    buffer[0] = 0;

    int scale = m_fpsScale;

    if(scale == 0) {
        scale = 1;
    }

    if(m_content == scAUDIO) {
        snprintf(buffer, sizeof(buffer), "%i Hz, %i channels, Lang: %s", m_sampleRate, m_channels, m_language);
    }
    else if(m_content == scVIDEO) {
        snprintf(buffer, sizeof(buffer), "%ix%i DAR: %.2f FPS: %.3f SPS/PPS/VPS: %i/%i/%i bytes", m_width, m_height , (double)m_aspect / 10000, (double)m_fpsRate / (double)scale, m_spsLength, m_ppsLength, m_vpsLength);
    }
    else if(m_content == scSUBTITLE) {
        snprintf(buffer, sizeof(buffer), "Lang: %s", m_language);
    }
    else if(m_content == scTELETEXT) {
        snprintf(buffer, sizeof(buffer), "TXT");
    }
    else if(m_content == scNONE) {
        snprintf(buffer, sizeof(buffer), "None");
    }
    else {
        snprintf(buffer, sizeof(buffer), "Unknown");
    }

    INFOLOG("Stream: %s PID: %i %s (parsed: %s)", typeName(m_type), m_pid, buffer, (m_parsed ? "yes" : "no"));
}

void StreamInfo::setSubtitlingDescriptor(unsigned char SubtitlingType, uint16_t CompositionPageId, uint16_t AncillaryPageId) {
    m_subTitlingType    = SubtitlingType;
    m_compositionPageId = CompositionPageId;
    m_ancillaryPageId   = AncillaryPageId;
    m_parsed            = true;
}
