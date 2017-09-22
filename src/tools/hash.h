/*
 *      vdr-plugin-robotv - roboTV server plugin for VDR
 *
 *      Copyright (C) 1986 Gary S. Brown (CRC32 code)
 *      Copyright (C) 2011 Alexander Pipelka
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

#ifndef ROBOTV_HASH_H
#define ROBOTV_HASH_H

#include <string>
#include <mutex>
#include <map>

#include <stdint.h>
#include <vdr/channels.h>
#include <vdr/timers.h>

namespace roboTV {

class Hash {
public:

    static uint32_t createChannelUid(const cChannel *channel);

    static const cChannel *findChannelByUid(const cChannels *channels, uint32_t channelUID);

    static uint32_t createTimerUid(const cTimer *channel);

    static const cTimer *findTimerByUid(const cTimers *timers, uint32_t timerUID);

    static cTimer *findTimerByUid(cTimers *timers, uint32_t timerUID);

    static uint32_t createStringHash(const std::string &string);

private:

    static uint32_t crc32(const char* buf, size_t size);

    static std::map<const std::string, uint32_t> m_map;

    static std::mutex m_mutex;
};

} // namespace roboTV

#endif // ROBOTV_HASH_H
