#include "configuration.h"
#if ARCH_PORTDUINO
#include "PortduinoGlue.h"
#endif
#if HAS_SCREEN
#include "CannedMessageModule.h"
#include "graphics/Screen.h"
#include "Channels.h"
#include "FSCommon.h"
#include "MeshService.h"
#include "NodeDB.h"
#include "PowerFSM.h" // needed for button bypass
#include "detect/ScanI2C.h"
#include "input/ScanAndSelect.h"
#include "mesh/generated/meshtastic/cannedmessages.pb.h"

#include "main.h"                               // for cardkb_found
#include "modules/ExternalNotificationModule.h" // for buzzer control
#if !MESHTASTIC_EXCLUDE_GPS
#include "GPS.h"
#endif
#if defined(USE_EINK) && defined(USE_EINK_DYNAMICDISPLAY)
#include "graphics/EInkDynamicDisplay.h" // To select between full and fast refresh on E-Ink displays
#endif
#ifdef SIMPLE_TDECK
#include "modules/AdminModule.h"
#endif

// OPTIONAL
// #define FOR_GUESTS
// #define MONASTERY_FRIENDS
#define FATHERS_NODES
// #define SECURITY
// #define HELPERS
// #define GATE_SECURITY
#define TESTING
// #define VASILI

#ifdef SIMPLE_TDECK
// namespace graphics {
	MessageHistory history;
// }

// std::vector<std::string> skipNodes = {"", "Unknown Name", "C2OPS", "Athos", "Birdman", "RAMBO", "Broadcast", "Command Post", "APFD", "Friek", "Cross", "CHIP", "St. Anthony", "Monastery", "Gatehouse", "Well3", "SeventyNineRak"};
std::vector<std::string> commandsForRouterOnlyStarting = {"ai", "q ", "ait", "qt", "qm", "qd", "qh", "aim", "aif", "aiff", "aih", "aid", "frcs", "wa "};
std::vector<std::string> commandsForRouterOnlyExact = {"i", "sgo", "ygo", "go", "f", "w", "k", "rp", "s", "wf"};
std::vector<std::pair<unsigned int, std::string>> MYNODES = {
    {3719082304, "Router"},
    {3734369073, "Fr Cyril"},
#ifdef FOR_GUESTS
    {3175760252, "Spare2"},
    {667676428, "Spare4"},
		// {1127590756, "Fr Andre"},
		{1127590756, "Grace"}, //same as Fr Andre's device
    // {205167532, "Dcn Michael"},
#endif
#ifdef HELPERS
    {4184751652, "Kitchen"},
    {667570636, "Fr Theoktist"},
#endif
#ifdef SECURITY
    {667627820, "Gate Security"},
    {3014898611, "Bookstore"},
    {NODENUM_BROADCAST, "BROADCAST"},
		{207139432, "Matrix"},
		{207216020, "Ronin"},
		{3771733328, "Athos"}, //Pete
		{3771735168, "Rambo"}, //Perry
		{3771734404, "Chopani"}, //Niko
		{207036432, "Chip"},
#endif
#ifdef VASILI
#endif
#ifdef GATE_SECURITY
    {3014898611, "Bookstore"},
    {4184751652, "Kitchen"},
    {207141012, "Fr Jerome"},
    {NODENUM_BROADCAST, "BROADCAST"},
    {202935032, "Fr Evgeni"},
		{669969380, "Fr Silouanos"},
    {2579251804, "Fr Alexios"},
    {667570636, "Fr Theoktist"},
		{207139432, "Matrix"},
		{207216020, "Ronin"},
		{3771733328, "Athos"}, //Pete
		{3771735168, "Rambo"}, //Perry
		{3771734404, "Chopani"}, //Niko
		{207036432, "Chip"},
#endif
#ifdef MONASTERY_FRIENDS
		{1127590756, "Fr Andre"},
    {3014898611, "Bookstore"},
    {NODENUM_BROADCAST, "BROADCAST"},
#endif
#ifdef TESTING
    {219520199, "test"}, // for testing
#endif
#ifdef TESTING2
		{1127590756, "Fr Andre"},
    {667676428, "Spare4"},
    {207089188, "Spare7"},
    // {205167532, "Dcn Michael"},
#endif
#ifdef FATHERS_NODES  //fathers
    {3014898611, "Bookstore"},
    {4184751652, "Kitchen"},
    {207141012, "Fr Jerome"},
    {NODENUM_BROADCAST, "BROADCAST"},
    {202935032, "Fr Evgeni"},
    {667627820, "Gate Security"},
		{669969380, "Fr Silouanos"},
    {2579251804, "Fr Alexios"},
    {667570636, "Fr Theoktist"},
    {205167532, "Dcn Michael"},
#endif
    // {2864386355, "Kitchen"}, // was old virtual node
		///REMOVE LATER!!!!
    // {207089188, "Spare1"},
		// {1127590756, "Fr Andre"},
    // {2864390690, "Fr Michael"}, //old virtual node
    // {205167532, "Fr Michael"},

    // {2217306826, "79"}, // for testing
    // {279520186, "CF"}, // for testing
    // {219520199, "test"}, // for testing
    // {2297825467, "W3"}, // for testing
		// below for Geronda only
		// {207036432, "CHIP"},
		// {3771734928, "Birdman"},
		//
		// {NODENUM_BROADCAST, "Broadcast"},
};
unsigned int getNodeNumberByIndex(const std::vector<std::pair<unsigned int, std::string>>& nodes, int index) {
    if (index >= 0 && static_cast<size_t>(index) < nodes.size()) {
        return nodes[index].first;
    } else {
        return 0;  // Return 0 or another sentinel value to indicate an error
    }
}
std::string getNodeNameByIndex(const std::vector<std::pair<unsigned int, std::string>>& nodes, int index) {
    if (index >= 0 && static_cast<size_t>(index) < nodes.size()) {
        return nodes[index].second;
    } else {
        return "";  // Return an empty string or handle the error appropriately
    }
}
uint8_t keyCountLoopForDeliveryStatus = 0, deliveryStatus = 0;
int leftScrollCount = 0, rightScrollCount = 0, nodeIndex = 0;
bool wasTouchEvent = false;
bool justEnteredFreetext = false; // prevents scrolling through nodes when first entering freetext mode

// void CannedMessageModule::setWasTouchEvent(bool value) { this->wasTouchEvent = value; }
// bool CannedMessageModule::getWasTouchEvent() { return this->wasTouchEvent; }

int CannedMessageModule::scrollLeft() {
	if (this->wasTouchEvent) {
		LOG_INFO("Was Touch!\n");
		nodeIndex = (nodeIndex + 1) % MYNODES.size();
		this->wasTouchEvent = false;
	} else {
		LOG_INFO("Not Touch!\n");
    leftScrollCount++;
    rightScrollCount = 0; // Reset the opposite direction counter
    if (leftScrollCount == 2) {
        nodeIndex = (nodeIndex + 1) % MYNODES.size();
        leftScrollCount = 0; // Reset after registering the event
    }
	}
		return nodeIndex;
}

int CannedMessageModule::scrollRight() {
	if (this->wasTouchEvent) {
		LOG_INFO("Was Touch!\n");
		nodeIndex = (nodeIndex - 1 + MYNODES.size()) % MYNODES.size();
		this->wasTouchEvent = false;
	} else {
		LOG_INFO("Not Touch!\n");
    rightScrollCount++;
    leftScrollCount = 0; // Reset the opposite direction counter
    if (rightScrollCount == 2) {
        nodeIndex = (nodeIndex - 1 + MYNODES.size()) % MYNODES.size();
        rightScrollCount = 0; // Reset after registering the event
    }
	}
		return nodeIndex;
}

void CannedMessageModule::setDeliveryStatus(uint8_t status) {
	LOG_INFO("setDeliveryStatus(%d)\n", status);
	switch (status) {
		case 0:
			screen->removeFunctionSymbal("(D) ");
			screen->removeFunctionSymbal(">>> "); break;
		case 1:
			screen->removeFunctionSymbal("(D) ");
			screen->setFunctionSymbal(">>> "); break;
		case 2:
			screen->removeFunctionSymbal(">>> ");
			screen->setFunctionSymbal("(D) "); break;
		case 3:
			screen->removeFunctionSymbal("(D) ");
			screen->removeFunctionSymbal(">>> "); break;
	}
	deliveryStatus = status;
	// if (status != 2) deliveryStatus = status;
	// else deliveryStatus = 0; // to make it so that the traceroutes etc don't show as acked
}
uint8_t CannedMessageModule::getDeliveryStatus() { return deliveryStatus; }
#endif

#ifndef INPUTBROKER_MATRIX_TYPE
#define INPUTBROKER_MATRIX_TYPE 0
#endif

#include "graphics/ScreenFonts.h"

// Remove Canned message screen if no action is taken for some milliseconds
#define INACTIVATE_AFTER_MS 20000

#define NODENUM_RPI5 3719082304
#define NODENUM_SP2 3175760252
#define NODENUM_SP4 667676428
#define NODENUM_DCNM 205167532
#define NODENUM_FRCYRIL 3734369073

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
#if defined(T_WATCH_S3) || defined(RAK14014)
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
		// skipNodes.push_back(cannedMessageModule->getNodeName(nodeDB->getNodeNum()));
		MYNODES.erase(
				std::remove_if(MYNODES.begin(), MYNODES.end(),
											 [&](const std::pair<unsigned int, std::string>& node) {
													 return node.first == nodeDB->getNodeNum();  // Compare the first element of the pair
											 }),
				MYNODES.end()
		);
		// screen->removeFunctionSymbal("ACK");
		this->dest = NODENUM_RPI5;

		if (config.bluetooth.enabled) screen->setFunctionSymbal("BT");
		if (config.power.is_power_saving) screen->setFunctionSymbal("PS");
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
				// below added temp frc trying make lock mode work
				// FIXME: maybe don't want to check for 0x22 here, might be irrelevant
				// I think this helped, but not positive
#ifdef SIMPLE_TDECK
		if ((screen->keyboardLockMode) && (event->kbchar != 0x22)) return 0;
#endif
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
    bool validEvent = false;
    if (event->inputEvent == static_cast<char>(meshtastic_ModuleConfig_CannedMessageConfig_InputEventChar_UP)) {
        if (this->messagesCount > 0) {
            this->runState = CANNED_MESSAGE_RUN_STATE_ACTION_UP;
            validEvent = true;
        }
    }
    if (event->inputEvent == static_cast<char>(meshtastic_ModuleConfig_CannedMessageConfig_InputEventChar_DOWN)) {
        if (this->messagesCount > 0) {
					LOG_INFO("Down with messagesCount: %d\n", this->messagesCount);
            this->runState = CANNED_MESSAGE_RUN_STATE_ACTION_DOWN;
#ifdef SIMPLE_TDECK // fixes touch scroll down, but still messes up existing freetext when using trackball scrolling
							this->cursor = this->freetext.length();
#endif
            validEvent = true;
        } else {
					// frc temp trying to ignore downpress here unless is on prevmsgs screen
					// I think the below is irrelevant because this->messagesCount probably doesn't matter for us.
					// 12-18
					LOG_INFO("Down with messagesCount at 0\n");
					LOG_INFO("Run state: %d\n", this->runState);
					LOG_INFO("Was touch eventB: %d\n", this->wasTouchEvent);
					if (screen->isOnPreviousMsgsScreen) {
						LOG_INFO("on PREVMSGS screen!\n");
						if (this->isOnFirstPreviousMsgsPage) LOG_INFO("is on first page\n");
						else LOG_INFO("is NOT on first page\n");
					} else {
						LOG_INFO("not on PREVMSGS screen!\n");
					}

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
#ifndef SIMPLE_TDECK //just testing, maybe this will cause to not lose text while typing and screen flips...
        this->freetext = ""; // clear freetext
        this->cursor = 0;
#endif
        this->destSelect = CANNED_MESSAGE_DESTINATION_TYPE_NONE;
#endif

        this->runState = CANNED_MESSAGE_RUN_STATE_INACTIVE;
        this->notifyObservers(&e);
    }
#ifndef SIMPLE_TDECK
    if ((event->inputEvent == static_cast<char>(meshtastic_ModuleConfig_CannedMessageConfig_InputEventChar_BACK)) ||
        (event->inputEvent == static_cast<char>(meshtastic_ModuleConfig_CannedMessageConfig_InputEventChar_LEFT)) ||
        (event->inputEvent == static_cast<char>(meshtastic_ModuleConfig_CannedMessageConfig_InputEventChar_RIGHT))) {
#else
    if ((event->inputEvent == static_cast<char>(meshtastic_ModuleConfig_CannedMessageConfig_InputEventChar_BACK)) ||
        (event->inputEvent == static_cast<char>(meshtastic_ModuleConfig_CannedMessageConfig_InputEventChar_LEFT)) ||
        (event->inputEvent == static_cast<char>(meshtastic_ModuleConfig_CannedMessageConfig_InputEventChar_RIGHT))  || (event->inputEvent == static_cast<char>(meshtastic_ModuleConfig_CannedMessageConfig_InputEventChar_DOWN)) || (event->inputEvent == static_cast<char>(meshtastic_ModuleConfig_CannedMessageConfig_InputEventChar_UP)))
		{
#endif
//     if ((event->inputEvent == static_cast<char>(meshtastic_ModuleConfig_CannedMessageConfig_InputEventChar_LEFT)) ||
//         (event->inputEvent == static_cast<char>(meshtastic_ModuleConfig_CannedMessageConfig_InputEventChar_RIGHT)) ||
// 				(event->inputEvent == static_cast<char>(meshtastic_ModuleConfig_CannedMessageConfig_InputEventChar_UP)))
// {

// #if defined(T_WATCH_S3) || defined(RAK14014)
#if defined(T_WATCH_S3) || defined(RAK14014) || defined(SIMPLE_TDECK) // enabling TDeck here allows scrolling through node names. NOTE: I don't know why I couldn't move this down below. It wasn't registering all the touch events below. should try to fix later to make cleaner
        if (event->inputEvent == static_cast<char>(meshtastic_ModuleConfig_CannedMessageConfig_InputEventChar_LEFT)) {
					LOG_INFO("this->runState: %d", this->runState);
            this->payload = 0xb4;
        } else if (event->inputEvent == static_cast<char>(meshtastic_ModuleConfig_CannedMessageConfig_InputEventChar_RIGHT)) {
					LOG_INFO("this->runState: %d", this->runState);
            this->payload = 0xb7;
        }
#ifdef SIMPLE_TDECK
// 		if (event->inputEvent ==
// static_cast<char>(meshtastic_ModuleConfig_CannedMessageConfig_InputEventChar_LEFT)) {
// 				this->payload = 0xb4;
// 		} else if (event->inputEvent == static_cast<char>(meshtastic_ModuleConfig_CannedMessageConfig_InputEventChar_RIGHT)) {
// 				this->payload = 0xb7;



				//NOTE: below is for entering-exiting freetext mode on swipe up/down. It wasn't perfect. In general it was working, except for at startup sometimes. Also, there was a strange bug where it would enter a black screen on swiping near beginning. Note also that this was the main place for entering-exiting freetext mode on swipe. There are other places in the code that are commented out for this feature, but it seems this was the best place to do it. 1-2-24
// 		else if (event->inputEvent ==
// static_cast<char>(meshtastic_ModuleConfig_CannedMessageConfig_InputEventChar_UP)) {
// 			if (this->wasTouchEvent) { // only exit freetext mode if was touchscreen UP, not from trackball UP which is too sensitive
// 				// NOTE: messy below, not sure what's needed, if anything. Might've started working after added touchDirection = 2, but not sure.
// 				this->wasTouchEvent = false; // fixed can't enter cursor scroll mode after any touch events 12-30
// 				LOG_INFO("Was touch event\n");
// 				this->touchDirection = 2; //UP
// 										// this->exitingFreetextMode = true;
// 				// this->payload = 0x23;
// 				// 12-27 testing, used to be 0x23 line above, but was messing up some other stuff
// 					// LOG_INFO("Got case 0x23\n");
// 					LOG_INFO("was touch eventA: %d\n", this->wasTouchEvent);
// 					this->runState = CANNED_MESSAGE_RUN_STATE_INACTIVE;
//             // this->runState = CANNED_MESSAGE_RUN_STATE_ACTION_UP; //12-30
// 					this->lastTouchMillis = millis();
//             // e.action = UIFrameEvent::Action::REGENERATE_FRAMESET; // We want to change the list of frames shown on-screen
//             requestFocus(); // Tell Screen::setFrames that our module's frame should be shown, even if not "first" in the frameset
// 					// this->notifyObservers(&e);
// 			}
// 		} else if (event->inputEvent ==
// static_cast<char>(meshtastic_ModuleConfig_CannedMessageConfig_InputEventChar_DOWN)) {
// 			if (this->wasTouchEvent) { // only enters freetext mode if was touchscreen DOWN, not from trackball UP which is too sensitive
// 				LOG_INFO("was touch event hereA\n");
// 				if (this->isOnFirstPreviousMsgsPage) { // only allow to go to freetext page if its on the first prev msgs page
// 					LOG_INFO("IS ON FIRST PREV MSGS PAGE\n");
// 							this->cursor = this->freetext.length();
// 							this->destSelect = CANNED_MESSAGE_DESTINATION_TYPE_NONE;
// 							this->lastTouchMillis = millis();
// 							this->runState = CANNED_MESSAGE_RUN_STATE_FREETEXT;
// 							requestFocus();
// 				} else LOG_INFO("IS NOT ON FIRST PREV MSGS PAGE\n");
// 			}
		// }
				//UNCOMMENT TO HERE IF YOU WANT TO TEST UP-DOWN SWIPE TO ENTER-EXIT FREETEXT AGAIN
		else if (event->inputEvent ==
static_cast<char>(meshtastic_ModuleConfig_CannedMessageConfig_InputEventChar_BACK)) {
				this->payload = 0x08;
		}
#endif
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
					LOG_INFO("Canned message event (%x)\n", event->kbchar);
					// TODO: below doesn't seem to do anything 12-18 testing
					if (event->kbchar != 0) { // for ignoring up trackball event when in freetext mode
						LOG_INFO("HERE\n");
            this->payload = event->kbchar;
					} else LOG_INFO("HERE2\n");
#else
            this->payload = event->kbchar;
#endif
        }
#endif

#ifdef SIMPLE_TDECK
				LOG_INFO("Canned message event2 (%h)\n", event->kbchar);
#endif
        this->lastTouchMillis = millis();
        validEvent = true;
    }

#ifndef SIMPLE_TDECK
    if (event->inputEvent == static_cast<char>(ANYKEY)) {
#else
    if ((event->inputEvent == static_cast<char>(ANYKEY)) || (event->inputEvent == static_cast<char>(meshtastic_ModuleConfig_CannedMessageConfig_InputEventChar_DOWN)) || (event->inputEvent == static_cast<char>(meshtastic_ModuleConfig_CannedMessageConfig_InputEventChar_UP))) {
#endif
#ifdef SIMPLE_TDECK
				// if (moduleConfig.external_notification.enabled && (externalNotificationModule->nagCycleCutoff != UINT32_MAX))
			LOG_INFO("was touch eventC: %d\n", this->wasTouchEvent);
			LOG_INFO("event->inputEvent: %h\n", event->inputEvent);
			LOG_INFO("event->kbchar: %h\n", event->kbchar);
			LOG_INFO("runState: %d\n", this->runState);
				// FIXME: later on try to make it detect if the LED is on first
					// externalNotificationModule->stopNow(); // this will turn off all GPIO and sounds and idle the loop
			externalNotificationModule->setExternalOff(0); // this will turn off all GPIO and sounds and idle the loop

			// if (this->runState == CANNED_MESSAGE_RUN_STATE_INACTIVE) return 0;
			// 3 is freetext mode, 1/inactive is normal mode
			// trying to solve: want to scroll down when on no more prev msgs, so that can go to freetext mode after scroll down. but not when in middle of prv msgs
			if ((event->inputEvent == static_cast<char>(meshtastic_ModuleConfig_CannedMessageConfig_InputEventChar_DOWN)) || (event->inputEvent == static_cast<char>(meshtastic_ModuleConfig_CannedMessageConfig_InputEventChar_UP))) {
				if (this->wasTouchEvent == false) {
					LOG_INFO("Got UP or DOWN here! Was not touch event, ignoring\n");
					// NOTE: this finally fixed the problem where trackball up down was messing up freetext mode
					return 0;
				} else {
					LOG_INFO("was touch event hereB\n");
		if (event->inputEvent ==
static_cast<char>(meshtastic_ModuleConfig_CannedMessageConfig_InputEventChar_UP)) {
			// LOG_INFO("Got UP here\n");
										// this->exitingFreetextMode = true;
				// this->payload = 0x23; //bad
				// this->touchDirection = 2; //UP
				// LOG_INFO("setting touchDirection: %d", this->touchDirection);
							// this->wasTouchEvent = true; //new 12-27 test
							// below finally worked here, wasn't working in other places


// removed below 12-30 trying stop swipe from exiting freetext mode and entering black screen
				// 			if (this->runState == CANNED_MESSAGE_RUN_STATE_FREETEXT) { // if exiting freetext
				// 				LOG_INFO("exiting freetext, setting goBackToFirstPreviousMessage\n");
    //         // this->runState = CANNED_MESSAGE_RUN_STATE_ACTION_UP; //12-30
				// 				this->goBackToFirstPreviousMessage = true;
				// // this->touchDirection = 1; //DOWN. For some reason this fixed going to prevMsg screen when exit freetext mode
				// this->touchDirection = 2;
				// // this->wasTouchEvent = true;
    //     this->lastTouchMillis = millis();
    //     validEvent = true;
				// 			}



				// if (this->isOnLastPreviousMsgsPage) {
            // this->runState = CANNED_MESSAGE_RUN_STATE_FREETEXT;
						// LOG_INFO("HERE2, RUN_STATE_FREETEXT\n");
				// }
			}
		else if (event->inputEvent ==
static_cast<char>(meshtastic_ModuleConfig_CannedMessageConfig_InputEventChar_DOWN)) {
				// this->payload = 0x08;
			// this->touchDirection = 1; //DOWN
			// LOG_INFO("Got DOWN here, setting touchDirection: %d", this->touchDirection);
   //          // this->runState = CANNED_MESSAGE_RUN_STATE_ACTION_DOWN; //12-30
							// this->wasTouchEvent = true; //new 12-27 test
		}
	}
			}
#endif
        // when inactive, this will switch to the freetext mode
        if ((this->runState == CANNED_MESSAGE_RUN_STATE_INACTIVE) || (this->runState == CANNED_MESSAGE_RUN_STATE_ACTIVE) ||
            (this->runState == CANNED_MESSAGE_RUN_STATE_DISABLED)) {

#ifdef SIMPLE_TDECK
					// 12-27 disabled all below, not sure consequences
			// if (this->skipNextFreetextMode == false) {
			// // if (this->lastTrackballMillis + 10000 > millis()) { // this stops it from entering freetext mode if you're just pressing the 0-Mic key to enable the trackball scrolling
			// 	LOG_INFO("event->kbchar: %x\n", event->kbchar);
			// 	// if (this->isOnFirstPreviousMsgsPage) { // only go to freetext mode if scrolling from newest prev msgs page
			// 	// FIXME: I think below has the wrong type of if check. I think you'll probably want a different check to see if you're trying to exit freetext mode
			// 	LOG_INFO("this->runState: %d\n", this->runState);
			// 	if (this->isOnFirstPreviousMsgsPage || (event->kbchar != 0x5f && event->kbchar != 0x60)) { // only go to freetext mode if scrolling from newest prev msgs page and was scrolling upwards (5f)
			// 	// if (this->runState == CANNED_MESSAGE_RUN_STATE_INACTIVE) {
			// 		LOG_INFO("this->payload: %x\n", this->payload);
			// 		if (event->kbchar != 0x7e) {
			// 		// if ((event->kbchar != 0x7e) && (event->kbchar != 0x5e)) { // this one ignores up-down on some, testing
   //          this->runState = CANNED_MESSAGE_RUN_STATE_FREETEXT;
			// 			LOG_INFO("setting RUN_STATE_FREETEXT\n");
			// 			LOG_INFO("event->kbchar2: %x\n", event->kbchar);
			// 		} else {
			// 			LOG_INFO("got 0 key press on prev mesgs screen\n");
			// 			// below doesn't work
			// 			this->goBackToFirstPreviousMessage = true;
			// 		}
			// 	}
			// 			// validEvent = true; // TESTING 12-18, not sure if does anything, trying to fix trackball up-down bug with freetext cursor
			// // }
			// } else this->skipNextFreetextMode = false;
#endif
        }

        validEvent = false; // If key is normal than it will be set to true.

        // Run modifier key code below, (doesnt inturrupt typing or reset to start screen page)
        switch (event->kbchar) { //note: showTemporaryMessage doesn't work here
				// case 0xf: // alt-f, toggle flashlight
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
				case 0x5f: // _, cursorScrollMode
					//toggle cursor scroll mode
					LOG_INFO("got 0x5f\n");
					if (!wasTouchEvent) {
						LOG_INFO("was not touch event, toggle cursor scroll mode");
						if (!this->cursorScrollMode) {
							this->cursorScrollMode = true;
							screen->setFunctionSymbal("Scrl"); // add the S symbol to the bottom right corner
						} else {
							this->cursorScrollMode = false;
							this->cursor = this->freetext.length();
							screen->removeFunctionSymbal("Scrl"); // remove the S symbol from the bottom right corner
						}
					} else { // was touch event
						LOG_INFO("was touch event, don't enter cursor scroll mode\n");
						// NOTE: below does same thing twice. Currently having both up and down touch scroll exit freetext mode. maybe unify or change
						if (this->touchDirection == 2) { // up, exit freetext mode, go to previous messages screen
							// NOTE that this also is required for flipping through the previous messages pages. It's not just for exiting freetext mode.
							LOG_INFO("UP\n");
							LOG_INFO("runState: %d\n", this->runState);
							this->wasTouchEvent = false;
							this->lastTouchMillis = millis();
							this->goBackToFirstPreviousMessage = true; // added 12-29 trying to fix going to 2nd prv msg on exit freetext mode
							// learned that it only ever gets here if run state is 3 / freetext, when set from somewhere else. It's not actually on freetext mode every time though. there's another section that is scrolling for us
							if (this->runState == CANNED_MESSAGE_RUN_STATE_FREETEXT) LOG_INFO("runState is CANNED_MESSAGE_RUN_STATE_FREETEXT\n");
							else LOG_INFO("runState is not CANNED_MESSAGE_RUN_STATE_FREETEXT\n");
						// if (this->isOnLastPreviousMsgsPage && this->runState != CANNED_MESSAGE_RUN_STATE_FREETEXT) {
						// 	LOG_INFO("IS on last page, entering freetext mode\n");
						// 	this->wasTouchEvent = false;
						// 	this->cursor = this->freetext.length();
						// 	this->runState = CANNED_MESSAGE_RUN_STATE_FREETEXT;
						// 	this->destSelect = CANNED_MESSAGE_DESTINATION_TYPE_NONE;
						// 	// e.action = UIFrameEvent::Action::REGENERATE_FRAMESET; // We want to change the list of frames shown on-screen
						// 	this->lastTouchMillis = millis();
						// 	this->notifyObservers(&e);
						// 	// this->skipNextRletter = true;
						// 	requestFocus();
						// } else {
						// 	LOG_INFO("IS NOT on last page\n");
							this->runState = CANNED_MESSAGE_RUN_STATE_INACTIVE; // this line is required for exiting freetext mode
        UIFrameEvent e;
							e.action = UIFrameEvent::Action::REGENERATE_FRAMESET; // We want to change the list of frames shown on-screen
							requestFocus(); // Tell Screen::setFrames that our module's frame should be shown, even if not "first" in the frameset
							this->notifyObservers(&e);
						// }
						} else { // down, freetext screen
							LOG_INFO("DOWN\n");
						if (this->isOnFirstPreviousMsgsPage) {
							LOG_INFO("IS on first page, entering freetext mode\n");
							this->wasTouchEvent = false;
							this->cursor = this->freetext.length();
							this->runState = CANNED_MESSAGE_RUN_STATE_FREETEXT;
							screen->isOnPreviousMsgsScreen = false;
							this->destSelect = CANNED_MESSAGE_DESTINATION_TYPE_NONE;
							this->lastTouchMillis = millis();
							UIFrameEvent e;
							this->notifyObservers(&e);
							// this->skipNextRletter = true;
							requestFocus();
						} else LOG_INFO("IS NOT on first page\n");


							// this->runState = CANNED_MESSAGE_RUN_STATE_INACTIVE;
							// this->lastTouchMillis = millis();
							// 	e.action = UIFrameEvent::Action::REGENERATE_FRAMESET; // We want to change the list of frames shown on-screen
							// 	requestFocus(); // Tell Screen::setFrames that our module's frame should be shown, even if not "first" in the frameset
							// this->notifyObservers(&e);
						}

					}
					break;
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
                    showTemporaryMessage("Notifications\nEnabled");
                    if (screen)
                        screen->removeFunctionSymbal("M"); // remove the mute symbol from the bottom right corner
                } else {
                    externalNotificationModule->stopNow(); // this will turn off all GPIO and sounds and idle the loop
                    externalNotificationModule->setMute(true);
                    externalNotificationModule->setExternalOff(0); // this will turn off the LED if it was on
                    showTemporaryMessage("Notifications\nDisabled");
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
#ifdef SIMPLE_TDECK
				case 0xAA: // TODO: DOES NOT WORK, key not detected. alt-b, toggle Bluetooth on/off
            if (config.bluetooth.enabled == true) {
                config.bluetooth.enabled = false;
                LOG_INFO("User toggled Bluetooth");
                nodeDB->saveToDisk();
                disableBluetooth();
                showTemporaryMessage("Bluetooth OFF");
            } else if (config.bluetooth.enabled == false) {
                config.bluetooth.enabled = true;
                LOG_INFO("User toggled Bluetooth");
                nodeDB->saveToDisk();
                rebootAtMsec = millis() + 2000;
                showTemporaryMessage("Bluetooth ON\nReboot");
            }
            break;
#endif
        case 0xaf: // fn+space send network ping like double press does
            service->refreshLocalMeshNode();
            if (service->trySendPosition(NODENUM_BROADCAST, true)) {
                showTemporaryMessage("Position\nUpdate Sent");
            } else {
                showTemporaryMessage("Node Info\nUpdate Sent");
            }
            break;
#ifdef SIMPLE_TDECK
				case 0x24:  // $ sign
					if (moduleConfig.external_notification.enabled == true) {
							if (externalNotificationModule->getMute()) {
									externalNotificationModule->setMute(false);
									showTemporaryMessage("Buzzer\nEnabled");
									if (screen)
											screen->removeFunctionSymbal("M"); // remove the mute symbol from the bottom right corner
							} else {
									externalNotificationModule->stopNow(); // this will turn off all GPIO and sounds and idle the loop
									externalNotificationModule->setMute(true);
									showTemporaryMessage("Buzzer\nDisabled");
									if (screen)
											screen->setFunctionSymbal("M"); // add the mute symbol to the bottom right corner
							}
					}
						requestFocus();
				// 		// runOnce();
				break;
			// case 0x00:  // up down trackball events
			// 	LOG_INFO("Trackball event, ignoring\n");
			// 	break;
#endif
        default:
            // pass the pressed key
            LOG_DEBUG("Canned message ANYKEY (%x)\n", event->kbchar);
#ifdef SIMPLE_TDECK
						// want to display value of skipNextFreetextMode
						// LOG_INFO("skipNextFreetextMode: %d\n", this->skipNextFreetextMode);
						// LOG_INFO("skipNextRletter: %d\n", this->skipNextRletter);
						// if ((screen->keyboardLockMode == false) && (event->kbchar != 0x22)) {
						// TODO: might want to reset keyCountLoopForDeliveryStatus to 0 sometime if we get a new message
            LOG_INFO("Canned message ANYKEY (%x)\n", event->kbchar);
					// 	if (event->kbchar == 0x60) {
					// 		LOG_INFO("GOT 60\n");
					// 		this->runState = CANNED_MESSAGE_RUN_STATE_FREETEXT; // this fixed the problem where touch scrolling up on freetext exited to black screen 12-30
					// }
						LOG_INFO("touchDirection here: %d\n", this->touchDirection);
						LOG_INFO("wasTouchEvent here: %d\n", this->wasTouchEvent);
						this->wasTouchEvent = false; // fixed can't enter cursor scroll mode after scroll up on freetext 12-30
						// 	return 0;
							// 2 ways to proceed here:
							// either here do exit-enter freetext mode (worth a test, but also might have to find other spots and remove them)
							// or go further down and just skip adding the apostrophe character when it was a touch event
						// }

						if (deliveryStatus == 2) { // means ACKed, showing (D)
						 LOG_INFO("deliveryStatus == 2\n");
							keyCountLoopForDeliveryStatus++;
							if (keyCountLoopForDeliveryStatus > 2) {
								setDeliveryStatus(0);
								keyCountLoopForDeliveryStatus = 0;
							}
						}
						if (!screen->keyboardLockMode) {
							if (this->skipNextRletter) {
								this->skipNextRletter = false;
							} else {
#endif
								// below works to fix problem where it was doing scrollLeft() when entering freetext mode on swipe up/down. But also I'm not sure if it messes up anything else 12-28. Might have to move it somewhere else
								// if (event->kbchar != 0x5e) {
								// if (event->kbchar == 0x60 || event->kbchar == 0x5f || event->kbchar == 0x7e) { // this is the apostrophe character
								// 	LOG_INFO("HERE ENTERING TEXTBB\n");
								// 	if (this->freetext.length() > 0) {
								// 		LOG_INFO("HERE ENTERING TEXTCC\n");
								// 		this->freetext = this->freetext.substring(0, this->freetext.length() - 1);
								// 		this->cursor = this->freetext.length();
								// 	}
								// }
								// if (event->kbchar != 0x60 && event->kbchar != 0x5f) { // checking to make sure it is not the apostrophe character  // 12-29 removed this because it was never allowing to enter cursorScrollMode






								// 1-2-24 added 0x5f below, to stop from entering freetext on some devices
								if (event->kbchar != 0x60 && event->kbchar != 0x5f && event->kbchar != 0x5e) { // checking to make sure it is not the apostrophe character
									LOG_INFO("got NOT 0x60 here\n");
								// 1-2-24 disabled below to stop entering freetext mode on swipe. This was probably the wrong place to do it anyway. look at other notes
								// 	this->justEnteredFreetext = true; // prevents scrolling through nodes when first entering freetext mode
								// 	LOG_INFO("this->justEnteredFreetext: %d\n", this->justEnteredFreetext);
									LOG_INFO("event->kbchar: %h\n", event->kbchar);
									if (event->kbchar == 0x5f) LOG_INFO("GOT1\n");
									// if (event->kbchar == "5f") LOG_INFO("GOT3\n");
									if (event->kbchar != 0x5f) LOG_INFO("GOT2\n");
									if (event->kbchar == 0x5f && !this->isOnFirstPreviousMsgsPage) { // this is the apostrophe character
										LOG_INFO("NOT ENTERING FREETEXT\n");
										return 0;
									}
									if (screen->isOnPreviousMsgsScreen) {
										if (event->kbchar == 0x7e) { // added 1-6-25, still testing
											LOG_INFO("Got 0x7e on previousMsgsScreen\n");
											this->goBackToFirstPreviousMessage = true; // added 12-29 trying to fix going to 2nd prv msg on exit freetext mode
											UIFrameEvent e;
											e.action = UIFrameEvent::Action::REGENERATE_FRAMESET; // We want to change the list of frames shown on-screen
											this->notifyObservers(&e);
											// requestFocus(); // Tell Screen::setFrames that our module's frame should be shown, even if not "first" in the frameset
											// validEvent = true;
											return 0;
										}
									}
									// 	LOG_INFO("got 0x5f here1\n");
								// 	// 	//NOW this detects correctly, try make it so that it doesn't enter freetext
								// 	// 	if (this->isOnFirstPreviousMsgsPage) {
								// 	// 		LOG_INFO("got 0x5f here2\n");
								// 	// 		LOG_INFO("isOnFirstPreviousMsgsPage: %d\n", this->isOnFirstPreviousMsgsPage);
								// 	// 	} else {
								// 	// 		LOG_INFO("got 0x5f here3\n");
								// 	// 		LOG_INFO("isOnFirstPreviousMsgsPage: %d\n", this->isOnFirstPreviousMsgsPage);
								// 	// 	}
								// 	//
								// 	// }
									LOG_INFO("HERE ENTERING TEXT\n");
									this->payload = event->kbchar;
									this->runState = CANNED_MESSAGE_RUN_STATE_FREETEXT; // 12-27 added this line, fixed freetext mode on entering any key
									screen->isOnPreviousMsgsScreen = false;
									this->lastTouchMillis = millis();
									validEvent = true;
							} else {
								// end prevent swipe from entering freetext mode
								// if (event->kbchar == 0x60) { // checking to make sure it is not the apostrophe character
									LOG_INFO("got 0x60 here\n");
									LOG_INFO("touchDirectionDD: %d\n", this->touchDirection);
								}

									// BELOW WAS THE ENTIRE PROBLEM, don't use. maybe move elsewhere. couldn't get it to exit freetext mode reliably while still allowing scrolling through previous messages 12-30
				// 					if (this->touchDirection == 1) { // down, entering freetext mode
				// 						LOG_INFO("DOWN HERE\n");
				// 						this->runState = CANNED_MESSAGE_RUN_STATE_FREETEXT;
				// 						// this->payload = event->kbchar; // not doing this is what prevents the apostrophe from being entered
				// 					LOG_INFO("this->freetext.length(): %d\n", this->freetext.length());
				// 					LOG_INFO("this->freetext: %s\n", this->freetext.c_str());
				// 					this->justEnteredFreetext = true; // prevents scrolling through nodes when first entering freetext mode, added 12-30
				// 					if (this->freetext == "") {
				// 						LOG_INFO("YES GOT EMPTY\n");
				// 						if (this->freetext.length() == 1) {
				// 							LOG_INFO("YES GOT EMPTY 2\n");
				// 							this->freetext = "";
				// 							this->cursor = 0;
				// 							LOG_INFO("this->freetext.length(): %d\n", this->freetext.length());
				// 						}
				// 					}
				// // TODO 12-28 might want to re-enable below
				// 						// if (this->freetext.length() > 0) {
				// 						// 	this->freetext = this->freetext.substring(0, this->freetext.length() - 1);
				// 						// 	this->cursor = this->freetext.length();
				// 						// } else {
				// 						// 	LOG_INFO("HERE2\n");
				// 						// 	this->freetext = "";
				// 						// 	this->cursor = 0;
				// 						// 	// not sure if all this is necessary, unfinished here. problem with first run through still not showing cursor 12-28
				// 						// 	this->currentMessageIndex = -1;
				// 						// 	this->freetext = this->freetext.substring(0, this->freetext.length() - 1);
				// 						// 	requestFocus();
				// 						// }
				// 						this->destSelect = CANNED_MESSAGE_DESTINATION_TYPE_NONE;
				// 						this->lastTouchMillis = millis();
				// 						validEvent = true;
				//
				// 					} else if (this->touchDirection == 2) { // up, exit freetext mode
				// 						// the below 3 are the ones that fixed exiting freetext mode 12-27
				// 						LOG_INFO("UP HERE, exiting freetext mode\n");
				// 						LOG_INFO("this->runStateHere: %d\n", this->runState);
				// 						UIFrameEvent e;
				// 						e.action = UIFrameEvent::Action::REGENERATE_FRAMESET; // We want to change the list of frames shown on-screen
				// 						// below doesn't work because it's always registering as up, so it never allows to scroll through previous messages
				// 						// LOG_INFO("this->exitingFreetextMode: %d\n", this->exitingFreetextMode);
				// 						// LOG_INFO("this->wasTouchEvent: %d\n", this->wasTouchEvent);
				// 						// this->exitingFreetextMode = true;
				// 						requestFocus(); // Tell Screen::setFrames that our module's frame should be shown, even if not "first" in the frameset
				// 						this->notifyObservers(&e);
				// 						this->runState = CANNED_MESSAGE_RUN_STATE_INACTIVE;
				// 						this->lastTouchMillis = millis();
				// 						// this->goBackToFirstPreviousMessage = true; // this one breaks the ability to scroll through previous messages. It does though fix when exiting freetext mode going up to the first prev msg
				// 						validEvent = true;
				// 					}

#ifdef SIMPLE_TDECK
							}
						}
						else if ((screen->keyboardLockMode) && (event->kbchar == 0x22)) {
							screen->keyboardLockMode = false;
							this->runState = CANNED_MESSAGE_RUN_STATE_INACTIVE; //prevents entering freetext mode
							screen->removeFunctionSymbal("KL");
						}
#endif
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
		setDeliveryStatus(3); //3 means actually tried to send a message. Setting in order to stop traceroutes and other packets from setting delivery status as 2 later on
		this->totalMessagesSent++;
		// LOG_INFO("Total messages sent: %d\n", this->totalMessagesSent);
		if (strcmp(message, " ") == 0) {
			LOG_INFO("Message is only a space, not sending\n");
			return;
		}
		// replace "sss" anywhere in message with unicode smiley emoji
    // Create a modifiable copy of the message
    constexpr size_t bufferSize = 256; // Adjust the buffer size as needed
    char modifiableMessage[bufferSize];
    strncpy(modifiableMessage, message, bufferSize - 1);
    modifiableMessage[bufferSize - 1] = '\0'; // Ensure null-termination

// const char *target1 = "sss";
// const char *emoji1 = "😊";
// const char *target2 = "ttt";
// const char *emoji2 = "👍";
// const char *target3 = "hhh";
// const char *emoji3 = "❤️";
// const char *target4 = "rofl";
// const char *emoji4 = "🤣";
//
// char result[bufferSize] = {0};
// char *currentPos = modifiableMessage;
// char *resultPos = result;
// size_t remaining = bufferSize - 1; // Keep track of remaining space
//
// while (*currentPos != '\0' && remaining > 0) {
//     if (strncmp(currentPos, target1, strlen(target1)) == 0) {
//         size_t len = strlen(emoji1);
//         if (len > remaining) break;
//         memcpy(resultPos, emoji1, len);
//         resultPos += len;
//         remaining -= len;
//         currentPos += strlen(target1);
//     }
//     // Similar blocks for target2, target3, target4
//     else {
//         *resultPos++ = *currentPos++;
//         remaining--;
//     }
// }
//
// *resultPos = '\0';

		
    // Replace "sss" with the smiley emoji (😊) and "ttt" with thumbs-up emoji (👍)
    const char *target1 = "sss";
    const char *emoji1 = "😊";
    const char *target2 = "ttt";
    const char *emoji2 = "👍";
    const char *target3 = "hhh";
    const char *emoji3 = "❤️";
    const char *target4 = "rofl";
		const char *emoji4 = "🤣";
    const char *target5 = "fff";
		const char *emoji5 = "🙏";

    char result[bufferSize] = {0}; // Buffer to hold the final message
    char *currentPos = modifiableMessage;
    char *resultPos = result;

    while (*currentPos != '\0') {
        // Check for "sss"
        if (strncmp(currentPos, target1, strlen(target1)) == 0) {
            strncat(resultPos, emoji1, bufferSize - strlen(result) - 1);
            resultPos += strlen(emoji1);
            currentPos += strlen(target1);
        } // Check for "ttt"
        else if (strncmp(currentPos, target2, strlen(target2)) == 0) {
            strncat(resultPos, emoji2, bufferSize - strlen(result) - 1);
            resultPos += strlen(emoji2);
            currentPos += strlen(target2);
        } // Check for "hhh"
        else if (strncmp(currentPos, target3, strlen(target3)) == 0) {
            strncat(resultPos, emoji3, bufferSize - strlen(result) - 1);
            resultPos += strlen(emoji3);
            currentPos += strlen(target3);
        } // Check for "rofl"
        else if (strncmp(currentPos, target4, strlen(target4)) == 0) {
            strncat(resultPos, emoji4, bufferSize - strlen(result) - 1);
            resultPos += strlen(emoji4);
            currentPos += strlen(target4);
				} // check for "fff"
        else if (strncmp(currentPos, target5, strlen(target5)) == 0) {
            strncat(resultPos, emoji5, bufferSize - strlen(result) - 1);
            resultPos += strlen(emoji5);
            currentPos += strlen(target5);
				}
        // Copy the current character
        else *resultPos++ = *currentPos++;
    }

    *resultPos = '\0'; // Null-terminate the result
    // LOG_INFO("Modified message: %s\n", result);

		// below was for enabling number count before every message, for testing
		// char totalMessagesSent[8];
		// memset(totalMessagesSent, 0, sizeof(totalMessagesSent)); // clear the string, first send has junk data
		// sprintf(totalMessagesSent, "%d] ", this->totalMessagesSent);
		// char newMessage[strlen(totalMessagesSent) + strlen(message) + 1];
		// strcpy(newMessage, totalMessagesSent);
		// strcat(newMessage, message);
		// p->decoded.payload.size = strlen(newMessage);
		// memcpy(p->decoded.payload.bytes, newMessage, p->decoded.payload.size);
    p->decoded.payload.size = strlen(result);
    memcpy(p->decoded.payload.bytes, result, p->decoded.payload.size);
#else
    p->decoded.payload.size = strlen(message);
    memcpy(p->decoded.payload.bytes, message, p->decoded.payload.size);
#endif
    if (moduleConfig.canned_message.send_bell && p->decoded.payload.size < meshtastic_Constants_DATA_PAYLOAD_LEN) {
        p->decoded.payload.bytes[p->decoded.payload.size] = 7;        // Bell character
        p->decoded.payload.bytes[p->decoded.payload.size + 1] = '\0'; // Bell character
        p->decoded.payload.size++;
    }

    // Only receive routing messages when expecting ACK for a canned message
    // Prevents the canned message module from regenerating the screen's frameset at unexpected times,
    // or raising a UIFrameEvent before another module has the chance
    this->waitingForAck = true;
#ifdef SIMPLE_TDECK  // don't need acks from "Router" node because the auto response is the ack
		if (p->to == NODENUM_RPI5) {
			p->want_ack = false;
			this->waitingForAck = false;
		}
#endif

    LOG_INFO("Sending message id=%d, dest=%x, msg=%.*s\n", p->id, p->to, p->decoded.payload.size, p->decoded.payload.bytes);

		history.addMessage(p->decoded.payload.bytes, this->getNodeName(p->to));
    service->sendToMesh(
        p, RX_SRC_LOCAL,
        true); // send to mesh, cc to phone. Even if there's no phone connected, this stores the message to match ACKs
}

int32_t CannedMessageModule::runOnce()
{
#ifdef SIMPLE_TDECK
	if (alreadySentFirstMessage == 0) {
		String date = __DATE__; // format: "MMM DD YYYY"
		String time = __TIME__; // format: "HH:MM:SS"
		String month = date.substring(0, 3);
		int monthNumber;
		if (month == "Jan") monthNumber = 1;
		else if (month == "Feb") monthNumber = 2;
		else if (month == "Mar") monthNumber = 3;
		else if (month == "Apr") monthNumber = 4;
		else if (month == "May") monthNumber = 5;
		else if (month == "Jun") monthNumber = 6;
		else if (month == "Jul") monthNumber = 7;
		else if (month == "Aug") monthNumber = 8;
		else if (month == "Sep") monthNumber = 9;
		else if (month == "Oct") monthNumber = 10;
		else if (month == "Nov") monthNumber = 11;
		else if (month == "Dec") monthNumber = 12;
		int day = date.substring(4, 6).toInt(); // Extract day and remove leading zero
		char startupMessage[30];
		snprintf(startupMessage, sizeof(startupMessage), "%s ON %d-%d", cannedMessageModule->getNodeName(nodeDB->getNodeNum()), monthNumber, day);
#ifndef TESTING
		sendText(NODENUM_RPI5, 0, startupMessage, false);
#endif
		alreadySentFirstMessage = 1;
		setDeliveryStatus(0); // to make it so that the traceroutes etc don't show as acked
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
							// check if freetext equals "rr" to resend last message
							if (this->freetext == "rr") {
								if (this->previousFreetext.length() > 1) {
									if (this->previousDest == NODENUM_BROADCAST) this->previousDest = NODENUM_RPI5;
									LOG_DEBUG("Resending previous message to %x: %s\n", this->previousDest, this->previousFreetext.c_str());
									sendText(this->previousDest, 0, this->previousFreetext.c_str(), true);
									showTemporaryMessage("Resending last message");
								} else {
									showTemporaryMessage("No previous\nmessage to resend");
								}
							} else if (this->freetext == "bt0") {
								if (!config.bluetooth.enabled) {
									showTemporaryMessage("Bluetooth already OFF");
								} else {
									config.bluetooth.enabled = false;
									LOG_INFO("User toggled Bluetooth");
									nodeDB->saveToDisk();
									disableBluetooth();
									showTemporaryMessage("Bluetooth OFF");
									screen->removeFunctionSymbal("BT");
								}
							} else if (this->freetext == "bt1") {
								if (config.bluetooth.enabled) {
									showTemporaryMessage("Bluetooth already ON");
								} else {
									config.bluetooth.enabled = true;
									LOG_INFO("User toggled Bluetooth");
									nodeDB->saveToDisk();
									rebootAtMsec = millis() + 2000;
									showTemporaryMessage("Bluetooth ON\nReboot");
								}
							} else if (this->freetext == "ps1") {
								if (config.power.is_power_saving) {
									showTemporaryMessage("PowerSave already ON");
								} else {
									config.power.is_power_saving = true;
									LOG_INFO("User toggled PowerSave");
									nodeDB->saveToDisk();
									rebootAtMsec = millis() + 2000;
									showTemporaryMessage("PowerSave ON\nReboot");
								}
							} else if (this->freetext == "ps0") {
								if (!config.power.is_power_saving) {
									showTemporaryMessage("PowerSave already OFF");
								} else {
									config.power.is_power_saving = false;
									LOG_INFO("User toggled PowerSave");
									nodeDB->saveToDisk();
									rebootAtMsec = millis() + 2000;
									showTemporaryMessage("PowerSave OFF\nReboot");
								}
							} else if (this->freetext == "c") { // clear all previous messages
																									// check if it's from SP2, and if so, send msg to SP4 'clr'
								// below is for auto clearing other device
								// if (nodeDB->getNodeNum() == NODENUM_SP2) {
								// 	sendText(NODENUM_SP4, 0, "clr", false);
								// } else if (nodeDB->getNodeNum() == NODENUM_SP4) {
								// 	sendText(NODENUM_SP2, 0, "clr", false);
								// }
								if ((nodeDB->getNodeNum() == NODENUM_SP2) && (this->dest == NODENUM_SP4)) {
									sendText(NODENUM_SP4, 0, "c", false);
									showTemporaryMessage("Cleared other");
									delay(400);
									char clrMessage[20];
									snprintf(clrMessage, sizeof(clrMessage), "sp4 RCLR");
									sendText(NODENUM_RPI5, 0, clrMessage, false);
								} else if ((nodeDB->getNodeNum() == NODENUM_SP4) && (this->dest == NODENUM_SP2)) {
									sendText(NODENUM_SP2, 0, "c", false);
									showTemporaryMessage("Cleared other");
									delay(400);
									char clrMessage[20];
									snprintf(clrMessage, sizeof(clrMessage), "sp2 RCLR");
									sendText(NODENUM_RPI5, 0, clrMessage, false);
								} else {
									screen->clearHistory();
									showTemporaryMessage("Cleared all\nprevious messages");
									// externalNotificationModule->setExternalOff(0); // this will turn off all GPIO and sounds and idle the loop
									char clrMessage[20];
									delay(500);
									snprintf(clrMessage, sizeof(clrMessage), "%s CLR", cannedMessageModule->getNodeName(nodeDB->getNodeNum()));
									sendText(NODENUM_RPI5, 0, clrMessage, false);
									alreadySentFirstMessage = 1; //screen.cpp can't do this
							  }
							} else if ((this->freetext == "rndb") || (this->freetext == "ndbr")) {
								nodeDB->resetNodes();
								showTemporaryMessage("Reset NodeDB");
							} else if ((this->freetext == "restart") || (this->freetext == "reboot")) {
								screen->startAlert("Rebooting...");
                rebootAtMsec = millis() + DEFAULT_REBOOT_SECONDS * 1000;
                runState = CANNED_MESSAGE_RUN_STATE_INACTIVE;
								//below could be useful in future
							// } else if ((this->freetext == "ignore") || (this->freetext == "ig")) {
							// 		MYNODES.erase(
							// 				std::remove_if(MYNODES.begin(), MYNODES.end(), 
							// 											 [&](const std::pair<unsigned int, std::string>& node) {
							// 													 return node.first == this->dest;
							// 											 }),
							// 				MYNODES.end()
							// 		);
							// 	do {
							// 		nodeIndex = (nodeIndex - 1 + MYNODES.size()) % MYNODES.size(); // Decrement nodeIndex and wrap around
							// 	} while (std::string(cannedMessageModule->getNodeName(MYNODES[nodeIndex].first)) == "Unknown");
							// 	this->dest = MYNODES[nodeIndex].first;
							// 	showTemporaryMessage("Ignored\nNode");
							} else {
								bool sendToRouterOnlyExact = false;
								bool sendToRouterOnlyStart = false;
								// check if string starts with router prefixes
								for (const auto& command : commandsForRouterOnlyStarting) {
										if (strncmp(this->freetext.c_str(), command.c_str(), command.size()) == 0) {
												sendToRouterOnlyStart = true; break;
										}
								}
								// Check for exact matches
								for (const auto& command : commandsForRouterOnlyExact) {
										if (this->freetext.c_str() == command) { sendToRouterOnlyExact = true; break; }
								}
								if (sendToRouterOnlyStart || sendToRouterOnlyExact) this->dest = this->previousDest = NODENUM_RPI5;
								if (this->dest == NODENUM_BROADCAST) {
									LOG_DEBUG("WAS BROADCAST\n");
									if (this->freetext.length() < 4) return 0;
									// char allMessage[this->freetext.length() + 6];
									// strcpy(allMessage, "ALL: ");
									// strcat(allMessage, this->freetext.c_str());
									// sendText(this->dest, 3, allMessage, true); //goes to StA channel for the fathers, and MFS channel for MONASTERY_FRIENDS (must have the channels in correct order), or Varangians for SECURITY tdecks
									sendText(this->dest, 3, this->freetext.c_str(), true); //goes to StA channel for the fathers, and MFS channel for MONASTERY_FRIENDS (must have the channels in correct order), or Varangians for SECURITY tdecks
								} else sendText(this->dest, 0, this->freetext.c_str(), true);
								LOG_DEBUG("Sending message to %x: %s\n", this->dest, this->freetext.c_str());
								// first check to make sure this->freetext isn't in commandsForRouterOnlyExact
								if (!sendToRouterOnlyExact) {
									this->previousDest = this->dest;
									this->previousFreetext = this->freetext;
								}
								LOG_INFO("previousDest: %x, previousFreetext: %s\n", this->previousDest, this->previousFreetext.c_str());
								LOG_INFO("dest: %x, freetext: %s\n", this->dest, this->freetext.c_str());
								//disable scrolling mode after sending msg
								this->cursorScrollMode = false;
								screen->removeFunctionSymbal("Scrl"); // remove the S symbol from the bottom right corner
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
                    sendText(NODENUM_RPI5, 0, this->messages[this->currentMessageIndex], true);
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
				case 0x23: // # sign or Q key long hold, for exiting freetext
					LOG_INFO("Got case 0x23\n");
					LOG_INFO("was touch eventA: %d\n", this->wasTouchEvent);
					this->runState = CANNED_MESSAGE_RUN_STATE_INACTIVE;
					this->lastTouchMillis = millis();
            e.action = UIFrameEvent::Action::REGENERATE_FRAMESET; // We want to change the list of frames shown on-screen
            requestFocus(); // Tell Screen::setFrames that our module's frame should be shown, even if not "first" in the frameset
					this->notifyObservers(&e);
					break;
				case 0x7e: // mic / 0 key, clear line
					LOG_INFO("Got case 0x7e\n");
					LOG_INFO("this->runState: %d\n", this->runState);
					// 1-6-25 testing
					// if (screen->isOnPreviousMsgsScreen) {
					// 					LOG_INFO("Got 0x7e here3\n");
					// 					this->goBackToFirstPreviousMessage = true; // added 12-29 trying to fix going to 2nd prv msg on exit freetext mode
					// 					UIFrameEvent e;
					// 					e.action = UIFrameEvent::Action::REGENERATE_FRAMESET; // We want to change the list of frames shown on-screen
					// 					this->notifyObservers(&e);
					// 					// requestFocus(); // Tell Screen::setFrames that our module's frame should be shown, even if not "first" in the frameset
					// 					// validEvent = true;
					// } else
					if (this->freetext.length() > 0) {
						LOG_INFO("more than 1 char is on freetext line, deleting line\n");
						this->freetext = ""; // clear freetext
						this->cursor = 0;
						this->freetext = this->freetext.substring(0, this->freetext.length() - 1);
						requestFocus();
					} else { //return to router node
						LOG_INFO("zero pressed and there's no text, returning to router node\n");
						nodeIndex = 0;
						this->dest = MYNODES[nodeIndex].first;
					}
					break;
				case 0x1a: // alt-w/1, previous Messages1
#if !defined(SECURITY) && !defined(GATE_SECURITY)
					sendText(NODENUM_RPI5, 0, "1", false);
					showTemporaryMessage("Requesting Previous\nMessages 1");
#endif
					break;
				// case 0x2a: // alt-e/2, previous Messages2
				// // case 0x9d: // alt-e/2, previous Messages2
				// 	sendText(NODENUM_RPI5, 0, "2", false);
				// 	showTemporaryMessage("Requesting Previous\nMessages 2");
				// 	break;
				case 0x9e: // alt-r, retype last message
					LOG_INFO("Got ALT-R, Retype last message\n");
					if (this->previousFreetext.length() > 0) {
						if (this->previousDest == NODENUM_BROADCAST) this->previousDest = NODENUM_RPI5;
						this->freetext = this->previousFreetext;
						this->cursor = this->freetext.length();
						this->runState = CANNED_MESSAGE_RUN_STATE_FREETEXT;
						screen->isOnPreviousMsgsScreen = false;
            this->destSelect = CANNED_MESSAGE_DESTINATION_TYPE_NODE;
            // e.action = UIFrameEvent::Action::REGENERATE_FRAMESET; // We want to change the list of frames shown on-screen
						// this->lastTouchMillis = millis();
						// this->notifyObservers(&e);
						this->skipNextRletter = true;
						requestFocus();
            // setIntervalFromNow(0); // on fast keypresses, this isn't fast enough.
            // runOnce();
            // validEvent = true;
						// showTemporaryMessage("Retyping last message");
					} else {
						showTemporaryMessage("No previous\nmessage to retype");
					}
					break;

					// LOG_INFO("Got ALT-R, Resend last message\n");
					// LOG_INFO("Got ALT-R, Resend last message\n");
					// if (this->previousFreetext.length() > 0) {
					// 	if (this->previousDest == NODENUM_BROADCAST) this->previousDest = NODENUM_RPI5;
					// 	LOG_INFO("Resending previous message to %d: %s\n", this->previousDest, this->previousFreetext.c_str());
					// 	sendText(this->previousDest, 1, this->previousFreetext.c_str(), true);
					// 	showTemporaryMessage("Resending last message");
					// } else {
					// 	showTemporaryMessage("No previous\nmessage to resend");
					// }
					// break;
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
				case 0x22: // " , toggle new keyboard lock mode
					if (this->freetext.length() > 0) break;
					LOG_INFO("Got \", Toggle Keyboard Lock Mode\n");
					if (!screen->keyboardLockMode) {
						LOG_INFO("Keyboard Lock Mode on\n");
						screen->keyboardLockMode = true;
						this->runState = CANNED_MESSAGE_RUN_STATE_INACTIVE; //prevents entering freetext mode
						// this->skipNextFreetextMode = true;
						screen->setFunctionSymbal("KL");
						UIFrameEvent e;
						e.action = UIFrameEvent::Action::REGENERATE_FRAMESET; // We want to change the list of frames shown on-screen
						this->notifyObservers(&e);
					} else {
						LOG_INFO("Keyboard Lock Mode off\n");
						screen->keyboardLockMode = false;
						this->runState = CANNED_MESSAGE_RUN_STATE_INACTIVE; //prevents entering freetext mode
						// this->skipNextFreetextMode = true;
						screen->removeFunctionSymbal("KL");
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
				// case 0x60: //testing 12-27 for strange apostrophe bug, try make 60 count as 5f
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
#endif
        case 0xb4: // left
			    if (screen->keyboardLockMode) break;
#ifdef SIMPLE_TDECK
// this always allows to change the destination with scrolling
          if (1 == 1) {
#else
          if (this->destSelect == CANNED_MESSAGE_DESTINATION_TYPE_NODE) {
                size_t numMeshNodes = nodeDB->getNumMeshNodes();
                if (this->dest == NODENUM_BROADCAST) {
                    this->dest = nodeDB->getNodeNum();
                }
#endif
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
						// if (!this->cursorScrollMode && this->touchDirection != 1) { // 12-27 it's possible the touchdirection check here fixed scrolling through nodes on swipe up-down to enter freetext. It might've messed up something else though.
						// if (!this->cursorScrollMode && !this->justEnteredFreetext) { //12-29 removed this because it was doing something weird on first left scroll, doing cursorScroll instead of moving through nodes
						if (!this->cursorScrollMode) {
							LOG_INFO("this->runStateHere: %d\n", this->runState);
								LOG_INFO("CursorScrollMode is off\n");
							// do {
							// 	LOG_INFO("CursorScrollMode is off, do while\n");
							// 	nodeIndex = (nodeIndex + 1) % MYNODES.size();
							// 	LOG_INFO("NodeIndex: %d\n", nodeIndex);
							// 	LOG_INFO("NodeName: %s\n", getNodeNameByIndex(MYNODES, nodeIndex).c_str());
							// } while (std::string(cannedMessageModule->getNodeName(MYNODES[nodeIndex].first)) == "Unknown");
								// scrollLeft();
							nodeIndex = this->scrollLeft();
							// nodeIndex = (nodeIndex + 1) % MYNODES.size();

							// LOG_INFO("NodeIndex: %d\n", nodeIndex);
							// LOG_INFO("NodeName: %s\n", getNodeNameByIndex(MYNODES, nodeIndex).c_str());

			// snprintf(startupMessage, sizeof(startupMessage), "%s ON", cannedMessageModule->getNodeName(nodeDB->getNodeNum()));
							// 		nodeName = cannedMessageModule->getNodeName(nodeDB->getMeshNodeByIndex(nextNode)->num);
							// LOG_INFO("NodeIndex: %d\n", nodeIndex);
							// LOG_INFO("NodeName: %s\n", getNodeNameByIndex(MYNODES, nodeIndex).c_str());
							this->dest = MYNODES[nodeIndex].first;
							// LOG_INFO("Dest: %d\n", this->dest);
							// LOG_INFO("nodeNameeee: %d\n", getNodeNameByIndex(MYNODES, nodeIndex).c_str());
							// nodeDB->getMeshNode(3664080480));
							// this->dest = nodeDB->getMeshNodeByIndex(nextNode)->num;
						} else {
							LOG_INFO("CursorScrollMode is on, LEFT\n");
							if (this->wasTouchEvent) {
								this->wasTouchEvent = false;
								if (this->cursor < this->freetext.length()) this->cursor++;
							} else {
                if (this->cursor > 0) this->cursor--;
							}
						}
						this->justEnteredFreetext = false;

#endif
            } else {
                if (this->cursor > 0) {
                    this->cursor--;
                }
            }
            break;
        case 0xb7: // right
#ifdef SIMPLE_TDECK
			    if (screen->keyboardLockMode) break;
          if (1 == 1) {
// this always allows to change the destination with scrolling
#else
          if (this->destSelect == CANNED_MESSAGE_DESTINATION_TYPE_NODE) {
                size_t numMeshNodes = nodeDB->getNumMeshNodes();
                if (this->dest == NODENUM_BROADCAST) {
                    this->dest = nodeDB->getNodeNum();
                }
#endif
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
						if (!this->cursorScrollMode) {
							LOG_INFO("CursorScrollMode is off\n");
							// do {
							// 	LOG_INFO("CursorScrollMode is off, do while\n");
							// 	nodeIndex = (nodeIndex - 1 + MYNODES.size()) % MYNODES.size(); // Decrement nodeIndex and wrap around
							// 	LOG_INFO("NodeIndex: %d\n", nodeIndex);
							// 	LOG_INFO("NodeName: %s\n", getNodeNameByIndex(MYNODES, nodeIndex).c_str());
							// } while (std::string(cannedMessageModule->getNodeName(MYNODES[nodeIndex].first)) == "Unknown");
								// scrollRight();
							nodeIndex = this->scrollRight();
							// nodeIndex = (nodeIndex - 1 + MYNODES.size()) % MYNODES.size(); // Decrement nodeIndex and wrap around

							// LOG_INFO("NodeIndex: %d\n", nodeIndex);
							// LOG_INFO("NodeName: %s\n", getNodeNameByIndex(MYNODES, nodeIndex).c_str());
							this->dest = MYNODES[nodeIndex].first;
							LOG_INFO("Dest: %d\n", this->dest);
						} else {
							LOG_INFO("CursorScrollMode is on, RIGHT\n");
							if (this->wasTouchEvent) {
								this->wasTouchEvent = false;
                if (this->cursor > 0) this->cursor--;
							} else {
								if (this->cursor < this->freetext.length()) this->cursor++;
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
						case 0x23: // # sign, for exiting freetext
						case 0x7e: // mic / 0 key, clear line
						case 0x1e: // shift-$, toggle brightness
						case 0x3c: // shift-speaker toggle brightness, some tdecks with black keyboards
						case 0x24: // $ sign
						case 0x1f: // alt-f, flashlight
						// case 0x60: // testing 12-27
						case 0x5f: // _, toggle cursorScrollMode
						case 0x1a: // alt-1, previous messages 1
						// case 0x2a: // alt-2, previous messages 2
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

    // If using "scan and select" input, don't draw the module frame just to say "disabled"
    // The scanAndSelectInput class will draw its own temporary alert for user, when the input button is pressed
    else if (scanAndSelectInput != nullptr && !hasMessages())
        return false;

    return (currentMessageIndex != -1) || (this->runState != CANNED_MESSAGE_RUN_STATE_INACTIVE);
}

// Has the user defined any canned messages?
// Expose publicly whether canned message module is ready for use
bool CannedMessageModule::hasMessages()
{
    return (this->messagesCount > 0);
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
    //NOTE: below was enabled, frc disabled it when merging 2.4.3, because it wasn't present in newest firmware. Testing
		//requestFocus(); // Tell Screen::setFrames to move to our module's frame, next time it runs
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
        requestFocus();                        // Tell Screen::setFrames to move to our module's frame
        EINK_ADD_FRAMEFLAG(display, COSMETIC); // Clean after this popup. Layout makes ghosting particularly obvious

#ifdef SIMPLE_TDECK
        display->setFont(FONT_LARGE);
#else
#ifdef USE_EINK
        display->setFont(FONT_SMALL); // No chunky text
#else
        display->setFont(FONT_MEDIUM); // Chunky text
#endif
#endif

        String displayString;
        display->setTextAlignment(TEXT_ALIGN_CENTER);
#ifdef SIMPLE_TDECK
        if (this->ack) {
					// TODO: below is just for testing trying to get rid of black screen
      //       if (this->runState == CANNED_MESSAGE_RUN_STATE_ACK_NACK_RECEIVED) {
						// 	LOG_INFO("ACK NACK RECEIVED\n");
						// } else if (this->runState == CANNED_MESSAGE_RUN_STATE_SENDING_ACTIVE) {
						// 	LOG_INFO("SENDING ACTIVE\n");
						// }

            // displayString = "En route...\n";
            // displayString = "Delivering...\n";
						if (deliveryStatus != 2) setDeliveryStatus(1);  // this prevents problem where if you're sending to a nearby node, right after getting set to 2 it gets immediately set back to 1 and so the (D) doesn't show at all
        // display->drawString(display->getWidth() / 2 - 50, 0 + y + 12 + (3 * FONT_HEIGHT_LARGE), "En route...");
        } else {
					// removed below 1-6-25
					// if ((this->deliveryFailedCount == 0) && (this->previousFreetext.length() > 0)) {
					// 	this->deliveryFailedCount = 1;
     //        // displayString = "Delivery failed\nRetrying...";
					// 	showTemporaryMessage("Delivery failed\nRetrying...");
					// 	LOG_DEBUG("Resending previous message to %x: %s\n", this->previousDest, this->previousFreetext.c_str());
					// 	sendText(this->previousDest, 0, this->previousFreetext.c_str(), true);
					// } else {
						// this->deliveryFailedCount = 0;
						setDeliveryStatus(0);
            // displayString = "Delivery failed";
						showTemporaryMessage("Delivery failed\n");
					// }
				}
#else
        if (this->ack) {
            displayString = "Delivered to\n%s";
        } else {
            displayString = "Delivery failed\nto %s";
        }
        display->drawStringf(display->getWidth() / 2 + x, 0 + y + 12, buffer, displayString,
                             cannedMessageModule->getNodeName(this->incoming));
        display->setFont(FONT_SMALL);

        String snrString = "Last Rx SNR: %f";
        String rssiString = "Last Rx RSSI: %d";

        // Don't bother drawing snr and rssi for tiny displays
        if (display->getHeight() > 100) {

            // Original implemenation used constants of y = 100 and y = 130. Shrink this if screen is *slightly* small
            int16_t snrY = 100;
            int16_t rssiY = 130;

            // If dislay is *slighly* too small for the original consants, squish up a bit
            if (display->getHeight() < rssiY) {
                snrY = display->getHeight() - ((1.5) * FONT_HEIGHT_SMALL);
                rssiY = display->getHeight() - ((2.5) * FONT_HEIGHT_SMALL);
            }

            if (this->ack) {
                display->drawStringf(display->getWidth() / 2 + x, snrY + y, buffer, snrString, this->lastRxSnr);
                display->drawStringf(display->getWidth() / 2 + x, rssiY + y, buffer, rssiString, this->lastRxRssi);
            }
        }
#endif
#ifdef SIMPLE_TDECK
				// } // for noack above
#endif
    } else if (cannedMessageModule->runState == CANNED_MESSAGE_RUN_STATE_SENDING_ACTIVE) {
        // E-Ink: clean the screen *after* this pop-up
        EINK_ADD_FRAMEFLAG(display, COSMETIC);

        requestFocus(); // Tell Screen::setFrames to move to our module's frame
#ifdef SIMPLE_TDECK
        display->setFont(FONT_LARGE);
        display->drawString(display->getWidth() / 2 - 50, 0 + y + 12 + (3 * FONT_HEIGHT_LARGE), "Sending...");
#else
#ifdef USE_EINK
        display->setFont(FONT_SMALL); // No chunky text
#else
        display->setFont(FONT_MEDIUM); // Chunky text
#endif
        display->drawString(display->getWidth() / 2 + x, 0 + y + 12 + (3 * FONT_HEIGHT_LARGE), "Sending...");
#endif
    }
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
            // display->drawStringf(1 + x, 0 + y, buffer, "To: %s", cannedMessageModule->getNodeName(this->dest));
            // display->drawStringf(0 + x, 0 + y, buffer, "To: %s", cannedMessageModule->getNodeName(this->dest));
                    // if (nodeDB->getMeshNodeByIndex(i)->num == this->dest) {
						// LOG_INFO("NodeName: %s\n", getNodeNameByIndex(MYNODES, nodeIndex).c_str());
            display->setColor(WHITE);
            display->fillRect(0 + x, 0 + y, x + display->getWidth(), y + FONT_HEIGHT_LARGE);
            display->setColor(BLACK);
            display->drawStringf(0 + x, 0 + y, buffer, "To: %s", getNodeNameByIndex(MYNODES, nodeIndex).c_str());
            // display->drawStringf(0 + x, 0 + y, buffer, "To: %s", getNodeNameByIndex(MYNODES, nodeIndex).c_str());
            display->setColor(WHITE);
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
						// display->drawStringf(0 + x, 0 + y, buffer, "To: %s", cannedMessageModule->getNodeName(this->dest));
						// LOG_INFO("NodeName: %s\n", getNodeNameByIndex(MYNODES, nodeIndex).c_str());
            display->setColor(WHITE);
            display->fillRect(0 + x, 0 + y, x + display->getWidth(), y + FONT_HEIGHT_LARGE);
            display->setColor(BLACK);
						display->drawStringf(0 + x, 0 + y, buffer, "To: %s", getNodeNameByIndex(MYNODES, nodeIndex).c_str());
						// display->drawStringf(1 + x, 0 + y, buffer, "To: %s", getNodeNameByIndex(MYNODES, nodeIndex).c_str());
            display->setColor(WHITE);
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
#endif
        display->drawStringMaxWidth(
            0 + x, 0 + y + FONT_HEIGHT_LARGE, x + display->getWidth(),
            cannedMessageModule->drawWithCursor(cannedMessageModule->freetext, cannedMessageModule->cursor));
#endif  //end for t-watch
    } else {
        if (this->messagesCount > 0) {
            display->setTextAlignment(TEXT_ALIGN_LEFT);
#ifdef SIMPLE_TDECK
            display->setFont(FONT_LARGE);
            // display->drawStringf(0 + x, 0 + y, buffer, "To: %s", cannedMessageModule->getNodeName(this->dest));
						LOG_INFO("NodeName: %s\n", getNodeNameByIndex(MYNODES, nodeIndex).c_str());
            display->setColor(WHITE);
            display->fillRect(0 + x, 0 + y, x + display->getWidth(), y + FONT_HEIGHT_LARGE);
            display->setColor(BLACK);
            display->drawStringf(0 + x, 0 + y, buffer, "To: %s", getNodeNameByIndex(MYNODES, nodeIndex).c_str());
            // display->drawStringf(1 + x, 0 + y, buffer, "To: %s", getNodeNameByIndex(MYNODES, nodeIndex).c_str());
            display->setColor(WHITE);
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
#ifdef USE_EINK
                        // Avoid drawing solid black with fillRect: harder to clear for E-Ink
                        display->drawString(0 + x, 0 + y + FONT_HEIGHT_SMALL * (i + 1), ">");
                        display->drawString(12 + x, 0 + y + FONT_HEIGHT_SMALL * (i + 1),
                                            cannedMessageModule->getCurrentMessage());
#else
                        display->fillRect(0 + x, 0 + y + FONT_HEIGHT_SMALL * (i + 1), x + display->getWidth(),
                                          y + FONT_HEIGHT_SMALL);
                        display->setColor(BLACK);
                        display->drawString(0 + x, 0 + y + FONT_HEIGHT_SMALL * (i + 1), cannedMessageModule->getCurrentMessage());
                        display->setColor(WHITE);
#endif
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
#ifndef SIMPLE_TDECK
            UIFrameEvent e;
            e.action = UIFrameEvent::Action::REGENERATE_FRAMESET; // We want to change the list of frames shown on-screen
            requestFocus(); // Tell Screen::setFrames that our module's frame should be shown, even if not "first" in the frameset
            this->runState = CANNED_MESSAGE_RUN_STATE_ACK_NACK_RECEIVED;
#else
						if (this->ack && deliveryStatus != 2) setDeliveryStatus(1);
#endif
            this->incoming = service->getNodenumFromRequestId(mp.decoded.request_id);
            meshtastic_Routing decoded = meshtastic_Routing_init_default;
            pb_decode_from_bytes(mp.decoded.payload.bytes, mp.decoded.payload.size, meshtastic_Routing_fields, &decoded);
            this->ack = decoded.error_reason == meshtastic_Routing_Error_NONE;
            waitingForAck = false; // No longer want routing packets
#ifndef SIMPLE_TDECK
            this->notifyObservers(&e);
#endif
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
                          &cannedMessageModuleConfig) != LoadFileResult::LOAD_SUCCESS) {
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
