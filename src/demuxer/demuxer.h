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

#ifndef ROBOTV_DEMUXER_H
#define ROBOTV_DEMUXER_H

#include <stdint.h>
#include <list>
#include "streaminfo.h"

class Parser;

#define DVD_NOPTS_VALUE    (-1LL<<52) // should be possible to represent in both double and __int64

struct StreamPacket {

    StreamPacket() {
        type = StreamInfo::stNONE;
        content = StreamInfo::scNONE;
        frameType = StreamInfo::ftUNKNOWN;
    }

    StreamInfo::FrameType frameType;
    StreamInfo::Type type;
    StreamInfo::Content content;

    int64_t pid;
    int64_t dts;
    int64_t pts;
    int64_t rawDts;
    int64_t rawPts;
    int duration;

    uint8_t* data;
    int size;
};

class TsDemuxer : public StreamInfo {
public:

    class Listener {
    public:

        virtual ~Listener() {};

        virtual void sendStreamPacket(StreamPacket* p) = 0;

        virtual void requestStreamChange() = 0;

    };

private:

    Listener* m_streamer;

    Parser* m_pesParser;

    int64_t rescale(int64_t a);

public:

    TsDemuxer(Listener* streamer, StreamInfo::Type type, int pid);

    TsDemuxer(Listener* streamer, const StreamInfo& info);

    virtual ~TsDemuxer();

    bool processTsPacket(unsigned char* data) const;

    const char* getLanguage() const {
        return m_language;
    }

    uint8_t getAudioType() const {
        return m_audioType;
    }

    bool isParsed() const {
        return m_parsed;
    }

    /* Video Stream Information */
    void setVideoInformation(int FpsScale, int FpsRate, int Height, int Width, int Aspect, int num, int den);

    uint32_t getFpsScale() const {
        return m_fpsScale;
    }

    uint32_t getFpsRate() const {
        return m_fpsRate;
    }

    uint32_t getHeight() const {
        return m_height;
    }

    uint32_t getWidth() const {
        return m_width;
    }

    int getAspect() const {
        return m_aspect;
    }

    /* Audio Stream Information */
    void setAudioInformation(int Channels, int SampleRate, int BitRate, int BitsPerSample, int BlockAlign);

    uint32_t getChannels() const {
        return m_channels;
    }

    uint32_t getSampleRate() const {
        return m_sampleRate;
    }

    uint32_t getBlockAlign() const {
        return m_blockAlign;
    }

    uint32_t getBitRate() const {
        return m_bitRate;
    }

    uint32_t getBitsPerSample() const {
        return m_bitsPerSample;
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

    /* Decoder specific data */
    void setVideoDecoderData(uint8_t* sps, int spsLength, uint8_t* pps, int ppsLength, uint8_t* vps = NULL, int vpsLength = 0);

    uint8_t* getVideoDecoderSps(int& length);

    uint8_t* getVideoDecoderPps(int& length);

    uint8_t* getVideoDecoderVps(int& length);

protected:

    void sendPacket(StreamPacket* pkt);

    friend class Parser;

private:

    Parser* createParser(StreamInfo::Type type);

};

#endif // ROBOTV_DEMUXER_H
