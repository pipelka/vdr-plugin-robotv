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

#ifndef ROBOTV_CHANNELCONTROLLER_H
#define ROBOTV_CHANNELCONTROLLER_H

#include <list>
#include "vdr/channels.h"
#include "robotv/robotvchannels.h"
#include "controller.h"

class MsgPacket;

class ChannelController : public Controller {
public:

    ChannelController();

    virtual ~ChannelController();

    bool process(MsgPacket* request, MsgPacket* response);

    void addChannelToPacket(const cChannel*, MsgPacket*);

protected:

    bool processGetChannels(MsgPacket* request, MsgPacket* response);

private:

    int channelCount();

    bool isChannelWanted(cChannel* channel, int type = 0);

    cString createLogoUrl(const cChannel* channel);

    cString createServiceReference(const cChannel* channel);

    static bool isRadio(const cChannel* channel);

    ChannelController(const ChannelController& orig);

    std::list<int> m_caids;

    int m_languageIndex = -1;

    bool m_wantFta = true;

    bool m_filterLanguage = false;

    int m_channelCount = 0;

    cCharSetConv m_toUtf8;

};

#endif // ROBOTV_CHANNELCONTROLLER_H

