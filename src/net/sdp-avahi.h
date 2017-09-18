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

#ifndef ROBOTV_SDP_AVAHI_H
#define ROBOTV_SDP_AVAHI_H

#include <avahi-client/client.h>
#include "sdp.h"

struct AvahiClient;
struct AvahiEntryGroup;
struct AvahiSimplePoll;
struct AvahiClient;

namespace roboTV {

class SdpAvahi : public Sdp {
public:

    SdpAvahi();

    virtual ~SdpAvahi();

    bool poll(int timeoutMs) override;

protected:

    bool start() override;

    bool stop() override;

private:

    bool createServices(AvahiClient *c);

    static void entryGroupCallback(AvahiEntryGroup* g, AvahiEntryGroupState state, void *userdata);

    static void clientCallback(AvahiClient* c, AvahiClientState state, void *userdata);

    AvahiClient* m_client;

    AvahiSimplePoll* m_poll = nullptr;

    AvahiEntryGroup* m_group = nullptr;

    static constexpr const char* m_name = "roboTV";
};

} // namespace roboTV


#endif // ROBOTV_SDP_AVAHI_H
