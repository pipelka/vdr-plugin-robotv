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

#ifndef ROBOTV_PES_H
#define ROBOTV_PES_H

#include <stdint.h>

// PES PIDs

#define PRIVATE_STREAM1   0xBD
#define PADDING_STREAM    0xBE
#define PRIVATE_STREAM2   0xBF
#define PRIVATE_STREAM3   0xFD
#define AUDIO_STREAM_S    0xC0  /* 1100 0000 */
#define AUDIO_STREAM_E    0xDF  /* 1101 1111 */
#define VIDEO_STREAM_S    0xE0  /* 1110 0000 */
#define VIDEO_STREAM_E    0xEF  /* 1110 1111 */

#define AUDIO_STREAM_MASK 0x1F  /* 0001 1111 */
#define VIDEO_STREAM_MASK 0x0F  /* 0000 1111 */
#define AUDIO_STREAM      0xC0  /* 1100 0000 */
#define VIDEO_STREAM      0xE0  /* 1110 0000 */

#define ECM_STREAM        0xF0
#define EMM_STREAM        0xF1
#define DSM_CC_STREAM     0xF2
#define ISO13522_STREAM   0xF3
#define PROG_STREAM_DIR   0xFF
#define MAX33BIT  0x00000001FFFFFFFFLL


// PES helper functions

inline bool PesIsHeader(const uint8_t* p) {
    return !(p)[0] && !(p)[1] && (p)[2] == 1;
}

inline bool PesHasLength(const uint8_t *p)
{
    return p[4] | p[5];
}

inline int PesLength(const uint8_t *p)
{
    return 6 + p[4] * 256 + p[5];
}

inline int PesPayloadOffset(const uint8_t *p)
{
    return 9 + p[8];
}

inline bool PesHasPts(const uint8_t *p)
{
    return (p[7] & 0x80) && p[8] >= 5;
}

inline int64_t PtsAdd(int64_t Pts1, int64_t Pts2) { return (Pts1 + Pts2) & MAX33BIT; }

inline bool PesHasDts(const uint8_t *p)
{
    return (p[7] & 0x40) && p[8] >= 10;
}

inline int64_t PesGetPts(const uint8_t *p)
{
    return ((((int64_t)p[ 9]) & 0x0E) << 29) |
           (( (int64_t)p[10])         << 22) |
           ((((int64_t)p[11]) & 0xFE) << 14) |
           (( (int64_t)p[12])         <<  7) |
           ((((int64_t)p[13]) & 0xFE) >>  1);
}

inline int64_t PesGetDts(const uint8_t *p)
{
    return ((((int64_t)p[14]) & 0x0E) << 29) |
           (( (int64_t)p[15])         << 22) |
           ((((int64_t)p[16]) & 0xFE) << 14) |
           (( (int64_t)p[17])         <<  7) |
           ((((int64_t)p[18]) & 0xFE) >>  1);
}


// TS Constants

#define TS_PAYLOAD_START      0x40
#define TS_SIZE               188
#define TS_ADAPT_FIELD_EXISTS 0x20
#define TS_SCRAMBLING_CONTROL 0xC0
#define TS_ERROR              0x80
#define TS_PAYLOAD_EXISTS     0x10
#define TS_PID_MASK_HI        0x1F


// TS Helper Functions

inline bool TsPayloadStart(const uint8_t *p) {
    return p[1] & TS_PAYLOAD_START;
}

inline bool TsHasAdaptationField(const uint8_t *p) {
    return p[3] & TS_ADAPT_FIELD_EXISTS;
}

inline int TsPayloadOffset(const uint8_t *p) {
    int o = TsHasAdaptationField(p) ? p[4] + 5 : 4;
    return o <= TS_SIZE ? o : TS_SIZE;
}

inline bool TsIsScrambled(const uint8_t *p) {
    return p[3] & TS_SCRAMBLING_CONTROL;
}

inline bool TsError(const uint8_t *p) {
    return p[1] & TS_ERROR;
}

inline bool TsHasPayload(const uint8_t *p) {
    return p[3] & TS_PAYLOAD_EXISTS;
}

inline int TsPid(const uint8_t *p) {
    return (p[1] & TS_PID_MASK_HI) * 256 + p[2];
}

#endif // ROBOTV_PES_H

