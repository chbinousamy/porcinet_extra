//--------------------------------------------------------------------------
// Copyright (C) 2014-2020 Cisco and/or its affiliates. All rights reserved.
//
// This program is free software; you can redistribute it and/or modify it
// under the terms of the GNU General Public License Version 2 as published
// by the Free Software Foundation.  You may not use, modify or distribute
// this program under any other version of the GNU General Public License.
//
// This program is distributed in the hope that it will be useful, but
// WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
// General Public License for more details.
//
// You should have received a copy of the GNU General Public License along
// with this program; if not, write to the Free Software Foundation, Inc.,
// 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
//--------------------------------------------------------------------------
// reg_test_splitter.cc author davis mcpherson <davmcphe@cisco.com>

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "rt_service_inspector.h"
#include "rt_service_inspector_splitter.h"

#include <string.h>

#include "framework/codec.h"
#include "log/messages.h"
#include "main/snort_config.h"
#include "packet_io/active.h"
#include "protocols/packet.h"
#include "stream/stream.h"

using namespace snort;

//--------------------------------------------------------------------------
// reg test splitter
//--------------------------------------------------------------------------

RegTestSplitter::RegTestSplitter(bool to_server) : StreamSplitter(to_server)
{ reset(); }

static bool has_script(const uint8_t* data, const uint32_t len)
{
    uint32_t partial_match = 0;
    static const uint8_t match_string[] = { '<', 's', 'c', 'r', 'i', 'p', 't', '>' };
    static const uint8_t string_length = sizeof(match_string);

    for (uint32_t k = 0; k < len; k++)
    {
        if (data[k] == match_string[partial_match])
        {
            if (++partial_match == string_length)
                return true;
        }
        else
        {
            partial_match = 0;
        }
    }
    return false;
}

StreamSplitter::Status RegTestSplitter::scan(
    Packet* p, const uint8_t* data, uint32_t len, uint32_t, uint32_t* fp)
{
    bytes += len;
    segs++;

    if ( strncmp((const char*)data, "flush", 5) == 0 )
    {
        *fp = len;
        rtsi_stats.flush_requests++;
        return FLUSH;
    }
    else if ( strncmp((const char*)data, "send_data_direct", 16) == 0 )
    {
        const char *data = "send_data_direct packet";
        EncodeFlags flags = ENC_FLAG_RST_SRVR | ENC_FLAG_RST_CLNT;
        p->packet_flags |= PKT_USE_DIRECT_INJECT;

        p->active->send_data(p, flags, (const uint8_t*)data, strlen(data));

        p->packet_flags &= ~PKT_USE_DIRECT_INJECT;
        rtsi_stats.send_data_direct_requests++;
    }
    else if ( strncmp((const char*)data, "send_data", 9) == 0 )
    {
        const char *data = "send_data packet";
        EncodeFlags flags = ENC_FLAG_RST_SRVR | ENC_FLAG_RST_CLNT;

        p->active->send_data(p, flags, (const uint8_t*)data, strlen(data));
        rtsi_stats.send_data_requests++;
    }
    else if ( strncmp((const char*)data, "hold", 4) == 0 )
    {
        Stream::set_packet_action_to_hold(p);
        rtsi_stats.hold_requests++;
    }
    else if (has_script(data, len) )
    {
        Stream::set_packet_action_to_hold(p);
        rtsi_stats.hold_requests++;
    }
    else if ( strncmp((const char*)data, "<no_ack>", 8) == 0 )
    {
        Stream::set_no_ack_mode(p->flow, true);
        LogMessage("rt_service: turn on no_ack mode\n");
    }
    else if ( strncmp((const char*)data, "</no_ack>", 9) == 0 )
    {
        Stream::set_no_ack_mode(p->flow, false);
        LogMessage("rt_service: turn off no_ack mode\n");
    }

    rtsi_stats.search_requests++;
    return SEARCH;
}

void RegTestSplitter::update()
{ reset(); }

bool RegTestSplitter::init_partial_flush(Flow*)
{ return true; }

void RegTestSplitter::reset()
{
    bytes = segs = 0;
}


