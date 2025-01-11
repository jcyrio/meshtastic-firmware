#include "TextMessageModule.h"
#include "MeshService.h"
#include "NodeDB.h"
#include "PowerFSM.h"
#include "buzz.h"
#include "configuration.h"
#ifdef SIMPLE_TDECK
#include "modules/ExternalNotificationModule.h" // for turning off LED when get broadcast that we don't want to accept
// #include "graphics/Screen.cpp"
// using namespace graphics;

// #define FOR_GUESTS
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
#if defined(FATHERS_NODES) || defined(FOR_GUESTS) || defined(VASILI)
		// char channelName[20];
		// snprintf(channelName, sizeof(channelName), "%s", channels.getName(mp.channel));
		// LOG_DEBUG("Channel Name: %s\n", channelName);
		// Ignore all broadcasts or DMs from LongFast
		// if (strcmp(channels.getName(mp.channel), "LongFast") == 0) {
			// LOG_INFO("Channel Name is LongFast\n");
		if ((strcmp(channels.getName(mp.channel), "StA") != 0) && (mp.to == 0xffffffff)) {
			LOG_INFO("Was Broadcast message, but Channel Name is not StA, ignoring\n");
			externalNotificationModule->setExternalOff(0); // this will turn off all GPIO and sounds and idle the loop
			return ProcessMessage::STOP;
		}
#endif
#if defined(SECURITY) || defined(GATE_SECURITY)
#if defined(SECURITY)
		if ((strcmp(channels.getName(mp.channel), "Varangians") != 0) && (mp.to == 0xffffffff)) {
			LOG_INFO("Was Broadcast message, but Channel Name is not Varangians, ignoring\n");
			externalNotificationModule->setExternalOff(0); // this will turn off all GPIO and sounds and idle the loop
			return ProcessMessage::STOP;
		}
#elif defined(GATE_SECURITY)
		if (((strcmp(channels.getName(mp.channel), "StA") != 0) && (strcmp(channels.getName(mp.channel), "Varangians") != 0)) && (mp.to == 0xffffffff)) {
			LOG_INFO("Was Broadcast message, but Channel Name is not StA or Varangians, ignoring\n");
			externalNotificationModule->setExternalOff(0); // this will turn off all GPIO and sounds and idle the loop
			return ProcessMessage::STOP;
		}
#endif
#endif
#ifdef MONASTERY_FRIENDS
		if ((strcmp(channels.getName(mp.channel), "MFA") != 0) && (mp.to == 0xffffffff)) {
			LOG_INFO("Was Broadcast message, but Channel Name is not MFA, ignoring\n");
			externalNotificationModule->setExternalOff(0); // this will turn off all GPIO and sounds and idle the loop
			return ProcessMessage::STOP;
		}
#endif

    // We only store/display messages destined for us.
    // Keep a copy of the most recent text message.
#ifdef SIMPLE_TDECK
		if (mp.to == 0xffffffff) { // if was broadcast, add prefix to message
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
			// devicestate.rx_text_message = modifiedPacket;
			// devicestate.has_rx_text_message = true;
			// powerFSM.trigger(EVENT_RECEIVED_MSG);
			// notifyObservers(&modifiedPacket);
			// delete[] newBuffer; // Clean up
		} else { // was not broadcast
			devicestate.rx_text_message = mp;
			devicestate.has_rx_text_message = true;

			powerFSM.trigger(EVENT_RECEIVED_MSG);
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
