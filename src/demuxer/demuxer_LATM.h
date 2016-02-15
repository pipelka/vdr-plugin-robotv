/*
 *      vdr-plugin-robotv - RoboTV server plugin for VDR
 *
 *      Copyright (C) 2015 Aleaxander Pipelka
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

#ifndef ROBOTV_DEMUXER_LATM_H
#define ROBOTV_DEMUXER_LATM_H

#include "parser.h"

class cBitStream;

class ParserLatm : public Parser {
public:

    ParserLatm(TsDemuxer* demuxer);

protected:

    int ParsePayload(unsigned char* data, int len);

    void SendPayload(unsigned char* payload, int length);

    bool CheckAlignmentHeader(unsigned char* buffer, int& framesize);

    void ReadStreamMuxConfig(cBitStream* bs);

    void ReadAudioSpecificConfig(cBitStream* bs);

private:

    int m_samplerateindex;

};

#endif // ROBOTV_DEMUXER_LATM_H
