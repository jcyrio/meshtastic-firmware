#include "TextMessageModule.h"
#include "MeshService.h"
#include "NodeDB.h"
#include "PowerFSM.h"
#include "buzz.h"
#include "configuration.h"
#ifdef SIMPLE_TDECK
#include "modules/ExternalNotificationModule.h" // for turning off LED when get broadcast that we don't want to accept
#include "modules/CannedMessageModule.h" // for turning off LED when get broadcast that we don't want to accept
#include "graphics/Screen.h"
extern bool wakeOnMessage;
extern bool keyboardLockMode;
extern bool isSpecialDarkNode;
// using namespace graphics;

// #define FOR_H
// #define MONASTERY_FRIENDS
#define FATHERS_NODES
// #define SECURITY
// #define HELPERS
// #define GATE_SECURITY
// #define VASILI
#endif

TextMessageModule *textMessageModule;

ProcessMessage TextMessageModule::handleReceived(const meshtastic_MeshPacket &mp)
{
#ifdef DEBUG_PORT
    auto &p = mp.decoded;
    LOG_INFO("Received text msg from=0x%0x, id=0x%x, msg=%.*s\n", mp.from, mp.id, p.payload.size, p.payload.bytes);
#endif
// #ifdef SIMPLE_TDECK
		// if (strcmp(reinterpret_cast<const char*>(p.payload.bytes), "c") == 0) {
		// 	LOG_INFO("Was command for clearing previous messages\n");
		// 	screen->clearHistory();
		// 	return ProcessMessage::STOP;
		// }
// #endif
// #ifdef FATHERS_NODES
#if defined(FATHERS_NODES) || defined(FOR_H) || defined(VASILI)
		// char channelName[20];
		// snprintf(channelName, sizeof(channelName), "%s", channels.getName(mp.channel));
		// LOG_DEBUG("Channel Name: %s\n", channelName);
		// Ignore all broadcasts or DMs from LongFast
		// if (strcmp(channels.getName(mp.channel), "LongFast") == 0) {
			// LOG_INFO("Channel Name is LongFast\n");
		LOG_INFO("Channel Name: %s\n", channels.getName(mp.channel));
		LOG_INFO("mp.to: %d\n", mp.to);
		LOG_INFO("NODENUM_BROADCAST: %d\n", NODENUM_BROADCAST);
		bool isNotStAchannel = strcmp(channels.getName(mp.channel), "StA") != 0;
		bool isNotT1channel = strcmp(channels.getName(mp.channel), "T1") != 0;
		if (isNotStAchannel && isNotT1channel && mp.to == NODENUM_BROADCAST) {
			LOG_INFO("Was Broadcast message, but Channel Name is not StA, ignoring\n");
			externalNotificationModule->setExternalOff(0); // this will turn off all GPIO and sounds and idle the loop
			return ProcessMessage::STOP;
		}
#endif
#if defined(SECURITY) || defined(GATE_SECURITY)
#if defined(SECURITY)
		bool isNotVarangiansChannel = strcmp(channels.getName(mp.channel), "Varangians") != 0;
		bool isNotC2OPSchannel = strcmp(channels.getName(mp.channel), "C2OPS") != 0;
		// if ((strcmp(channels.getName(mp.channel), "Varangians") != 0) && (mp.to == NODENUM_BROADCAST)) {
		if (isNotVarangiansChannel && isNotC2OPSchannel && mp.to == NODENUM_BROADCAST) {
			LOG_INFO("Was Broadcast message, but Channel Name is not Varangians, ignoring\n");
			externalNotificationModule->setExternalOff(0); // this will turn off all GPIO and sounds and idle the loop
			return ProcessMessage::STOP;
		}
#elif defined(GATE_SECURITY)
		bool isNotStAchannel = strcmp(channels.getName(mp.channel), "StA") != 0;
		bool isNotVarangiansChannel = strcmp(channels.getName(mp.channel), "Varangians") != 0;
		if (isNotStAchannel && isNotVarangiansChannel && mp.to == NODENUM_BROADCAST) {
			LOG_INFO("Was Broadcast message, but Channel Name is not StA or Varangians, ignoring\n");
			externalNotificationModule->setExternalOff(0); // this will turn off all GPIO and sounds and idle the loop
			return ProcessMessage::STOP;
		}
#endif
#endif
#ifdef MONASTERY_FRIENDS
		if ((strcmp(channels.getName(mp.channel), "MFA") != 0) && (mp.to == NODENUM_BROADCAST)) {
			LOG_INFO("Was Broadcast message, but Channel Name is not MFA, ignoring\n");
			externalNotificationModule->setExternalOff(0); // this will turn off all GPIO and sounds and idle the loop
			return ProcessMessage::STOP;
		}
#endif

    // We only store/display messages destined for us.
    // Keep a copy of the most recent text message.
#ifdef SIMPLE_TDECK
		if (mp.to == NODENUM_BROADCAST) { // if was broadcast, add prefix to message
			LOG_INFO("Was Broadcast message, adding prefix\n");
 		  meshtastic_MeshPacket modifiedPacket = mp;
			const char* prefix = "ALL: ";
			const pb_size_t prefixLen = strlen(prefix);
			// Make sure we don't exceed buffer size
			if (prefixLen + mp.decoded.payload.size > sizeof(modifiedPacket.decoded.payload.bytes)) {
					// Handle overflow case - truncate the message if needed
					pb_size_t messageLen = sizeof(modifiedPacket.decoded.payload.bytes) - prefixLen;
					memmove(
							modifiedPacket.decoded.payload.bytes + prefixLen,
							mp.decoded.payload.bytes,
							messageLen
					);
			} else {
					// Enough space for both prefix and message
					memmove(
							modifiedPacket.decoded.payload.bytes + prefixLen,
							mp.decoded.payload.bytes,
							mp.decoded.payload.size
					);
			}
			// Copy prefix at the start
			memcpy(modifiedPacket.decoded.payload.bytes, prefix, prefixLen);
			modifiedPacket.decoded.payload.size = prefixLen + mp.decoded.payload.size;
			// meshtastic_MeshPacket modifiedPacket = mp;
			// const char* prefix = "ALL: ";
			// size_t prefixLen = strlen(prefix); // Create new buffer with space for prefix + original message
			// size_t newSize = prefixLen + mp.decoded.payload.size;
			// uint8_t* newBuffer = new uint8_t[newSize];
			// memcpy(newBuffer, prefix, prefixLen); // Copy prefix
			// memcpy(newBuffer + prefixLen, mp.decoded.payload.bytes, mp.decoded.payload.size); // Copy original message
			// modifiedPacket.decoded.payload.bytes = newBuffer; // Update the payload
			// modifiedPacket.decoded.payload.size = newSize;
			devicestate.rx_text_message = modifiedPacket;
			devicestate.has_rx_text_message = true;
			powerFSM.trigger(EVENT_RECEIVED_MSG);
			notifyObservers(&modifiedPacket);
			// delete[] newBuffer; // Clean up
		} else { // was not broadcast
			devicestate.rx_text_message = mp;
			devicestate.has_rx_text_message = true;

#ifdef SIMPLE_TDECK
			// if (screen->keyboardLockMode && wakeOnMessage) {
			if (keyboardLockMode && isSpecialDarkNode) {
				powerFSM.trigger(EVENT_DARK);
				const char* currentMsgContent = reinterpret_cast<const char*>(mp.decoded.payload.bytes);
				// const char* myNodeName = cannedMessageModule->getNodeName(mp.from);
				auto node = nodeDB->getMeshNode(getFrom(&mp));
				const char* sender = (node) ? node->user.short_name : "???";
				cannedMessageModule->addToHistoryWithArgs(currentMsgContent, sender);
			} else {
				powerFSM.trigger(EVENT_RECEIVED_MSG);
			}
#else
			powerFSM.trigger(EVENT_RECEIVED_MSG);
#endif
			notifyObservers(&mp);
		}
#else
    devicestate.rx_text_message = mp;
    devicestate.has_rx_text_message = true;

    powerFSM.trigger(EVENT_RECEIVED_MSG);
    notifyObservers(&mp);
#endif

    return ProcessMessage::CONTINUE; // Let others look at this message also if they want
}

bool TextMessageModule::wantPacket(const meshtastic_MeshPacket *p)
{
    return MeshService::isTextPayload(p);
}
