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

#include "wirbelscan.h"
#include "config/config.h"

using namespace WIRBELSCAN_SERVICE;

WirbelScan::WirbelScan() : m_plugin(NULL) {
	m_plugin = cPluginManager::GetPlugin("wirbelscan");

	if(m_plugin == NULL) {
		esyslog("Unable to connect to wirbelscan plugin !");
		return;
	}

	isyslog("Connected to wirbelscan plugin ...");

	cWirbelscanInfo info;

	if(getVersion(&info)) {
		isyslog("wirbelscan plugin version: %s", info.PluginVersion);
	}
	else {
		esyslog("unable to fetch version information !");
	}
}

WirbelScan::~WirbelScan() {
	if(!isScanning()) {
		return;
	}

	WIRBELSCAN_SERVICE::cWirbelscanCmd cmd;
	cmd.cmd = WIRBELSCAN_SERVICE::CmdStopScan;
	doCmd(cmd);
}

bool WirbelScan::getVersion(cWirbelscanInfo* info) {
	if(m_plugin == NULL) {
		return false;
	}

	return m_plugin->Service(SPlugin "" SInfo, info);
}

bool WirbelScan::doCmd(cWirbelscanCmd& cmd) {
	if(m_plugin == NULL) {
		return false;
	}

	if(!m_plugin->Service(SPlugin "" SCommand, &cmd)) {
		cmd.replycode = false;
	}

	return cmd.replycode;
}

bool WirbelScan::getStatus(cWirbelscanStatus& status) {
	if(m_plugin == NULL) {
		return false;
	}

	return m_plugin->Service(SPlugin "Get" SStatus, &status);
}

bool WirbelScan::getSetup(cWirbelscanScanSetup& param) {
	if(m_plugin == NULL) {
		return false;
	}

	return m_plugin->Service(SPlugin "Get" SSetup, &param);
}

bool WirbelScan::setSetup(cWirbelscanScanSetup& param) {
	if(m_plugin == NULL) {
		return false;
	}

	return m_plugin->Service(SPlugin "Set" SSetup, &param);
}

bool WirbelScan::getCountry(WirbelScan::List& list) {
	if(m_plugin == NULL) {
		return false;
	}

	if(!m_plugin->Service(SPlugin "Get" SCountry, NULL)) {
		return false;
	}

	cPreAllocBuffer b = {0, 0, NULL};
	m_plugin->Service(SPlugin "Get" SCountry, &b);

	SListItem* buffer = new SListItem[b.size];

	b.buffer = buffer;
	m_plugin->Service(SPlugin "Get" SCountry, &b);

	list.clear();

	for(uint32_t i = 0; i < b.count; i++) {
		list.push_back(b.buffer[i]);
	}

	return true;
}

bool WirbelScan::getSat(WirbelScan::List& list) {
	if(m_plugin == NULL) {
		return false;
	}

	if(!m_plugin->Service(SPlugin "Get" SSat, NULL)) {
		return false;
	}

	cPreAllocBuffer b = {0, 0, NULL};
	m_plugin->Service(SPlugin "Get" SSat, &b);

	SListItem* buffer = new SListItem[b.size];

	b.buffer = buffer;
	m_plugin->Service(SPlugin "Get" SSat, &b);

	list.clear();

	for(uint32_t i = 0; i < b.count; i++) {
		list.push_back(b.buffer[i]);
	}

	return true;
}

bool WirbelScan::isScanning() {
	cWirbelscanStatus status;

	if(!getStatus(status)) {
		return false;
	}

	return status.status == StatusScanning;
}

// currently unused

bool WirbelScan::getUser(cUserTransponder& transponder) {
	return false;
}

// currently unused

bool WirbelScan::setUser(cUserTransponder& transponder) {
	return false;
}
