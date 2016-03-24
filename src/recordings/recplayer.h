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

#ifndef ROBOTV_RECPLAYER_H
#define ROBOTV_RECPLAYER_H

#include <stdio.h>
#include <vdr/tools.h>
#include <vdr/recording.h>

class Segment {
public:
    uint64_t start;
    uint64_t end;
};

class RecPlayer {
public:

    RecPlayer(cRecording* rec);

    ~RecPlayer();

    uint64_t getLengthBytes();

    int getBlock(unsigned char* buffer, uint64_t position, int amount);

    bool openFile(int index);

    void closeFile();

protected:

    bool update();

    uint64_t m_totalLength;

    cVector<Segment*> m_segments;

private:

    void scan();

    void cleanup();

    char* fileNameFromIndex(int index);

    void checkBufferSize(int s);

    bool m_pesrecording;

    char m_fileName[512];

    int m_file;

    int m_fileOpen;

    char* m_recordingFilename;

    cTimeMs m_rescanTime;

    uint32_t m_rescanInterval;
};

#endif // ROBOTV_RECPLAYER_H
