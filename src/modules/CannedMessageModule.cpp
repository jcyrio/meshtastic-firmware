#include "configuration.h"
#if ARCH_PORTDUINO
#include "PortduinoGlue.h"
#endif
#if HAS_SCREEN
#include "CannedMessageModule.h"
#include "Channels.h"
#include "FSCommon.h"
#include "MeshService.h"
#include "NodeDB.h"
#include "PowerFSM.h" // needed for button bypass
#include "detect/ScanI2C.h"
#include "mesh/generated/meshtastic/cannedmessages.pb.h"

#include "main.h"                               // for cardkb_found
#include "modules/ExternalNotificationModule.h" // for buzzer control
#if !MESHTASTIC_EXCLUDE_GPS
#include "GPS.h"
#endif
#ifdef SIMPLE_TDECK
// std::vector<std::string> skipNodes = {"", "Unknown Name", "C2OPS", "Athos", "Birdman", "RAMBO", "Broadcast", "Command Post", "APFD", "Friek", "Cross", "CHIP", "St. Anthony", "Monastery", "mqtt", "MQTTclient", "Tester"};
std::vector<std::string> skipNodes = {"", "Unknown Name", "C2OPS", "Athos", "Birdman", "RAMBO", "Broadcast", "Command Post", "APFD", "Friek", "Cross", "CHIP", "St. Anthony", "Monastery", "Gatehouse", "Well3"};
std::vector<unsigned int> nodeList = { 
	// 3664080480, //my tbeam supreme, broken
	// 1486348306,  //not sure
	2864386355,  //kitchen
	3014898611,  //bookstore
	3719082304, //router
	207089188,  //spare1
	4184751652, //spare2
	207141012,  //fr jerome
	4184738532, //spare6
	2864390690, //dcnmichael
	202935032, //fr evgeni
	3734369073, //frc techo
	2579205344, //fr theoktist
  667627820, //fr silouanos
  2579251804, // Geronda Paisios
	// BELOW FOR GERONDA ONLY
	// birdman is !e0d01b90, rambo is !e0d01c80, chip is !0c572010
 //  207036432, //chip
	// 3771734928, //birdman
	// 3771735168, //rambo
};
#endif

#ifndef INPUTBROKER_MATRIX_TYPE
#define INPUTBROKER_MATRIX_TYPE 0
#endif

#include "graphics/ScreenFonts.h"

// Remove Canned message screen if no action is taken for some milliseconds
#define INACTIVATE_AFTER_MS 20000

#define NODENUM_HYTEC 808611244
#define NODENUM_RPI5 3719082304
#define NODENUM_MONTDECK 4184738532
#define NODENUM_FRTH_TDECK 207089188

extern ScanI2C::DeviceAddress cardkb_found;

static const char *cannedMessagesConfigFile = "/prefs/cannedConf.proto";

meshtastic_CannedMessageModuleConfig cannedMessageModuleConfig;

CannedMessageModule *cannedMessageModule;

CannedMessageModule::CannedMessageModule()
    : SinglePortModule("canned", meshtastic_PortNum_TEXT_MESSAGE_APP), concurrency::OSThread("CannedMessageModule")
{
    if (moduleConfig.canned_message.enabled || CANNED_MESSAGE_MODULE_ENABLE) {
        this->loadProtoForModule();
        if ((this->splitConfiguredMessages() <= 0) && (cardkb_found.address == 0x00) && !INPUTBROKER_MATRIX_TYPE &&
            !CANNED_MESSAGE_MODULE_ENABLE) {
            LOG_INFO("CannedMessageModule: No messages are configured. Module is disabled\n");
            this->runState = CANNED_MESSAGE_RUN_STATE_DISABLED;
            disable();
        } else {
            LOG_INFO("CannedMessageModule is enabled\n");

            // T-Watch interface currently has no way to select destination type, so default to 'node'
#ifdef T_WATCH_S3
						//FRC TODO: might want to do the same for SIMPLE_TDECK, test sometime
            this->destSelect = CANNED_MESSAGE_DESTINATION_TYPE_NODE;
#endif

            this->inputObserver.observe(inputBroker);
        }
    } else {
        this->runState = CANNED_MESSAGE_RUN_STATE_DISABLED;
        disable();
    }
#ifdef SIMPLE_TDECK
		// LOG_INFO("Own node name: %s\n", cannedMessageModule->getNodeName(nodeDB->getNodeNum()));
		skipNodes.push_back(cannedMessageModule->getNodeName(nodeDB->getNodeNum()));
// FIXME: remove below later, doesn't do anything
		// char startupMessage[20];
		// snprintf(startupMessage, sizeof(startupMessage), "%s ON", cannedMessageModule->getNodeName(nodeDB->getNodeNum()));
		// sendText(NODENUM_RPI5, 1, startupMessage, false);
		nodeList.erase(std::remove(nodeList.begin(), nodeList.end(), nodeDB->getNodeNum()), nodeList.end());
		screen->showFirstBrightnessLevel();
#endif
}

/**
 * @brief Items in array this->messages will be set to be pointing on the right
 *     starting points of the string this->messageStore
 *
 * @return int Returns the number of messages found.
 */
// FIXME: This is just one set of messages now
int CannedMessageModule::splitConfiguredMessages()
{
    int messageIndex = 0;
    int i = 0;

    String messages = cannedMessageModuleConfig.messages;

#if defined(T_WATCH_S3) || defined(RAK14014)
    String separator = messages.length() ? "|" : "";

    messages = "[---- Free Text ----]" + separator + messages;
#endif

    // collect all the message parts
    strncpy(this->messageStore, messages.c_str(), sizeof(this->messageStore));

    // The first message points to the beginning of the store.
    this->messages[messageIndex++] = this->messageStore;
    int upTo = strlen(this->messageStore) - 1;

    while (i < upTo) {
        if (this->messageStore[i] == '|') {
            // Message ending found, replace it with string-end character.
            this->messageStore[i] = '\0';

            // hit our max messages, bail
            if (messageIndex >= CANNED_MESSAGE_MODULE_MESSAGE_MAX_COUNT) {
                this->messagesCount = messageIndex;
                return this->messagesCount;
            }

            // Next message starts after pipe (|) just found.
            this->messages[messageIndex++] = (this->messageStore + i + 1);
        }
        i += 1;
    }
    if (strlen(this->messages[messageIndex - 1]) > 0) {
        // We have a last message.
        LOG_DEBUG("CannedMessage %d is: '%s'\n", messageIndex - 1, this->messages[messageIndex - 1]);
        this->messagesCount = messageIndex;
    } else {
        this->messagesCount = messageIndex - 1;
    }

    return this->messagesCount;
}

int CannedMessageModule::handleInputEvent(const InputEvent *event)
{
    if ((strlen(moduleConfig.canned_message.allow_input_source) > 0) &&
        (strcasecmp(moduleConfig.canned_message.allow_input_source, event->source) != 0) &&
        (strcasecmp(moduleConfig.canned_message.allow_input_source, "_any") != 0)) {
        // Event source is not accepted.
        // Event only accepted if source matches the configured one, or
        //   the configured one is "_any" (or if there is no configured
        //   source at all)
        return 0;
    }
    if (this->runState == CANNED_MESSAGE_RUN_STATE_SENDING_ACTIVE) {
        return 0; // Ignore input while sending
    }
#ifdef SIMPLE_TDECK
    if ((this->runState == CANNED_MESSAGE_RUN_STATE_REQUEST_PREVIOUS_ACTIVE) && (this->previousMessageIndex > 0)) {
        return 0; // Ignore input while sending
    }
#endif
    bool validEvent = false;
#ifdef SIMPLE_TDECK
		if ((this->runState != CANNED_MESSAGE_RUN_STATE_FREETEXT) &&
				((event->inputEvent == static_cast<char>(meshtastic_ModuleConfig_CannedMessageConfig_InputEventChar_UP)) || ((event->inputEvent == static_cast<char>(meshtastic_ModuleConfig_CannedMessageConfig_InputEventChar_DOWN)) && (this->previousMessageIndex > 0)))) {
			if (this->lastTrackballMillis + 10000 > millis()) {
				LOG_INFO("GOT HERE, ALLOWING TRACKBALL BECAUSE ITS BEEN 10 SECONDS\n");
				this->lastTrackballMillis = millis();
			if (event->inputEvent == static_cast<char>(meshtastic_ModuleConfig_CannedMessageConfig_InputEventChar_UP)) {
				this->previousMessageIndex++;
			} else this->previousMessageIndex--;
			LOG_DEBUG("Previous message index: %d\n", this->previousMessageIndex);
			this->runState = CANNED_MESSAGE_RUN_STATE_PREVIOUS_MSG;
        // UIFrameEvent e = {false, true};
        // e.frameChanged = true;
				UIFrameEvent e;
				e.action = UIFrameEvent::Action::REGENERATE_FRAMESET; // We want to change the list of frames shown on-screen
        this->currentMessageIndex = -1;
        this->freetext = ""; // clear freetext
        this->cursor = 0;
        this->notifyObservers(&e);
			validEvent = true;
			} // end trackballEnabled
}
#endif
    if (event->inputEvent == static_cast<char>(meshtastic_ModuleConfig_CannedMessageConfig_InputEventChar_UP)) {
        if (this->messagesCount > 0) {
            this->runState = CANNED_MESSAGE_RUN_STATE_ACTION_UP;
            validEvent = true;
        }
    }
    if (event->inputEvent == static_cast<char>(meshtastic_ModuleConfig_CannedMessageConfig_InputEventChar_DOWN)) {
        if (this->messagesCount > 0) {
            this->runState = CANNED_MESSAGE_RUN_STATE_ACTION_DOWN;
            validEvent = true;
        }
    }
    if (event->inputEvent == static_cast<char>(meshtastic_ModuleConfig_CannedMessageConfig_InputEventChar_SELECT)) {

#if defined(T_WATCH_S3) || defined(RAK14014)
        if (this->currentMessageIndex == 0) {
            this->runState = CANNED_MESSAGE_RUN_STATE_FREETEXT;

            requestFocus(); // Tell Screen::setFrames to move to our module's frame, next time it runs
            UIFrameEvent e;
            e.action = UIFrameEvent::Action::REGENERATE_FRAMESET; // We want to change the list of frames shown on-screen
            this->notifyObservers(&e);

            return 0;
        }
#endif

        // when inactive, call the onebutton shortpress instead. Activate Module only on up/down
        if ((this->runState == CANNED_MESSAGE_RUN_STATE_INACTIVE) || (this->runState == CANNED_MESSAGE_RUN_STATE_DISABLED)) {
            powerFSM.trigger(EVENT_PRESS);
        } else {
            this->payload = this->runState;
            this->runState = CANNED_MESSAGE_RUN_STATE_ACTION_SELECT;
            validEvent = true;
        }
    }
    if (event->inputEvent == static_cast<char>(meshtastic_ModuleConfig_CannedMessageConfig_InputEventChar_CANCEL)) {
        UIFrameEvent e;
        e.action = UIFrameEvent::Action::REGENERATE_FRAMESET; // We want to change the list of frames shown on-screen
        this->currentMessageIndex = -1;

#if !defined(T_WATCH_S3) && !defined(RAK14014)
        this->freetext = ""; // clear freetext
        this->cursor = 0;
        this->destSelect = CANNED_MESSAGE_DESTINATION_TYPE_NONE;
#endif

        this->runState = CANNED_MESSAGE_RUN_STATE_INACTIVE;
        this->notifyObservers(&e);
    }
    if ((event->inputEvent == static_cast<char>(meshtastic_ModuleConfig_CannedMessageConfig_InputEventChar_BACK)) ||
        (event->inputEvent == static_cast<char>(meshtastic_ModuleConfig_CannedMessageConfig_InputEventChar_LEFT)) ||
        (event->inputEvent == static_cast<char>(meshtastic_ModuleConfig_CannedMessageConfig_InputEventChar_RIGHT))) {

#if defined(T_WATCH_S3) || defined(RAK14014)
        if (event->inputEvent == static_cast<char>(meshtastic_ModuleConfig_CannedMessageConfig_InputEventChar_LEFT)) {
            this->payload = 0xb4;
        } else if (event->inputEvent == static_cast<char>(meshtastic_ModuleConfig_CannedMessageConfig_InputEventChar_RIGHT)) {
            this->payload = 0xb7;
        }
#else
        // tweak for left/right events generated via trackball/touch with empty kbchar
        if (!event->kbchar) {
            if (event->inputEvent == static_cast<char>(meshtastic_ModuleConfig_CannedMessageConfig_InputEventChar_LEFT)) {
                this->payload = 0xb4;
            } else if (event->inputEvent == static_cast<char>(meshtastic_ModuleConfig_CannedMessageConfig_InputEventChar_RIGHT)) {
                this->payload = 0xb7;
            }
        } else {
#ifdef SIMPLE_TDECK
            // pass the pressed key
					//FRC TEMP
					LOG_DEBUG("Canned message event (%x)\n", event->kbchar);
#endif
            this->payload = event->kbchar;
        }
#endif

        this->lastTouchMillis = millis();
        validEvent = true;
    }
#ifdef SIMPLE_TDECK
//FIXME: doesn't work. Trying to make it so that pressing trackball key goes to Router node in freetext
  //   if (event->inputEvent == static_cast<char>(meshtastic_ModuleConfig_CannedMessageConfig_InputEventChar_SELECT)) {
  //       if ((this->runState == CANNED_MESSAGE_RUN_STATE_INACTIVE) || (this->runState == CANNED_MESSAGE_RUN_STATE_ACTIVE)) {
  //           this->runState = CANNED_MESSAGE_RUN_STATE_FREETEXT;
		// 				this->dest = NODENUM_RPI5;
		// 				this->lastTouchMillis = millis();
		// 				this->freetext = ""; // clear freetext
		// 				this->cursor = 0;
		// 				validEvent = true;
		// 				UIFrameEvent e = {false, true};
		// 				e.frameChanged = true;
		// 	}
		// }
#endif
    if (event->inputEvent == static_cast<char>(ANYKEY)) {
#ifdef SIMPLE_TDECK
				// if (moduleConfig.external_notification.enabled && (externalNotificationModule->nagCycleCutoff != UINT32_MAX)) 
				// FIXME: later on try to make it detect if the LED is on first
					// externalNotificationModule->stopNow(); // this will turn off all GPIO and sounds and idle the loop
					externalNotificationModule->setExternalOff(0); // this will turn off all GPIO and sounds and idle the loop
#endif
        // when inactive, this will switch to the freetext mode
        if ((this->runState == CANNED_MESSAGE_RUN_STATE_INACTIVE) || (this->runState == CANNED_MESSAGE_RUN_STATE_ACTIVE) ||
            (this->runState == CANNED_MESSAGE_RUN_STATE_DISABLED)) {
					
#ifdef SIMPLE_TDECK
			if (this->skipNextFreetextMode == false) {
			// if (this->lastTrackballMillis + 10000 > millis()) { // this stops it from entering freetext mode if you're just pressing the 0-Mic key to enable the trackball scrolling
#endif
            this->runState = CANNED_MESSAGE_RUN_STATE_FREETEXT;
#ifdef SIMPLE_TDECK
			// }
			} else this->skipNextFreetextMode = false;
#endif
        }

        validEvent = false; // If key is normal than it will be set to true.

        // Run modifier key code below, (doesnt inturrupt typing or reset to start screen page)
        switch (event->kbchar) { //note: showTemporaryMessage doesn't work here
				// case 0xf: // alt-f, toggle flashlight
				// 	LOG_INFO("Got ALT-F, Flashlight toggle\n");
				// 	LOG_INFO("Got ALT-F, Flashlight toggle\n");
				// 	LOG_INFO("Got ALT-F, Flashlight toggle\n");
				// 	LOG_INFO("Got ALT-F, Flashlight toggle\n");
				// 	LOG_INFO("Got ALT-F, Flashlight toggle\n");
				// 	if (this->flashlightOn == 1) {
				// 		LOG_INFO("Flashlight off\n");
				// 		this->flashlightOn = 0;
				// 		externalNotificationModule->setExternalOff(0); // this will turn off all GPIO and sounds and idle the loop
				// 	} else {
				// 		LOG_INFO("Flashlight on\n");
				// 		this->flashlightOn = 1;
				// 		externalNotificationModule->setExternalOn(0); // this will turn off all GPIO and sounds and idle the loop
				// 	}
				// 	// this->runState = CANNED_MESSAGE_RUN_STATE_INACTIVE; //prevents entering freetext mode
				// 	this->skipNextFreetextMode = true;
				// 	this->runState = CANNED_MESSAGE_RUN_STATE_ACTION_SELECT; //this is what fixed the first screen going to freetext mode
				// 	delay(200); //debounce
				// 	break;
        case 0x11: // make screen brighter
            if (screen)
                screen->increaseBrightness();
            LOG_DEBUG("increasing Screen Brightness\n");
            break;
        case 0x12: // make screen dimmer
            if (screen)
                screen->decreaseBrightness();
            LOG_DEBUG("Decreasing Screen Brightness\n");
            break;
        case 0xf1: // draw modifier (function) symbal
            if (screen)
                screen->setFunctionSymbal("Fn");
            break;
        case 0xf2: // remove modifier (function) symbal
            if (screen)
                screen->removeFunctionSymbal("Fn");
            break;
        // mute (switch off/toggle) external notifications on fn+m
        case 0xac:
            if (moduleConfig.external_notification.enabled == true) {
                if (externalNotificationModule->getMute()) {
                    externalNotificationModule->setMute(false);
                    showTemporaryMessage("Notifications \nEnabled");
                    if (screen)
                        screen->removeFunctionSymbal("M"); // remove the mute symbol from the bottom right corner
                } else {
                    externalNotificationModule->stopNow(); // this will turn off all GPIO and sounds and idle the loop
                    externalNotificationModule->setMute(true);
                    externalNotificationModule->setExternalOff(0); // this will turn off the LED if it was on
                    showTemporaryMessage("Notifications \nDisabled");
                    if (screen)
                        screen->setFunctionSymbal("M"); // add the mute symbol to the bottom right corner
                }
            }
            break;
#ifndef SIMPLE_TDECK  //FRC used 9e for alt-r resend last message
        case 0x9e: // toggle GPS like triple press does
#if !MESHTASTIC_EXCLUDE_GPS
            if (gps != nullptr) {
                gps->toggleGpsMode();
            }
            if (screen)
                screen->forceDisplay();
            showTemporaryMessage("GPS Toggled");
#endif
            break;
#endif
        case 0xaf: // fn+space send network ping like double press does
            service.refreshLocalMeshNode();
            if (service.trySendPosition(NODENUM_BROADCAST, true)) {
                showTemporaryMessage("Position \nUpdate Sent");
            } else {
                showTemporaryMessage("Node Info \nUpdate Sent");
            }
            break;
#ifdef SIMPLE_TDECK
				case 0x24:  // $ sign
					if (moduleConfig.external_notification.enabled == true) {
							if (externalNotificationModule->getMute()) {
									externalNotificationModule->setMute(false);
									showTemporaryMessage("Notifications \nEnabled");
									if (screen)
											screen->removeFunctionSymbal("M"); // remove the mute symbol from the bottom right corner
							} else {
									externalNotificationModule->stopNow(); // this will turn off all GPIO and sounds and idle the loop
									externalNotificationModule->setMute(true);
									showTemporaryMessage("Notifications \nDisabled");
									if (screen)
											screen->setFunctionSymbal("M"); // add the mute symbol to the bottom right corner
							}
					}
						requestFocus();
				// 		// runOnce();
				break;
				// case 0x7e: // 0-mic key, press to enable trackball scrolling for next 10 seconds
				// 					 // NOTE: this one supersedes the other one that deletes the line. Maybe remove that case
				// 	LOG_INFO("RunState: %d\n", this->runState);
				// 	if (this->runState != CANNED_MESSAGE_RUN_STATE_FREETEXT) {
				// 		LOG_INFO("Trackball enabled for next 10 seconds\n");
				// 		this->lastTrackballMillis = millis();
    //         this->lastTouchMillis = millis();
    //         this->payload = event->kbchar;
				// 		this->skipNextFreetextMode = true;
				// 		this->runState = CANNED_MESSAGE_RUN_STATE_ACTION_SELECT; //this is what fixed the first screen going to freetext mode
    //         validEvent = true;
				// 	} else {  // if in freetext mode, delete / clear the line
				// 		LOG_INFO("Was in freetext mode, deleting line\n");
				// 		this->freetext = ""; // clear freetext
				// 		// this->notifyObservers(&e);
				// 		// this->freetext = this->freetext.substring(1);
				// 		this->cursor = 0;
				// 		this->freetext = this->freetext.substring(0, this->freetext.length() - 1);
    //         // validEvent = true;
				// 		requestFocus();
				// 		// runOnce();
				// 	}
				// 	break;
#endif
        default:
            // pass the pressed key
            LOG_DEBUG("Canned message ANYKEY (%x)\n", event->kbchar);
            LOG_DEBUG("Canned message ANYKEY (%x)\n", event->kbchar);
            LOG_DEBUG("Canned message ANYKEY (%x)\n", event->kbchar);
            LOG_DEBUG("Canned message ANYKEY (%x)\n", event->kbchar);
            LOG_DEBUG("Canned message ANYKEY (%x)\n", event->kbchar);
            LOG_DEBUG("Canned message ANYKEY (%x)\n", event->kbchar);
            LOG_DEBUG("Canned message ANYKEY (%x)\n", event->kbchar);
            LOG_DEBUG("Canned message ANYKEY (%x)\n", event->kbchar);
            LOG_DEBUG("Canned message ANYKEY (%x)\n", event->kbchar);
            LOG_DEBUG("Canned message ANYKEY (%x)\n", event->kbchar);
            LOG_DEBUG("Canned message ANYKEY (%x)\n", event->kbchar);
            LOG_DEBUG("Canned message ANYKEY (%x)\n", event->kbchar);
						// want to display value of skipNextFreetextMode
						LOG_INFO("skipNextFreetextMode: %d\n", this->skipNextFreetextMode);
            this->payload = event->kbchar;
            this->lastTouchMillis = millis();
            validEvent = true;
            break;
        }
        if (screen && (event->kbchar != 0xf1)) {
            screen->removeFunctionSymbal("Fn"); // remove modifier (function) symbal
        }
    }

#if defined(T_WATCH_S3) || defined(RAK14014)
    if (this->runState == CANNED_MESSAGE_RUN_STATE_FREETEXT) {
        String keyTapped = keyForCoordinates(event->touchX, event->touchY);

        if (keyTapped == "⇧") {
            this->highlight = -1;

            this->payload = 0x00;

            validEvent = true;

            this->shift = !this->shift;
        } else if (keyTapped == "⌫") {
            this->highlight = keyTapped[0];

            this->payload = 0x08;

            validEvent = true;

            this->shift = false;
        } else if (keyTapped == "123" || keyTapped == "ABC") {
            this->highlight = -1;

            this->payload = 0x00;

            this->charSet = this->charSet == 0 ? 1 : 0;

            validEvent = true;
        } else if (keyTapped == " ") {
            this->highlight = keyTapped[0];

            this->payload = keyTapped[0];

            validEvent = true;

            this->shift = false;
        } else if (keyTapped == "↵") {
            this->highlight = 0x00;

            this->runState = CANNED_MESSAGE_RUN_STATE_ACTION_SELECT;

            this->payload = CANNED_MESSAGE_RUN_STATE_FREETEXT;

            this->currentMessageIndex = event->kbchar - 1;

            validEvent = true;

            this->shift = false;
        } else if (keyTapped != "") {
            this->highlight = keyTapped[0];

            this->payload = this->shift ? keyTapped[0] : std::tolower(keyTapped[0]);

            validEvent = true;

            this->shift = false;
        }
    }
#endif

    if (event->inputEvent == static_cast<char>(MATRIXKEY)) {
        // this will send the text immediately on matrix press
        this->runState = CANNED_MESSAGE_RUN_STATE_ACTION_SELECT;
        this->payload = MATRIXKEY;
        this->currentMessageIndex = event->kbchar - 1;
        this->lastTouchMillis = millis();
        validEvent = true;
    }

    if (validEvent) {
        requestFocus(); // Tell Screen::setFrames to move to our module's frame, next time it runs

        // Let runOnce to be called immediately.
        if (this->runState == CANNED_MESSAGE_RUN_STATE_ACTION_SELECT) {
            setIntervalFromNow(0); // on fast keypresses, this isn't fast enough.
#ifdef SIMPLE_TDECK
				} else if ((this->runState == CANNED_MESSAGE_RUN_STATE_PREVIOUS_MSG) && (this->previousMessageIndex > 0)) {
					this->dontACK = 1;
					setIntervalFromNow(1300);
#endif
        } else {
					this->dontACK = 0;
            runOnce();
        }
    }

    return 0;
}

void CannedMessageModule::sendText(NodeNum dest, ChannelIndex channel, const char *message, bool wantReplies)
{
    meshtastic_MeshPacket *p = allocDataPacket();
    p->to = dest;
    p->channel = channel;
    p->want_ack = true;
// add totalMessagesSent to beginning of message
#ifdef SIMPLE_TDECK
		this->totalMessagesSent++;
		LOG_INFO("Total messages sent: %d\n", this->totalMessagesSent);
		// below was for enabling number count before every message, for testing
		// char totalMessagesSent[8];
		// memset(totalMessagesSent, 0, sizeof(totalMessagesSent)); // clear the string, first send has junk data
		// sprintf(totalMessagesSent, "%d] ", this->totalMessagesSent);
		// char newMessage[strlen(totalMessagesSent) + strlen(message) + 1];
		// strcpy(newMessage, totalMessagesSent);
		// strcat(newMessage, message);
		// p->decoded.payload.size = strlen(newMessage);
		// memcpy(p->decoded.payload.bytes, newMessage, p->decoded.payload.size);
#endif
    p->decoded.payload.size = strlen(message);
    memcpy(p->decoded.payload.bytes, message, p->decoded.payload.size);
    if (moduleConfig.canned_message.send_bell && p->decoded.payload.size < meshtastic_Constants_DATA_PAYLOAD_LEN) {
        p->decoded.payload.bytes[p->decoded.payload.size] = 7;        // Bell character
        p->decoded.payload.bytes[p->decoded.payload.size + 1] = '\0'; // Bell character
        p->decoded.payload.size++;
    }

    // Only receive routing messages when expecting ACK for a canned message
    // Prevents the canned message module from regenerating the screen's frameset at unexpected times,
    // or raising a UIFrameEvent before another module has the chance
    this->waitingForAck = true;

    LOG_INFO("Sending message id=%d, dest=%x, msg=%.*s\n", p->id, p->to, p->decoded.payload.size, p->decoded.payload.bytes);

    service.sendToMesh(
        p, RX_SRC_LOCAL,
        true); // send to mesh, cc to phone. Even if there's no phone connected, this stores the message to match ACKs
}

int32_t CannedMessageModule::runOnce()
{
#ifdef SIMPLE_TDECK
	if (alreadySentFirstMessage == 0) {
		char startupMessage[20];
		snprintf(startupMessage, sizeof(startupMessage), "%s ON", cannedMessageModule->getNodeName(nodeDB->getNodeNum()));
		sendText(NODENUM_RPI5, 1, startupMessage, false);
		alreadySentFirstMessage = 1;
	}
#endif
    if (((!moduleConfig.canned_message.enabled) && !CANNED_MESSAGE_MODULE_ENABLE) ||
        (this->runState == CANNED_MESSAGE_RUN_STATE_DISABLED) || (this->runState == CANNED_MESSAGE_RUN_STATE_INACTIVE)) {
        temporaryMessage = "";
        return INT32_MAX;
    }
    // LOG_DEBUG("Check status\n");
    UIFrameEvent e;
    if ((this->runState == CANNED_MESSAGE_RUN_STATE_SENDING_ACTIVE) ||
        (this->runState == CANNED_MESSAGE_RUN_STATE_ACK_NACK_RECEIVED) || (this->runState == CANNED_MESSAGE_RUN_STATE_MESSAGE)) {
        // TODO: might have some feedback of sending state
        this->runState = CANNED_MESSAGE_RUN_STATE_INACTIVE;
        temporaryMessage = "";
        e.action = UIFrameEvent::Action::REGENERATE_FRAMESET; // We want to change the list of frames shown on-screen
        this->currentMessageIndex = -1;
        this->freetext = ""; // clear freetext
        this->cursor = 0;

#if !defined(T_WATCH_S3) && !defined(RAK14014)
        this->destSelect = CANNED_MESSAGE_DESTINATION_TYPE_NONE;
#endif

        this->notifyObservers(&e);
#ifdef SIMPLE_TDECK
		} else if (this->runState == CANNED_MESSAGE_RUN_STATE_PREVIOUS_MSG) {
		if (this->previousMessageIndex == 0) {
        // UIFrameEvent e = {false, true};
        // e.frameChanged = true;
				requestFocus(); // Tell Screen::setFrames to move to our module's frame, next time it runs
				UIFrameEvent e;
				e.action = UIFrameEvent::Action::REGENERATE_FRAMESET; // We want to change the list of frames shown on-screen
        this->currentMessageIndex = -1;
        this->freetext = ""; // clear freetext
        this->cursor = 0;
        this->destSelect = CANNED_MESSAGE_DESTINATION_TYPE_NONE;
        this->runState = CANNED_MESSAGE_RUN_STATE_INACTIVE;
        this->notifyObservers(&e);
		} else {
	LOG_DEBUG("** Previous message index: %d\n", this->previousMessageIndex);
	LOG_DEBUG("** processing message\n");
	// TODO: previously this had just frameChanged = true, not sure if need requestFocus and uiframevent
				requestFocus(); // Tell Screen::setFrames to move to our module's frame, next time it runs
				UIFrameEvent e;
				e.action = UIFrameEvent::Action::REGENERATE_FRAMESET; // We want to change the list of frames shown on-screen
        // e.frameChanged = true;
        this->currentMessageIndex = -1;
        this->freetext = ""; // clear freetext
        this->cursor = 0;
        this->destSelect = CANNED_MESSAGE_DESTINATION_TYPE_NONE;
				this->runState = CANNED_MESSAGE_RUN_STATE_REQUEST_PREVIOUS_ACTIVE;
        this->notifyObservers(&e);
	char str[6];
	sprintf(str, "%d", this->previousMessageIndex);
		sendText(NODENUM_RPI5, 1, str, false);
		this->previousMessageIndex = 0;
		}
#endif
    } else if (((this->runState == CANNED_MESSAGE_RUN_STATE_ACTIVE) || (this->runState == CANNED_MESSAGE_RUN_STATE_FREETEXT)) &&
               ((millis() - this->lastTouchMillis) > INACTIVATE_AFTER_MS)) {
        // Reset module
        e.action = UIFrameEvent::Action::REGENERATE_FRAMESET; // We want to change the list of frames shown on-screen
        this->currentMessageIndex = -1;
        this->freetext = ""; // clear freetext
        this->cursor = 0;

#if !defined(T_WATCH_S3) && !defined(RAK14014)
        this->destSelect = CANNED_MESSAGE_DESTINATION_TYPE_NONE;
#endif

        this->runState = CANNED_MESSAGE_RUN_STATE_INACTIVE;
        this->notifyObservers(&e);
    } else if (this->runState == CANNED_MESSAGE_RUN_STATE_ACTION_SELECT) {
        if (this->payload == CANNED_MESSAGE_RUN_STATE_FREETEXT) {
            if (this->freetext.length() > 0) {
#ifdef SIMPLE_TDECK
							// check if freetext equals "rr" or "RR" to resend last message
							if (this->freetext == "rr" || this->freetext == "RR") {
								if (this->previousFreetext.length() > 0) {
									if (this->previousDest == NODENUM_BROADCAST) this->previousDest = NODENUM_RPI5;
									LOG_DEBUG("Resending previous message to %x: %s\n", this->previousDest, this->previousFreetext.c_str());
									sendText(this->previousDest, 1, this->previousFreetext.c_str(), true);
									showTemporaryMessage("Resending last message");
								} else {
									showTemporaryMessage("No previous \nmessage to resend");
								}
							} else {


							//if there is a leading '$' char at the start, then remove it
							// if (this->freetext[0] == '$') {
							// if (this->freetext[0] == '>') {
							// 	this->freetext = this->freetext.substring(1);
							// }
							// always goes to St Anthony's channel
							// prevent all broadcast, go just to router node
							if (this->dest == NODENUM_BROADCAST) { //for some reason the first message, without any side scrolling, defaults to NODENUM_BROADCAST. Afterwards it's fine, or after scrolling
								LOG_DEBUG("WAS BRODCAST\n");
								this->dest = NODENUM_RPI5;
							}
							sendText(this->dest, 1, this->freetext.c_str(), true);
							LOG_DEBUG("Sending message to %x: %s\n", this->dest, this->freetext.c_str());
							this->previousDest = this->dest;
							this->previousFreetext = this->freetext;
							LOG_INFO("previousDest: %x, previousFreetext: %s\n", this->previousDest, this->previousFreetext.c_str());
							LOG_INFO("dest: %x, freetext: %s\n", this->dest, this->freetext.c_str());
							}
#else
                sendText(this->dest, indexChannels[this->channel], this->freetext.c_str(), true);
#endif
// #endif
                this->runState = CANNED_MESSAGE_RUN_STATE_SENDING_ACTIVE;
            } else {
                this->runState = CANNED_MESSAGE_RUN_STATE_INACTIVE;
            }
        } else {
            if ((this->messagesCount > this->currentMessageIndex) && (strlen(this->messages[this->currentMessageIndex]) > 0)) {
                if (strcmp(this->messages[this->currentMessageIndex], "~") == 0) {
                    powerFSM.trigger(EVENT_PRESS);
                    return INT32_MAX;
                } else {
#if defined(T_WATCH_S3) || defined(RAK14014)
                    sendText(this->dest, indexChannels[this->channel], this->messages[this->currentMessageIndex], true);
#else
#ifdef SIMPLE_TDECK
										// always goes to St Anthony's channel (not sure what it's sending here though)
                    // sendText(NODENUM_BROADCAST, 1, this->messages[this->currentMessageIndex], true);
										// new 6-17-24, trying make sure never send to broadcast, saw some StA's messages in app
                    sendText(NODENUM_RPI5, 1, this->messages[this->currentMessageIndex], true);
#else
                    sendText(NODENUM_BROADCAST, channels.getPrimaryIndex(), this->messages[this->currentMessageIndex], true);
#endif
#endif
                }
                this->runState = CANNED_MESSAGE_RUN_STATE_SENDING_ACTIVE;
            } else {
                // LOG_DEBUG("Reset message is empty.\n");
                this->runState = CANNED_MESSAGE_RUN_STATE_INACTIVE;
            }
        }
        e.action = UIFrameEvent::Action::REGENERATE_FRAMESET; // We want to change the list of frames shown on-screen
        this->currentMessageIndex = -1;
        this->freetext = ""; // clear freetext
        this->cursor = 0;

#if !defined(T_WATCH_S3) && !defined(RAK14014)
        this->destSelect = CANNED_MESSAGE_DESTINATION_TYPE_NONE;
#endif

        this->notifyObservers(&e);
        return 2000;
    } else if ((this->runState != CANNED_MESSAGE_RUN_STATE_FREETEXT) && (this->currentMessageIndex == -1)) {
        this->currentMessageIndex = 0;
        LOG_DEBUG("First touch (%d):%s\n", this->currentMessageIndex, this->getCurrentMessage());
        e.action = UIFrameEvent::Action::REGENERATE_FRAMESET; // We want to change the list of frames shown on-screen
        this->runState = CANNED_MESSAGE_RUN_STATE_ACTIVE;
    } else if (this->runState == CANNED_MESSAGE_RUN_STATE_ACTION_UP) {
        if (this->messagesCount > 0) {
            this->currentMessageIndex = getPrevIndex();
            this->freetext = ""; // clear freetext
            this->cursor = 0;

#if !defined(T_WATCH_S3) && !defined(RAK14014)
            this->destSelect = CANNED_MESSAGE_DESTINATION_TYPE_NONE;
#endif

            this->runState = CANNED_MESSAGE_RUN_STATE_ACTIVE;
            LOG_DEBUG("MOVE UP (%d):%s\n", this->currentMessageIndex, this->getCurrentMessage());
        }
    } else if (this->runState == CANNED_MESSAGE_RUN_STATE_ACTION_DOWN) {
        if (this->messagesCount > 0) {
            this->currentMessageIndex = this->getNextIndex();
            this->freetext = ""; // clear freetext
            this->cursor = 0;

#if !defined(T_WATCH_S3) && !defined(RAK14014)
            this->destSelect = CANNED_MESSAGE_DESTINATION_TYPE_NONE;
#endif

            this->runState = CANNED_MESSAGE_RUN_STATE_ACTIVE;
            LOG_DEBUG("MOVE DOWN (%d):%s\n", this->currentMessageIndex, this->getCurrentMessage());
        }
    } else if (this->runState == CANNED_MESSAGE_RUN_STATE_FREETEXT || this->runState == CANNED_MESSAGE_RUN_STATE_ACTIVE) {
        switch (this->payload) {
#ifdef SIMPLE_TDECK
				case 0x7e: // mic / 0 key, clear line
					// LOG_INFO("RunState: %d\n", this->runState);
					// if (this->runState != CANNED_MESSAGE_RUN_STATE_FREETEXT) {
					if (this->freetext.length() == 0) {
						LOG_INFO("Trackball enabled for next 10 seconds\n");
						this->lastTrackballMillis = millis();
            this->lastTouchMillis = millis();
            // this->payload = event->kbchar;
						this->skipNextFreetextMode = true;
						this->runState = CANNED_MESSAGE_RUN_STATE_ACTION_SELECT; //this is what fixed the first screen going to freetext mode
            // validEvent = true;
					} else {  // if in writing mode and more than 1 char written, delete / clear the line
						LOG_INFO("more than 1 char is on freetext line, deleting line\n");
						this->freetext = ""; // clear freetext
						// this->notifyObservers(&e);
						// this->freetext = this->freetext.substring(1);
						this->cursor = 0;
						this->freetext = this->freetext.substring(0, this->freetext.length() - 1);
            // validEvent = true;
						requestFocus();
						// runOnce();
					}
					// this->freetext = ""; // clear freetext
					// this->notifyObservers(&e);
					// // this->freetext = this->freetext.substring(1);
					// this->cursor = 0;
					// this->freetext = this->freetext.substring(0, this->freetext.length() - 1);
					// this->cursor--;
					// might want runOnce here
					break;
				case 0x1a: // alt-w/1, previous Messages1
					sendText(NODENUM_RPI5, 1, "1", false);
					showTemporaryMessage("Requesting Previous \nMessages 1");
					break;
				case 0x9f: // alt-e/2, previous Messages2
					sendText(NODENUM_RPI5, 1, "2", false);
					showTemporaryMessage("Requesting Previous \nMessages 2");
					break;
				case 0x9e: // alt-r, resend last message
					LOG_INFO("Got ALT-R, Resend last message\n");
					LOG_INFO("Got ALT-R, Resend last message\n");
					if (this->previousFreetext.length() > 0) {
						if (this->previousDest == NODENUM_BROADCAST) this->previousDest = NODENUM_RPI5;
						LOG_INFO("Resending previous message to %d: %s\n", this->previousDest, this->previousFreetext.c_str());
						sendText(this->previousDest, 1, this->previousFreetext.c_str(), true);
						showTemporaryMessage("Resending last message");
					} else {
						showTemporaryMessage("No previous \nmessage to resend");
					}
					// this->runState = CANNED_MESSAGE_RUN_STATE_INACTIVE; //prevents entering freetext mode
					// this->skipNextFreetextMode = true;
					// delay(100); //debounce
					break;
				case 0x7a: // z, clear LED when there's a notification
				case 0x78: // x, clear LED when there's a notification
					if (this->freetext.length() == 0) {
						LOG_INFO("Got Z or X, Clear LED\n");
						externalNotificationModule->setExternalOff(0); // this will turn off all GPIO and sounds and idle the loop
						this->flashlightOn = 0;
						this->runState = CANNED_MESSAGE_RUN_STATE_INACTIVE; //prevents entering freetext mode
						this->skipNextFreetextMode = true;
					}
					break;
				case 0x1f: // alt-f, toggle flashlight
					LOG_INFO("Got ALT-F, Flashlight toggle\n");
					LOG_INFO("Got ALT-F, Flashlight toggle\n");
					LOG_INFO("Got ALT-F, Flashlight toggle\n");
					if (this->flashlightOn == 1) {
						LOG_INFO("Flashlight off\n");
						this->flashlightOn = 0;
						externalNotificationModule->setExternalOff(0); // this will turn off all GPIO and sounds and idle the loop
					} else {
						LOG_INFO("Flashlight on\n");
						this->flashlightOn = 1;
						externalNotificationModule->setExternalOn(0); // this will turn off all GPIO and sounds and idle the loop
					}
					// this->runState = CANNED_MESSAGE_RUN_STATE_INACTIVE; //prevents entering freetext mode
					// this->skipNextFreetextMode = true;
					// validEvent = true;
					// this->runState = CANNED_MESSAGE_RUN_STATE_ACTION_SELECT; //this is what fixed the first screen going to freetext mode
					// delay(500); //debounce
					break;
				case 0x5f: // _, cursorScrollMode
					//toggle cursor scroll mode
					if (this->cursorScrollMode == 0) {
						this->cursorScrollMode = 1;
						screen->setFunctionSymbal("S"); // add the S symbol to the bottom right corner
					} else {
						this->cursorScrollMode = 0;
                        this->cursor = this->freetext.length();
						screen->removeFunctionSymbal("S"); // remove the S symbol from the bottom right corner
					}
					break;
        case 0x1e: // shift-$, toggle brightness
        case 0x3c: // shift-speaker toggle brightness, some newer tdecks black keyboards
        case 0x3e: // > sign
        case 0x04: // > sign, at least on newest tdecks with black trackballs
									 // NOTE: there is a delay here. Might want to fix in future
					screen->increaseBrightness();
					LOG_INFO("Brightness increased\n");
					// this->skipNextFreetextMode = true; //this caused a big delay in setting brightness
					this->runState = CANNED_MESSAGE_RUN_STATE_INACTIVE;  //prevents entering freetext mode
					break;
     //    case 0x24: // $ sign
     //    // case 0x20: // speaker sign, some tdecks with newer keyboards
					// if (moduleConfig.external_notification.enabled == true) {
					// 		if (externalNotificationModule->getMute()) {
					// 				externalNotificationModule->setMute(false);
					// 				showTemporaryMessage("Notifications \nEnabled");
					// 				if (screen)
					// 						screen->removeFunctionSymbal("M"); // remove the mute symbol from the bottom right corner
					// 		} else {
					// 				externalNotificationModule->stopNow(); // this will turn off all GPIO and sounds and idle the loop
					// 				externalNotificationModule->setMute(true);
					// 				showTemporaryMessage("Notifications \nDisabled");
					// 				if (screen)
					// 						screen->setFunctionSymbal("M"); // add the mute symbol to the bottom right corner
					// 		}
					// }
					// // if (this->destSelect == CANNED_MESSAGE_DESTINATION_TYPE_NODE) {
					// // 		this->destSelect = CANNED_MESSAGE_DESTINATION_TYPE_NONE;
					// // } else {
					// // 		this->destSelect = CANNED_MESSAGE_DESTINATION_TYPE_NODE;
					// // 		if (this->dest == NODENUM_BROADCAST) {
					// // 				this->dest = NODENUM_RPI5;
					// // 		}
					// // }
					// // note wasn't able to find out how to delete the $ sign. When I put backspace here it wasn't doing anything
					// // This does though remove subsequent characters. Only the first one isn't caught
					// // if (this->freetext.length() > 0) {
					// // 	this->freetext = this->freetext.substring(0, this->freetext.length() - 1);
					// // 	// this->freetext = "";
					// // 	this->cursor--;
					// // 	// this->notifyObservers(&e);
					// // }
					// break;
#endif
        case 0xb4: // left
#ifndef SIMPLE_TDECK
          if (this->destSelect == CANNED_MESSAGE_DESTINATION_TYPE_NODE) {
#else  // this always allows to change the destination with scrolling
          if (1 == 1) {
#endif
                size_t numMeshNodes = nodeDB->getNumMeshNodes();
                if (this->dest == NODENUM_BROADCAST) {
                    this->dest = nodeDB->getNodeNum();
                }
#ifndef SIMPLE_TDECK
                for (unsigned int i = 0; i < numMeshNodes; i++) {
                    if (nodeDB->getMeshNodeByIndex(i)->num == this->dest) {
                        this->dest =
                            (i > 0) ? nodeDB->getMeshNodeByIndex(i - 1)->num : nodeDB->getMeshNodeByIndex(numMeshNodes - 1)->num;
                        break;
                    }
                }
                if (this->dest == nodeDB->getNodeNum()) {
                    // this->dest = NODENUM_BROADCAST;
									//new test
										this->dest = NODENUM_RPI5;
                }
            } else if (this->destSelect == CANNED_MESSAGE_DESTINATION_TYPE_CHANNEL) {
                for (unsigned int i = 0; i < channels.getNumChannels(); i++) {
                    if ((channels.getByIndex(i).role == meshtastic_Channel_Role_SECONDARY) ||
                        (channels.getByIndex(i).role == meshtastic_Channel_Role_PRIMARY)) {
                        indexChannels[numChannels] = i;
                        numChannels++;
                    }
                }
                if (this->channel == 0) {
                    this->channel = numChannels - 1;
                } else {
                    this->channel--;
                }
#else  // SIMPLE_TDECK
           //      for (unsigned int i = 0; i < numMeshNodes; i++) {
           //          if (nodeDB->getMeshNodeByIndex(i)->num == this->dest) {
											// unsigned int nextNode = i;
											// const char* nodeName;
											// do {
											// 	nextNode = (nextNode > 0) ? nextNode - 1 : numMeshNodes - 1;
											// 	nodeName = cannedMessageModule->getNodeName(nodeDB->getMeshNodeByIndex(nextNode)->num);
											// } while (std::find(skipNodes.begin(), skipNodes.end(), nodeName) != skipNodes.end());
											// this->dest = nodeDB->getMeshNodeByIndex(nextNode)->num;
											// // LOG_INFO("Own node name: %s\n", cannedMessageModule->getNodeName(nodeDB->getNodeNum()));
											// // LOG_INFO("Next node: %s\n", nodeName);
											// break;
           //          }
           //      }
								
						// nodeIndex = (nodeIndex + 1) % nodeList.size(); // Increment nodeIndex and wrap around
						// this->dest = nodeList[nodeIndex];
						if (this->cursorScrollMode == 0) {
                            LOG_INFO("CursorScrollMode is off\n");
							do {
                                LOG_INFO("CursorScrollMode is off, do while\n");
								nodeIndex = (nodeIndex + 1) % nodeList.size();
                                LOG_INFO("NodeIndex: %d\n", nodeIndex);
							} while (std::string(cannedMessageModule->getNodeName(nodeList[nodeIndex])) == "Unknown");
			// snprintf(startupMessage, sizeof(startupMessage), "%s ON", cannedMessageModule->getNodeName(nodeDB->getNodeNum()));
												// 		nodeName = cannedMessageModule->getNodeName(nodeDB->getMeshNodeByIndex(nextNode)->num);
                            LOG_INFO("NodeIndex: %d\n", nodeIndex);
							this->dest = nodeList[nodeIndex];
                            LOG_INFO("Dest: %d\n", this->dest);
							// nodeDB->getMeshNode(3664080480));
							// this->dest = nodeDB->getMeshNodeByIndex(nextNode)->num;
						} else {
                            LOG_INFO("CursorScrollMode is on\n");
                if (this->cursor > 0) {
                    this->cursor--;
                }
						}
								
#endif
            } else {
                if (this->cursor > 0) {
                    this->cursor--;
                }
            }
            break;
        case 0xb7: // right
#ifndef SIMPLE_TDECK
          if (this->destSelect == CANNED_MESSAGE_DESTINATION_TYPE_NODE) {
#else  // this always allows to change the destination with scrolling
          if (1 == 1) {
#endif
                size_t numMeshNodes = nodeDB->getNumMeshNodes();
                if (this->dest == NODENUM_BROADCAST) {
                    this->dest = nodeDB->getNodeNum();
                }
#ifndef SIMPLE_TDECK
                for (unsigned int i = 0; i < numMeshNodes; i++) {
                    if (nodeDB->getMeshNodeByIndex(i)->num == this->dest) {
                        this->dest =
                            (i < numMeshNodes - 1) ? nodeDB->getMeshNodeByIndex(i + 1)->num : nodeDB->getMeshNodeByIndex(0)->num;
                        break;
                    }
                }
                if (this->dest == nodeDB->getNodeNum()) {
                    this->dest = NODENUM_BROADCAST;
                }
            } else if (this->destSelect == CANNED_MESSAGE_DESTINATION_TYPE_CHANNEL) {
                for (unsigned int i = 0; i < channels.getNumChannels(); i++) {
                    if ((channels.getByIndex(i).role == meshtastic_Channel_Role_SECONDARY) ||
                        (channels.getByIndex(i).role == meshtastic_Channel_Role_PRIMARY)) {
                        indexChannels[numChannels] = i;
                        numChannels++;
                    }
                }
                if (this->channel == numChannels - 1) {
                    this->channel = 0;
                } else {
                    this->channel++;
                }
#else  // SIMPLE_TDECK
           //      for (unsigned int i = 0; i < numMeshNodes; i++) {
           //          if (nodeDB->getMeshNodeByIndex(i)->num == this->dest) {
											// unsigned int nextNode = i;
											// const char* nodeName;
											// do {
											// 		nextNode = (nextNode < numMeshNodes - 1) ? nextNode + 1 : 0;
											// 		nodeName = cannedMessageModule->getNodeName(nodeDB->getMeshNodeByIndex(nextNode)->num);
											// } while (std::find(skipNodes.begin(), skipNodes.end(), nodeName) != skipNodes.end());
											// this->dest = nodeDB->getMeshNodeByIndex(nextNode)->num;
											// LOG_INFO("Next node: %s\n", nodeName);
											// break;
           //          }
           //      }
						// nodeIndex = (nodeIndex - 1 + nodeList.size()) % nodeList.size(); // Decrement nodeIndex and wrap around
						// this->dest = nodeList[nodeIndex];
						if (this->cursorScrollMode == 0) {
                            LOG_INFO("CursorScrollMode is off\n");
							do {
                                LOG_INFO("CursorScrollMode is off, do while\n");
								nodeIndex = (nodeIndex - 1 + nodeList.size()) % nodeList.size(); // Decrement nodeIndex and wrap around
                                LOG_INFO("NodeIndex: %d\n", nodeIndex);
							} while (std::string(cannedMessageModule->getNodeName(nodeList[nodeIndex])) == "Unknown");
                            LOG_INFO("NodeIndex: %d\n", nodeIndex);
							this->dest = nodeList[nodeIndex];
                            LOG_INFO("Dest: %d\n", this->dest);
						} else {
                            LOG_INFO("CursorScrollMode is on\n");
								if (this->cursor < this->freetext.length()) {
										this->cursor++;
								}
						}
#endif
            } else {
                if (this->cursor < this->freetext.length()) {
                    this->cursor++;
                }
            }
            break;
        default:
            break;
        }
        if (this->runState == CANNED_MESSAGE_RUN_STATE_FREETEXT) {
            e.action = UIFrameEvent::Action::REGENERATE_FRAMESET; // We want to change the list of frames shown on-screen
            switch (this->payload) { // code below all trigger the freetext window (where you type to send a message) or reset the
                                     // display back to the default window
            case 0x08:               // backspace
                if (this->freetext.length() > 0 && this->highlight == 0x00) {
                    if (this->cursor == this->freetext.length()) {
                        this->freetext = this->freetext.substring(0, this->freetext.length() - 1);
                    } else {
                        this->freetext = this->freetext.substring(0, this->cursor - 1) +
                                         this->freetext.substring(this->cursor, this->freetext.length());
                    }
                    this->cursor--;
                }
                break;
            case 0x09: // tab
#ifndef SIMPLE_TDECK
                if (this->destSelect == CANNED_MESSAGE_DESTINATION_TYPE_CHANNEL) {
                    this->destSelect = CANNED_MESSAGE_DESTINATION_TYPE_NONE;
                } else if (this->destSelect == CANNED_MESSAGE_DESTINATION_TYPE_NODE) {
                    this->destSelect = CANNED_MESSAGE_DESTINATION_TYPE_CHANNEL;
                } else {
                    this->destSelect = CANNED_MESSAGE_DESTINATION_TYPE_NODE;
                }
#else // skip channels, only rotate through nodes
                if (this->destSelect == CANNED_MESSAGE_DESTINATION_TYPE_NODE) {
                    this->destSelect = CANNED_MESSAGE_DESTINATION_TYPE_NONE;
                } else {
                    this->destSelect = CANNED_MESSAGE_DESTINATION_TYPE_NODE;
                }
#endif
                break;
            case 0xb4: // left
            case 0xb7: // right
#ifdef SIMPLE_TDECK
						case 0x7e: // mic / 0 key, clear line
						case 0x1e: // shift-$, toggle brightness
						case 0x3c: // shift-speaker toggle brightness, some tdecks with black keyboards
						case 0x24: // $ sign
						case 0x1f: // alt-f, flashlight
						case 0x5f: // _, toggle cursorScrollMode
						case 0x1a: // alt-1, previous messages 1
						case 0x9f: // alt-2, previous messages 2
						case 0x9e: // alt-r, resend last message
						// case 0x20: // speaker sign (some tdecks, new)
						case 0x3e: // > sign
						case 0x04: // > sign, at least on newest tdecks with black trackball
#endif
                // already handled above
                break;
                // handle fn+s for shutdown
            case 0x9b:
                if (screen)
                    screen->startAlert("Shutting down...");
                shutdownAtMsec = millis() + DEFAULT_SHUTDOWN_SECONDS * 1000;
                runState = CANNED_MESSAGE_RUN_STATE_INACTIVE;
                break;
            // and fn+r for reboot
            case 0x90:
                if (screen)
                    screen->startAlert("Rebooting...");
                rebootAtMsec = millis() + DEFAULT_REBOOT_SECONDS * 1000;
                runState = CANNED_MESSAGE_RUN_STATE_INACTIVE;
                break;
            default:
                if (this->highlight != 0x00) {
                    break;
                }

                if (this->cursor == this->freetext.length()) {
                    this->freetext += this->payload;
                } else {
                    this->freetext =
                        this->freetext.substring(0, this->cursor) + this->payload + this->freetext.substring(this->cursor);
                }

                this->cursor += 1;

                uint16_t maxChars = meshtastic_Constants_DATA_PAYLOAD_LEN - (moduleConfig.canned_message.send_bell ? 1 : 0);
                if (this->freetext.length() > maxChars) {
                    this->cursor = maxChars;
                    this->freetext = this->freetext.substring(0, maxChars);
                }
                break;
            }
            if (screen)
                screen->removeFunctionSymbal("Fn");
        }

        this->lastTouchMillis = millis();
        this->notifyObservers(&e);
        return INACTIVATE_AFTER_MS;
    }

    if (this->runState == CANNED_MESSAGE_RUN_STATE_ACTIVE) {
        this->lastTouchMillis = millis();
        this->notifyObservers(&e);
        return INACTIVATE_AFTER_MS;
    }

    return INT32_MAX;
}

const char *CannedMessageModule::getCurrentMessage()
{
    return this->messages[this->currentMessageIndex];
}
const char *CannedMessageModule::getPrevMessage()
{
    return this->messages[this->getPrevIndex()];
}
const char *CannedMessageModule::getNextMessage()
{
    return this->messages[this->getNextIndex()];
}
const char *CannedMessageModule::getMessageByIndex(int index)
{
    return (index >= 0 && index < this->messagesCount) ? this->messages[index] : "";
}

const char *CannedMessageModule::getNodeName(NodeNum node)
{
    if (node == NODENUM_BROADCAST) {
#ifdef SIMPLE_TDECK
        return "Router";
#else
        return "Broadcast";
#endif
    } else {
        meshtastic_NodeInfoLite *info = nodeDB->getMeshNode(node);
        if (info != NULL) {
            return info->user.long_name;
        } else {
            return "Unknown";
        }
    }
}

bool CannedMessageModule::shouldDraw()
{
    if (!moduleConfig.canned_message.enabled && !CANNED_MESSAGE_MODULE_ENABLE) {
        return false;
    }
    return (currentMessageIndex != -1) || (this->runState != CANNED_MESSAGE_RUN_STATE_INACTIVE);
}

int CannedMessageModule::getNextIndex()
{
    if (this->currentMessageIndex >= (this->messagesCount - 1)) {
        return 0;
    } else {
        return this->currentMessageIndex + 1;
    }
}

int CannedMessageModule::getPrevIndex()
{
    if (this->currentMessageIndex <= 0) {
        return this->messagesCount - 1;
    } else {
        return this->currentMessageIndex - 1;
    }
}
void CannedMessageModule::showTemporaryMessage(const String &message)
{
    temporaryMessage = message;
		requestFocus(); // Tell Screen::setFrames to move to our module's frame, next time it runs
    UIFrameEvent e;
    e.action = UIFrameEvent::Action::REGENERATE_FRAMESET; // We want to change the list of frames shown on-screen
    notifyObservers(&e);
    runState = CANNED_MESSAGE_RUN_STATE_MESSAGE;
    // run this loop again in 2 seconds, next iteration will clear the display
    setIntervalFromNow(2000);
}

#if defined(T_WATCH_S3) || defined(RAK14014)

String CannedMessageModule::keyForCoordinates(uint x, uint y)
{
    int outerSize = *(&this->keyboard[this->charSet] + 1) - this->keyboard[this->charSet];

    for (int8_t outerIndex = 0; outerIndex < outerSize; outerIndex++) {
        int innerSize = *(&this->keyboard[this->charSet][outerIndex] + 1) - this->keyboard[this->charSet][outerIndex];

        for (int8_t innerIndex = 0; innerIndex < innerSize; innerIndex++) {
            Letter letter = this->keyboard[this->charSet][outerIndex][innerIndex];

            if (x > letter.rectX && x < (letter.rectX + letter.rectWidth) && y > letter.rectY &&
                y < (letter.rectY + letter.rectHeight)) {
                return letter.character;
            }
        }
    }

    return "";
}

void CannedMessageModule::drawKeyboard(OLEDDisplay *display, OLEDDisplayUiState *state, int16_t x, int16_t y)
{
    int outerSize = *(&this->keyboard[this->charSet] + 1) - this->keyboard[this->charSet];

    int xOffset = 0;

    int yOffset = 56;

    display->setTextAlignment(TEXT_ALIGN_LEFT);

    display->setFont(FONT_SMALL);

    display->setColor(OLEDDISPLAY_COLOR::WHITE);

    display->drawStringMaxWidth(0, 0, display->getWidth(),
                                cannedMessageModule->drawWithCursor(cannedMessageModule->freetext, cannedMessageModule->cursor));

    display->setFont(FONT_MEDIUM);

    int cellHeight = round((display->height() - 64) / outerSize);

    int yCorrection = 8;

    for (int8_t outerIndex = 0; outerIndex < outerSize; outerIndex++) {
        yOffset += outerIndex > 0 ? cellHeight : 0;

        int innerSizeBound = *(&this->keyboard[this->charSet][outerIndex] + 1) - this->keyboard[this->charSet][outerIndex];

        int innerSize = 0;

        for (int8_t innerIndex = 0; innerIndex < innerSizeBound; innerIndex++) {
            if (this->keyboard[this->charSet][outerIndex][innerIndex].character != "") {
                innerSize++;
            }
        }

        int cellWidth = display->width() / innerSize;

        for (int8_t innerIndex = 0; innerIndex < innerSize; innerIndex++) {
            xOffset += innerIndex > 0 ? cellWidth : 0;

            Letter letter = this->keyboard[this->charSet][outerIndex][innerIndex];

            Letter updatedLetter = {letter.character, letter.width, xOffset, yOffset, cellWidth, cellHeight};

            this->keyboard[this->charSet][outerIndex][innerIndex] = updatedLetter;

            float characterOffset = ((cellWidth / 2) - (letter.width / 2));

            if (letter.character == "⇧") {
                if (this->shift) {
                    display->fillRect(xOffset, yOffset, cellWidth, cellHeight);

                    display->setColor(OLEDDISPLAY_COLOR::BLACK);

                    drawShiftIcon(display, xOffset + characterOffset, yOffset + yCorrection + 5, 1.2);

                    display->setColor(OLEDDISPLAY_COLOR::WHITE);
                } else {
                    display->drawRect(xOffset, yOffset, cellWidth, cellHeight);

                    drawShiftIcon(display, xOffset + characterOffset, yOffset + yCorrection + 5, 1.2);
                }
            } else if (letter.character == "⌫") {
                if (this->highlight == letter.character[0]) {
                    display->fillRect(xOffset, yOffset, cellWidth, cellHeight);

                    display->setColor(OLEDDISPLAY_COLOR::BLACK);

                    drawBackspaceIcon(display, xOffset + characterOffset, yOffset + yCorrection + 5, 1.2);

                    display->setColor(OLEDDISPLAY_COLOR::WHITE);

                    setIntervalFromNow(0);
                } else {
                    display->drawRect(xOffset, yOffset, cellWidth, cellHeight);

                    drawBackspaceIcon(display, xOffset + characterOffset, yOffset + yCorrection + 5, 1.2);
                }
            } else if (letter.character == "↵") {
                display->drawRect(xOffset, yOffset, cellWidth, cellHeight);

                drawEnterIcon(display, xOffset + characterOffset, yOffset + yCorrection + 5, 1.7);
            } else {
                if (this->highlight == letter.character[0]) {
                    display->fillRect(xOffset, yOffset, cellWidth, cellHeight);

                    display->setColor(OLEDDISPLAY_COLOR::BLACK);

                    display->drawString(xOffset + characterOffset, yOffset + yCorrection,
                                        letter.character == " " ? "space" : letter.character);

                    display->setColor(OLEDDISPLAY_COLOR::WHITE);

                    setIntervalFromNow(0);
                } else {
                    display->drawRect(xOffset, yOffset, cellWidth, cellHeight);

                    display->drawString(xOffset + characterOffset, yOffset + yCorrection,
                                        letter.character == " " ? "space" : letter.character);
                }
            }
        }

        xOffset = 0;
    }

    this->highlight = 0x00;
}

void CannedMessageModule::drawShiftIcon(OLEDDisplay *display, int x, int y, float scale)
{
    PointStruct shiftIcon[10] = {{8, 0}, {15, 7}, {15, 8}, {12, 8}, {12, 12}, {4, 12}, {4, 8}, {1, 8}, {1, 7}, {8, 0}};

    int size = 10;

    for (int i = 0; i < size - 1; i++) {
        int x0 = x + (shiftIcon[i].x * scale);
        int y0 = y + (shiftIcon[i].y * scale);
        int x1 = x + (shiftIcon[i + 1].x * scale);
        int y1 = y + (shiftIcon[i + 1].y * scale);

        display->drawLine(x0, y0, x1, y1);
    }
}

void CannedMessageModule::drawBackspaceIcon(OLEDDisplay *display, int x, int y, float scale)
{
    PointStruct backspaceIcon[6] = {{0, 7}, {5, 2}, {15, 2}, {15, 12}, {5, 12}, {0, 7}};

    int size = 6;

    for (int i = 0; i < size - 1; i++) {
        int x0 = x + (backspaceIcon[i].x * scale);
        int y0 = y + (backspaceIcon[i].y * scale);
        int x1 = x + (backspaceIcon[i + 1].x * scale);
        int y1 = y + (backspaceIcon[i + 1].y * scale);

        display->drawLine(x0, y0, x1, y1);
    }

    PointStruct backspaceIconX[4] = {{7, 4}, {13, 10}, {7, 10}, {13, 4}};

    size = 4;

    for (int i = 0; i < size - 1; i++) {
        int x0 = x + (backspaceIconX[i].x * scale);
        int y0 = y + (backspaceIconX[i].y * scale);
        int x1 = x + (backspaceIconX[i + 1].x * scale);
        int y1 = y + (backspaceIconX[i + 1].y * scale);

        display->drawLine(x0, y0, x1, y1);
    }
}

void CannedMessageModule::drawEnterIcon(OLEDDisplay *display, int x, int y, float scale)
{
    PointStruct enterIcon[6] = {{0, 7}, {4, 3}, {4, 11}, {0, 7}, {15, 7}, {15, 0}};

    int size = 6;

    for (int i = 0; i < size - 1; i++) {
        int x0 = x + (enterIcon[i].x * scale);
        int y0 = y + (enterIcon[i].y * scale);
        int x1 = x + (enterIcon[i + 1].x * scale);
        int y1 = y + (enterIcon[i + 1].y * scale);

        display->drawLine(x0, y0, x1, y1);
    }
}

#endif

void CannedMessageModule::drawFrame(OLEDDisplay *display, OLEDDisplayUiState *state, int16_t x, int16_t y)
{
    char buffer[50];

    if (temporaryMessage.length() != 0) {
        requestFocus(); // Tell Screen::setFrames to move to our module's frame
        LOG_DEBUG("Drawing temporary message: %s", temporaryMessage.c_str());
        display->setTextAlignment(TEXT_ALIGN_CENTER);
        display->setFont(FONT_MEDIUM);
        display->drawString(display->getWidth() / 2 + x, 0 + y + 12, temporaryMessage);
    } else if (cannedMessageModule->runState == CANNED_MESSAGE_RUN_STATE_ACK_NACK_RECEIVED) {
        requestFocus(); // Tell Screen::setFrames to move to our module's frame
        display->setTextAlignment(TEXT_ALIGN_CENTER);
#ifdef SIMPLE_TDECK
        display->setFont(FONT_LARGE);
#else
        display->setFont(FONT_MEDIUM);
#endif
        String displayString;
#ifdef SIMPLE_TDECK
        if (this->ack) {
            displayString = "Delivered\n";
        } else {
            displayString = "Delivery failed";
        }
#else
        if (this->ack) {
            displayString = "Delivered to\n%s";
        } else {
            displayString = "Delivery failed\nto %s";
        }
#endif
// TODO: might want to allow the Delivery Failed msg if dontACK = true
#ifdef SIMPLE_TDECK
				if (this->dontACK == 0) { // I don't think this works
        display->drawStringf(display->getWidth() / 2 + x, 0 + y + 12 + FONT_HEIGHT_LARGE, buffer, displayString,
#else
        display->drawStringf(display->getWidth() / 2 + x, 0 + y + 12, buffer, displayString,
#endif
                             cannedMessageModule->getNodeName(this->incoming));
#ifndef SIMPLE_TDECK
        display->setFont(FONT_SMALL);

        String snrString = "Last Rx SNR: %f";
        String rssiString = "Last Rx RSSI: %d";

        if (this->ack) {
            display->drawStringf(display->getWidth() / 2 + x, y + 100, buffer, snrString, this->lastRxSnr);
            display->drawStringf(display->getWidth() / 2 + x, y + 130, buffer, rssiString, this->lastRxRssi);
        }
#endif
#ifdef SIMPLE_TDECK
				}
#endif
    } else if (cannedMessageModule->runState == CANNED_MESSAGE_RUN_STATE_SENDING_ACTIVE) {
        requestFocus(); // Tell Screen::setFrames to move to our module's frame
        display->setTextAlignment(TEXT_ALIGN_CENTER);
#ifdef SIMPLE_TDECK
        display->setFont(FONT_LARGE);
#else
        display->setFont(FONT_MEDIUM);
#endif
        display->drawString(display->getWidth() / 2 + x, 0 + y + 12 + (3 * FONT_HEIGHT_LARGE), "Sending...");
    }

#ifdef SIMPLE_TDECK
		else if (cannedMessageModule->runState == CANNED_MESSAGE_RUN_STATE_REQUEST_PREVIOUS_ACTIVE) {
        // display->setTextAlignment(TEXT_ALIGN_CENTER);
        // display->setFont(FONT_LARGE);
        // display->drawString(display->getWidth() / 2 + x, 0 + y + 12 + (3 * FONT_HEIGHT_LARGE), "Retrieving...");
				showTemporaryMessage("Retrieving...");
    }
		//TODO: should this be else if below? compare with orig
		//new 4-24-24 2:25
		else if ((cannedMessageModule->runState == CANNED_MESSAGE_RUN_STATE_PREVIOUS_MSG) && (this->previousMessageIndex != 0)) {
		display->setTextAlignment(TEXT_ALIGN_CENTER);
		display->setFont(FONT_LARGE);
		char msgBuffer1[32]; char msgBuffer2[32];
		snprintf(msgBuffer1, sizeof(msgBuffer1), "View previous");
		snprintf(msgBuffer2, sizeof(msgBuffer2), "message #%d", this->previousMessageIndex);
		display->drawString(display->getWidth() / 2 + x, display->getHeight() / 2 + y - FONT_HEIGHT_LARGE, msgBuffer1);
		display->drawString(display->getWidth() / 2 + x, display->getHeight() / 2 + y, msgBuffer2);
}
#endif



		else if (cannedMessageModule->runState == CANNED_MESSAGE_RUN_STATE_DISABLED) {
        display->setTextAlignment(TEXT_ALIGN_LEFT);
        display->setFont(FONT_SMALL);
        display->drawString(10 + x, 0 + y + FONT_HEIGHT_SMALL, "Canned Message\nModule disabled.");
    } else if (cannedMessageModule->runState == CANNED_MESSAGE_RUN_STATE_FREETEXT) {
        requestFocus(); // Tell Screen::setFrames to move to our module's frame
#if defined(T_WATCH_S3) || defined(RAK14014)
        drawKeyboard(display, state, 0, 0);
#else

        display->setTextAlignment(TEXT_ALIGN_LEFT);
#ifdef SIMPLE_TDECK
        display->setFont(FONT_LARGE);
#else
        display->setFont(FONT_SMALL);
#endif
        if (this->destSelect != CANNED_MESSAGE_DESTINATION_TYPE_NONE) {
#ifdef SIMPLE_TDECK
            display->fillRect(0 + x, 0 + y, x + display->getWidth(), y + FONT_HEIGHT_LARGE);
#else
            display->fillRect(0 + x, 0 + y, x + display->getWidth(), y + FONT_HEIGHT_SMALL);
#endif
            display->setColor(BLACK);
        }
        switch (this->destSelect) {
#ifdef SIMPLE_TDECK
        case CANNED_MESSAGE_DESTINATION_TYPE_NODE:
            display->drawStringf(1 + x, 0 + y, buffer, "To: %s", cannedMessageModule->getNodeName(this->dest));
            display->drawStringf(0 + x, 0 + y, buffer, "To: %s", cannedMessageModule->getNodeName(this->dest));
            break;
						//never gets here TODO: remove
        case CANNED_MESSAGE_DESTINATION_TYPE_CHANNEL:
            display->drawStringf(1 + x, 0 + y, buffer, "To: %s@>%s<", cannedMessageModule->getNodeName(this->dest),
                                 channels.getName(indexChannels[this->channel]));
            display->drawStringf(0 + x, 0 + y, buffer, "To: %s@>%s<", cannedMessageModule->getNodeName(this->dest),
                                 channels.getName(indexChannels[this->channel]));
            break;
#else
        case CANNED_MESSAGE_DESTINATION_TYPE_NODE:
            display->drawStringf(1 + x, 0 + y, buffer, "To: >%s<@%s", cannedMessageModule->getNodeName(this->dest),
                                 channels.getName(indexChannels[this->channel]));
            display->drawStringf(0 + x, 0 + y, buffer, "To: >%s<@%s", cannedMessageModule->getNodeName(this->dest),
                                 channels.getName(indexChannels[this->channel]));
            break;
        case CANNED_MESSAGE_DESTINATION_TYPE_CHANNEL:
            display->drawStringf(1 + x, 0 + y, buffer, "To: %s@>%s<", cannedMessageModule->getNodeName(this->dest),
                                 channels.getName(indexChannels[this->channel]));
            display->drawStringf(0 + x, 0 + y, buffer, "To: %s@>%s<", cannedMessageModule->getNodeName(this->dest),
                                 channels.getName(indexChannels[this->channel]));
            break;
#endif
        default:
            if (display->getWidth() > 128) {
#ifdef SIMPLE_TDECK
                display->drawStringf(0 + x, 0 + y, buffer, "To: %s", cannedMessageModule->getNodeName(this->dest));
                // display->drawStringf(0 + x, 0 + y, buffer, "Send Message:");
#else
                display->drawStringf(0 + x, 0 + y, buffer, "To: %s@%s", cannedMessageModule->getNodeName(this->dest),
                                     channels.getName(indexChannels[this->channel]));
#endif
            } else {
                display->drawStringf(0 + x, 0 + y, buffer, "To: %.5s@%.5s", cannedMessageModule->getNodeName(this->dest),
                                     channels.getName(indexChannels[this->channel]));
            }
            break;
        }
        // used chars right aligned, only when not editing the destination
        if (this->destSelect == CANNED_MESSAGE_DESTINATION_TYPE_NONE) {
            uint16_t charsLeft =
                meshtastic_Constants_DATA_PAYLOAD_LEN - this->freetext.length() - (moduleConfig.canned_message.send_bell ? 1 : 0);
            snprintf(buffer, sizeof(buffer), "%d left", charsLeft);
#ifdef SIMPLE_TDECK
            display->drawString(x + display->getWidth() - display->getStringWidth(buffer), y + 180, buffer);
#else
						display->drawString(x + display->getWidth() - display->getStringWidth(buffer), y + 0, buffer);
#endif
        }
        display->setColor(WHITE);
#ifdef SIMPLE_TDECK
				display->setFont(FONT_LARGE);
        display->drawStringMaxWidth(
            0 + x, 0 + y + FONT_HEIGHT_LARGE * 3, x + display->getWidth(),
            cannedMessageModule->drawWithCursor(cannedMessageModule->freetext, cannedMessageModule->cursor));
#else
        display->drawStringMaxWidth(
            0 + x, 0 + y + FONT_HEIGHT_SMALL, x + display->getWidth(),
            cannedMessageModule->drawWithCursor(cannedMessageModule->freetext, cannedMessageModule->cursor));
#endif
#endif
    } else {
        if (this->messagesCount > 0) {
            display->setTextAlignment(TEXT_ALIGN_LEFT);
#ifdef SIMPLE_TDECK
            display->setFont(FONT_LARGE);
            display->drawStringf(0 + x, 0 + y, buffer, "To: %s", cannedMessageModule->getNodeName(this->dest));
            int lines = (display->getHeight() / FONT_HEIGHT_LARGE) - 1;
#else
            display->setFont(FONT_SMALL);
            display->drawStringf(0 + x, 0 + y, buffer, "To: %s", cannedMessageModule->getNodeName(this->dest));
            int lines = (display->getHeight() / FONT_HEIGHT_SMALL) - 1;
#endif
            if (lines == 3) {
                // static (old) behavior for small displays
                display->drawString(0 + x, 0 + y + FONT_HEIGHT_SMALL, cannedMessageModule->getPrevMessage());
                display->fillRect(0 + x, 0 + y + FONT_HEIGHT_SMALL * 2, x + display->getWidth(), y + FONT_HEIGHT_SMALL);
                display->setColor(BLACK);
                display->drawString(0 + x, 0 + y + FONT_HEIGHT_SMALL * 2, cannedMessageModule->getCurrentMessage());
                display->setColor(WHITE);
                display->drawString(0 + x, 0 + y + FONT_HEIGHT_SMALL * 3, cannedMessageModule->getNextMessage());
            } else {
                // use entire display height for larger displays
                int topMsg = (messagesCount > lines && currentMessageIndex >= lines - 1) ? currentMessageIndex - lines + 2 : 0;
                for (int i = 0; i < std::min(messagesCount, lines); i++) {
#ifdef SIMPLE_TDECK
                    if (i == currentMessageIndex - topMsg) {
                        display->fillRect(0 + x, 0 + y + FONT_HEIGHT_LARGE * (i + 1), x + display->getWidth(),
                                          y + FONT_HEIGHT_LARGE);
                        display->setColor(BLACK);
                        display->drawString(0 + x, 0 + y + FONT_HEIGHT_LARGE * (i + 1), cannedMessageModule->getCurrentMessage());
                        display->setColor(WHITE);
                    } else {
                        display->drawString(0 + x, 0 + y + FONT_HEIGHT_LARGE * (i + 1),
                                            cannedMessageModule->getMessageByIndex(topMsg + i));
                    }
#else
                    if (i == currentMessageIndex - topMsg) {
                        display->fillRect(0 + x, 0 + y + FONT_HEIGHT_SMALL * (i + 1), x + display->getWidth(),
                                          y + FONT_HEIGHT_SMALL);
                        display->setColor(BLACK);
                        display->drawString(0 + x, 0 + y + FONT_HEIGHT_SMALL * (i + 1), cannedMessageModule->getCurrentMessage());
                        display->setColor(WHITE);
                    } else {
                        display->drawString(0 + x, 0 + y + FONT_HEIGHT_SMALL * (i + 1),
                                            cannedMessageModule->getMessageByIndex(topMsg + i));
                    }
#endif
                }
            }
        }
    }
}

ProcessMessage CannedMessageModule::handleReceived(const meshtastic_MeshPacket &mp)
{
    if (mp.decoded.portnum == meshtastic_PortNum_ROUTING_APP && waitingForAck) {
        // look for a request_id
        if (mp.decoded.request_id != 0) {
            UIFrameEvent e;
            e.action = UIFrameEvent::Action::REGENERATE_FRAMESET; // We want to change the list of frames shown on-screen
            requestFocus(); // Tell Screen::setFrames that our module's frame should be shown, even if not "first" in the frameset
            this->runState = CANNED_MESSAGE_RUN_STATE_ACK_NACK_RECEIVED;
            this->incoming = service.getNodenumFromRequestId(mp.decoded.request_id);
            meshtastic_Routing decoded = meshtastic_Routing_init_default;
            pb_decode_from_bytes(mp.decoded.payload.bytes, mp.decoded.payload.size, meshtastic_Routing_fields, &decoded);
            this->ack = decoded.error_reason == meshtastic_Routing_Error_NONE;
            waitingForAck = false; // No longer want routing packets
            this->notifyObservers(&e);
            // run the next time 2 seconds later
            setIntervalFromNow(2000);
        }
    }

    return ProcessMessage::CONTINUE;
}

void CannedMessageModule::loadProtoForModule()
{
    if (nodeDB->loadProto(cannedMessagesConfigFile, meshtastic_CannedMessageModuleConfig_size,
                          sizeof(meshtastic_CannedMessageModuleConfig), &meshtastic_CannedMessageModuleConfig_msg,
                          &cannedMessageModuleConfig) != LoadFileResult::SUCCESS) {
        installDefaultCannedMessageModuleConfig();
    }
}

/**
 * @brief Save the module config to file.
 *
 * @return true On success.
 * @return false On error.
 */
bool CannedMessageModule::saveProtoForModule()
{
    bool okay = true;

#ifdef FS
    FS.mkdir("/prefs");
#endif

    okay &= nodeDB->saveProto(cannedMessagesConfigFile, meshtastic_CannedMessageModuleConfig_size,
                              &meshtastic_CannedMessageModuleConfig_msg, &cannedMessageModuleConfig);

    return okay;
}

/**
 * @brief Fill configuration with default values.
 */
void CannedMessageModule::installDefaultCannedMessageModuleConfig()
{
    memset(cannedMessageModuleConfig.messages, 0, sizeof(cannedMessageModuleConfig.messages));
}

/**
 * @brief An admin message arrived to AdminModule. We are asked whether we want to handle that.
 *
 * @param mp The mesh packet arrived.
 * @param request The AdminMessage request extracted from the packet.
 * @param response The prepared response
 * @return AdminMessageHandleResult HANDLED if message was handled
 *   HANDLED_WITH_RESULT if a result is also prepared.
 */
AdminMessageHandleResult CannedMessageModule::handleAdminMessageForModule(const meshtastic_MeshPacket &mp,
                                                                          meshtastic_AdminMessage *request,
                                                                          meshtastic_AdminMessage *response)
{
    AdminMessageHandleResult result;

    switch (request->which_payload_variant) {
    case meshtastic_AdminMessage_get_canned_message_module_messages_request_tag:
        LOG_DEBUG("Client is getting radio canned messages\n");
        this->handleGetCannedMessageModuleMessages(mp, response);
        result = AdminMessageHandleResult::HANDLED_WITH_RESPONSE;
        break;

    case meshtastic_AdminMessage_set_canned_message_module_messages_tag:
        LOG_DEBUG("Client is setting radio canned messages\n");
        this->handleSetCannedMessageModuleMessages(request->set_canned_message_module_messages);
        result = AdminMessageHandleResult::HANDLED;
        break;

    default:
        result = AdminMessageHandleResult::NOT_HANDLED;
    }

    return result;
}

void CannedMessageModule::handleGetCannedMessageModuleMessages(const meshtastic_MeshPacket &req,
                                                               meshtastic_AdminMessage *response)
{
    LOG_DEBUG("*** handleGetCannedMessageModuleMessages\n");
    if (req.decoded.want_response) {
        response->which_payload_variant = meshtastic_AdminMessage_get_canned_message_module_messages_response_tag;
        strncpy(response->get_canned_message_module_messages_response, cannedMessageModuleConfig.messages,
                sizeof(response->get_canned_message_module_messages_response));
    } // Don't send anything if not instructed to. Better than asserting.
}

void CannedMessageModule::handleSetCannedMessageModuleMessages(const char *from_msg)
{
    int changed = 0;

    if (*from_msg) {
        changed |= strcmp(cannedMessageModuleConfig.messages, from_msg);
        strncpy(cannedMessageModuleConfig.messages, from_msg, sizeof(cannedMessageModuleConfig.messages));
        LOG_DEBUG("*** from_msg.text:%s\n", from_msg);
    }

    if (changed) {
        this->saveProtoForModule();
    }
}

String CannedMessageModule::drawWithCursor(String text, int cursor)
{
    String result = text.substring(0, cursor) + "_" + text.substring(cursor);
    return result;
}

#endif
