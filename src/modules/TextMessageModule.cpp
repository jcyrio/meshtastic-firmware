#include "TextMessageModule.h"
#include "MeshService.h"
#include "NodeDB.h"
#include "PowerFSM.h"
#include "buzz.h"
#include "configuration.h"
TextMessageModule *textMessageModule;

ProcessMessage TextMessageModule::handleReceived(const meshtastic_MeshPacket &mp)
{
#ifdef DEBUG_PORT
    auto &p = mp.decoded;
    LOG_INFO("Received text msg from=0x%0x, id=0x%x, msg=%.*s\n", mp.from, mp.id, p.payload.size, p.payload.bytes);
#endif
#ifdef SIMPLE_TDECK
		// char channelName[20];
		// snprintf(channelName, sizeof(channelName), "%s", channels.getName(mp.channel));
		// LOG_DEBUG("Channel Name: %s\n", channelName);
		// Ignore all broadcasts or DMs from LongFast
		// if (strcmp(channels.getName(mp.channel), "LongFast") == 0) {
			// LOG_INFO("Channel Name is LongFast\n");
		if ((strcmp(channels.getName(mp.channel), "StA") != 0) && (mp.to == 0xffffffff)) {
			LOG_INFO("Was Broadcast message, but Channel Name is not StA, ignoring\n");
			return ProcessMessage::STOP;
		}
#endif
    // We only store/display messages destined for us.
    // Keep a copy of the most recent text message.
    devicestate.rx_text_message = mp;
    devicestate.has_rx_text_message = true;

    powerFSM.trigger(EVENT_RECEIVED_MSG);
    notifyObservers(&mp);

    return ProcessMessage::CONTINUE; // Let others look at this message also if they want
}

bool TextMessageModule::wantPacket(const meshtastic_MeshPacket *p)
{
    return MeshService::isTextPayload(p);
}
