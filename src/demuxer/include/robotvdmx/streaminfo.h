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

#ifndef ROBOTV_STREAMINFO_H
#define ROBOTV_STREAMINFO_H

#include <stdint.h>
#include <fstream>
#include <string>

class StreamInfo {
public:

    enum class Content {
        NONE,
        VIDEO,
        AUDIO,
        SUBTITLE,
        TELETEXT,
        STREAMINFO
    };

    enum class Type {
        NONE,
        MPEG2AUDIO,
        AC3,
        EAC3,
        AAC,
        LATM,
        MPEG2VIDEO,
        H264,
        DVBSUB,
        TELETEXT,
        H265
    };

    enum class FrameType {
        UNKNOWN,
        IFRAME,
        PFRAME,
        BFRAME,
        DFRAME
    };

public:

    StreamInfo();

    StreamInfo(int pid, Type type, const char* lang = NULL);

    virtual ~StreamInfo();

    bool operator ==(const StreamInfo& rhs) const;

    bool isMetaOf(const StreamInfo& rhs) const;

    bool operator !=(const StreamInfo& rhs) const;

    inline int getPid() const {
        return m_pid;
    }

    static const Content getContent(Type type);

    inline const Content& getContent() const {
        return m_content;
    }

    void setContent();

    inline const Type& getType() const {
        return m_type;
    }

    const char* typeName();

    static const char* typeName(const StreamInfo::Type& type);

    static const char* contentName(const StreamInfo::Content& content);

    std::string info() const;

    inline bool isParsed() const {
        return m_parsed;
    }

    inline bool isEnabled() const {
        return m_enabled;
    }

    void setSubtitlingDescriptor(unsigned char SubtitlingType, uint16_t CompositionPageId, uint16_t AncillaryPageId);

protected:

    Content m_content; // stream content (e.g. scVIDEO)

    Type m_type; // stream type (e.g. stAC3)

    int m_pid; // transport stream pid

    char m_language[4]; // ISO 639 3-letter language code (empty string if undefined)

    uint8_t m_audioType; // ISO 639 audio type

    uint32_t m_fpsScale; // scale of 1000 and a rate of 29970 will result in 29.97 fps

    uint32_t m_fpsRate;

    uint16_t m_height; // height of the stream reported by the demuxer

    uint16_t m_width; // width of the stream reported by the demuxer

    int m_aspect; // display aspect of stream (*10000 : 1,7777 = 17777)

    uint8_t m_channels; // number of audio channels (e.g. 6 for 5.1)

    uint32_t m_sampleRate; // number of audio samples per second (e.g. 48000)

    uint32_t m_bitRate; // audio bitrate (e.g. 160000)

    bool m_parsed; // stream parsed flag (if all stream data is known)

    unsigned char m_subTitlingType; // subtitling type

    uint16_t m_compositionPageId; // composition page id

    uint16_t m_ancillaryPageId; // ancillary page id

    // decoder data
    uint8_t m_sps[128]; // SPS data (for decoder)
    uint8_t m_pps[128]; // PPS data (for decoder)
    uint8_t m_vps[128]; // VPS data (for decoder)

    size_t m_spsLength; // SPS length
    size_t m_ppsLength; // PPS length
    size_t m_vpsLength; // VPS length

    bool m_enabled = false;

    friend class ChannelCache;

private:

    void Initialize();

};

#endif // ROBOTV_STREAMINFO_H
