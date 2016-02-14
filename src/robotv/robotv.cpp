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

#include <getopt.h>
#include <vdr/plugin.h>
#include "robotv.h"

cPluginRoboTVServer::cPluginRoboTVServer(void) {
    Server = NULL;
}

cPluginRoboTVServer::~cPluginRoboTVServer() {
    // Clean up after yourself!
}

const char* cPluginRoboTVServer::CommandLineHelp(void) {
    return "  -t n, --timeout=n      stream data timeout in seconds (default: 10)\n";
}

bool cPluginRoboTVServer::ProcessArgs(int argc, char* argv[]) {
    // Implement command line argument processing here if applicable.
    static struct option long_options[] = {
        { "timeout",  required_argument, NULL, 't' },
        { NULL,       no_argument,       NULL,  0  }
    };

    int c;

    while((c = getopt_long(argc, argv, "t:", long_options, NULL)) != -1) {
        switch(c) {
            case 't':
                if(optarg != NULL) {
                    RoboTVServerConfig.stream_timeout = atoi(optarg);
                }

                break;

            default:
                return false;
        }
    }

    return true;
}

bool cPluginRoboTVServer::Initialize(void) {
    // Initialize any background activities the plugin shall perform.
    RoboTVServerConfig.ConfigDirectory = ConfigDirectory(PLUGIN_NAME_I18N);
#if VDRVERSNUM >= 10730
    RoboTVServerConfig.CacheDirectory = CacheDirectory(PLUGIN_NAME_I18N);
#else
    RoboTVServerConfig.CacheDirectory = ConfigDirectory(PLUGIN_NAME_I18N);
#endif
    RoboTVServerConfig.Load();
    return true;
}

bool cPluginRoboTVServer::Start(void) {
    Server = new cRoboTVServer(RoboTVServerConfig.listen_port);

    return true;
}

void cPluginRoboTVServer::Stop(void) {
    delete Server;
    Server = NULL;
}

void cPluginRoboTVServer::Housekeeping(void) {
    // Perform any cleanup or other regular tasks.
}

void cPluginRoboTVServer::MainThreadHook(void) {
    // Perform actions in the context of the main program thread.
    // WARNING: Use with great care - see PLUGINS.html!
}

cString cPluginRoboTVServer::Active(void) {
    // Return a message string if shutdown should be postponed
    return NULL;
}

time_t cPluginRoboTVServer::WakeupTime(void) {
    // Return custom wakeup time for shutdown script
    return 0;
}

cMenuSetupPage* cPluginRoboTVServer::SetupMenu(void) {
    // Return a setup menu in case the plugin supports one.
    return NULL;
}

bool cPluginRoboTVServer::SetupParse(const char* Name, const char* Value) {
    // Parse your own setup parameters and store their values.
    return false;
}

bool cPluginRoboTVServer::Service(const char* Id, void* Data) {
    // Handle custom service requests from other plugins
    return false;
}

const char** cPluginRoboTVServer::SVDRPHelpPages(void) {
    // Return help text for SVDRP commands this plugin implements
    return NULL;
}

cString cPluginRoboTVServer::SVDRPCommand(const char* Command, const char* Option, int& ReplyCode) {
    // Process SVDRP commands this plugin implements
    return NULL;
}

VDRPLUGINCREATOR(cPluginRoboTVServer); // Don't touch this!
