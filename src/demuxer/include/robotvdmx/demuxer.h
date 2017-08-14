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

#ifndef ROBOTV_DEMUXER_H
#define ROBOTV_DEMUXER_H

#include <stdint.h>
#include <list>
#include "streaminfo.h"

class Parser;

#define DVD_NOPTS_VALUE    (-1LL<<52) // should be possible to represent in both double and __int64

class TsDemuxer : public StreamInfo {
public:

    struct StreamPacket {
        StreamInfo::FrameType frameType = StreamInfo::FrameType::UNKNOWN;
        StreamInfo::Type type = StreamInfo::Type::NONE;
        StreamInfo::Content content = StreamInfo::Content::NONE;

        int64_t pid;
        int64_t dts;
        int64_t pts;
        int64_t streamPosition = 0;
        int duration = 0;

        uint8_t* data = nullptr;
        int size = 0;
    };

    class Listener {
    public:

        virtual ~Listener() {};

        virtual void onStreamPacket(StreamPacket *p) = 0;

        virtual void onStreamChange() = 0;

    };

private:

    Listener* m_streamer;

    Parser* m_pesParser;

    int64_t rescale(int64_t a);

public:

    TsDemuxer(Listener* streamer, StreamInfo::Type type, int pid);

    TsDemuxer(Listener* streamer, const StreamInfo& info);

    virtual ~TsDemuxer();

    bool processTsPacket(unsigned char* packet) const;

    const char* getLanguage() const {
        return m_language;
    }

    uint8_t getAudioType() const {
        return m_audioType;
    }

    /* Video Stream Information */
    void setVideoInformation(int fpsScale, int fpsRate, uint16_t height, uint16_t width, int aspect);

    uint32_t getFpsScale() const {
        return m_fpsScale;
    }

    uint32_t getFpsRate() const {
        return m_fpsRate;
    }

    uint16_t getHeight() const {
        return m_height;
    }

    uint16_t getWidth() const {
        return m_width;
    }

    int getAspect() const {
        return m_aspect;
    }

    /* Audio Stream Information */
    void setAudioInformation(uint8_t channels, uint32_t sampleRate, uint32_t bitRate);

    uint32_t getChannels() const {
        return m_channels;
    }

    uint32_t getSampleRate() const {
        return m_sampleRate;
    }

    uint32_t getBitRate() const {
        return m_bitRate;
    }

    /* Subtitle related stream information */
    unsigned char subtitlingType() const {
        return m_subTitlingType;
    }
    uint16_t compositionPageId() const {
        return m_compositionPageId;
    }

    uint16_t ancillaryPageId() const {
        return m_ancillaryPageId;
    }

    inline void setStreamPosition(int64_t position) {
        m_streamPosition = position;
    }

    /* Decoder specific data */
    void setVideoDecoderData(uint8_t* sps, size_t spsLength, uint8_t* pps, size_t ppsLength, uint8_t* vps = NULL, size_t vpsLength = 0);

    uint8_t* getVideoDecoderSps(int& length);

    uint8_t* getVideoDecoderPps(int& length);

    uint8_t* getVideoDecoderVps(int& length);

    void reset();

    void flush();

protected:

    void sendPacket(StreamPacket* pkt);

    friend class Parser;

private:

    int64_t m_streamPosition;

    Parser* createParser(StreamInfo::Type type);

};

#endif // ROBOTV_DEMUXER_H
