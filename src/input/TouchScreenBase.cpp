#include "TouchScreenBase.h"
// #define DISABLE_VERTICAL_SWIPE
#ifdef SIMPLE_TDECK
#include "modules/CannedMessageModule.h"
#endif
#include "main.h"

#ifndef TIME_LONG_PRESS
#define TIME_LONG_PRESS 400
#endif

// move a minimum distance over the screen to detect a "swipe"
#ifndef TOUCH_THRESHOLD_X
#define TOUCH_THRESHOLD_X 30
#endif

#ifndef TOUCH_THRESHOLD_Y
#define TOUCH_THRESHOLD_Y 20
#endif

TouchScreenBase::TouchScreenBase(const char *name, uint16_t width, uint16_t height)
    : concurrency::OSThread(name), _display_width(width), _display_height(height), _first_x(0), _last_x(0), _first_y(0),
      _last_y(0), _start(0), _tapped(false), _originName(name)
{
}

void TouchScreenBase::init(bool hasTouch)
{
    if (hasTouch) {
        LOG_INFO("TouchScreen initialized %d %d\n", TOUCH_THRESHOLD_X, TOUCH_THRESHOLD_Y);
        this->setInterval(100);
    } else {
        disable();
        this->setInterval(UINT_MAX);
    }
}

int32_t TouchScreenBase::runOnce()
{
    TouchEvent e;
    e.touchEvent = static_cast<char>(TOUCH_ACTION_NONE);

    // process touch events
    int16_t x, y;
    bool touched = getTouch(x, y);
    if (touched) {
        this->setInterval(20);
        _last_x = x;
        _last_y = y;
    }
    if (touched != _touchedOld) {
        if (touched) {
            hapticFeedback();
            _state = TOUCH_EVENT_OCCURRED;
            _start = millis();
            _first_x = x;
            _first_y = y;
        } else {
            _state = TOUCH_EVENT_CLEARED;
            time_t duration = millis() - _start;
            x = _last_x;
            y = _last_y;
            this->setInterval(50);

            // compute distance
            int16_t dx = x - _first_x;
            int16_t dy = y - _first_y;
            uint16_t adx = abs(dx);
            uint16_t ady = abs(dy);

            // swipe horizontal
            if (adx > ady && adx > TOUCH_THRESHOLD_X) {
#ifdef SIMPLE_TDECK
							if (!screen->keyboardLockMode) {
#endif
                if (0 > dx) { // swipe right to left
                    e.touchEvent = static_cast<char>(TOUCH_ACTION_LEFT);
#ifdef SIMPLE_TDECK
										cannedMessageModule->wasTouchEvent = true;
#endif
                    LOG_DEBUG("action SWIPE: right to left\n");
                } else { // swipe left to right
                    e.touchEvent = static_cast<char>(TOUCH_ACTION_RIGHT);
#ifdef SIMPLE_TDECK
										cannedMessageModule->wasTouchEvent = true;
#endif
                    LOG_DEBUG("action SWIPE: left to right\n");
									}
#ifdef SIMPLE_TDECK
                }
#endif
            }
#ifndef DISABLE_VERTICAL_SWIPE
            // swipe vertical
            else if (ady > adx && ady > TOUCH_THRESHOLD_Y) {
#ifdef SIMPLE_TDECK
							if (!screen->keyboardLockMode) {
#endif
                if (0 > dy) { // swipe bottom to top
#ifdef SIMPLE_TDECK
    InputEvent e;
#endif
                    // e.touchEvent = static_cast<char>(TOUCH_ACTION_UP);
    char eventUp2 = static_cast<char>(meshtastic_ModuleConfig_CannedMessageConfig_InputEventChar_UP);
        // e.inputEvent = this->eventUp2;
#ifdef SIMPLE_TDECK
										cannedMessageModule->wasTouchEvent = false;
				InputEvent trackballEvent;
        trackballEvent.inputEvent = static_cast<char>(meshtastic_ModuleConfig_CannedMessageConfig_InputEventChar_UP);
        trackballEvent.source = "trackball1";
        trackballEvent.kbchar = 0;
        inputBroker->handleInputEvent(&trackballEvent);
#endif
                    LOG_DEBUG("action SWIPE: bottom to top\n");
                } else { // swipe top to bottom
                    // e.touchEvent = static_cast<char>(TOUCH_ACTION_DOWN);
    // char eventDown2 = static_cast<char>(meshtastic_ModuleConfig_CannedMessageConfig_InputEventChar_DOWN);
        // e.inputEvent = this->eventDown2;
#ifdef SIMPLE_TDECK
										cannedMessageModule->wasTouchEvent = false;
				InputEvent trackballEvent;
        trackballEvent.inputEvent = static_cast<char>(meshtastic_ModuleConfig_CannedMessageConfig_InputEventChar_DOWN);
        trackballEvent.source = "trackball1";
        trackballEvent.kbchar = 0;
        inputBroker->handleInputEvent(&trackballEvent);
        // e.source = 'trackball1';
        // e.kbchar = 0x00;
        // this->notifyObservers(&e);
    // this->action = TB_ACTION_NONE;
#endif
                    LOG_DEBUG("action SWIPE: top to bottom\n");
                }
#ifdef SIMPLE_TDECK
                }
#endif
            }
#endif
            // tap
            else {
#ifdef SIMPLE_TDECK
							if (!screen->keyboardLockMode) {
#endif
                if (duration > 0 && duration < TIME_LONG_PRESS) {
                    if (_tapped) {
                        _tapped = false;
                        e.touchEvent = static_cast<char>(TOUCH_ACTION_DOUBLE_TAP);
                        LOG_DEBUG("action DOUBLE TAP(%d/%d)\n", x, y);
                    } else {
                        _tapped = true;
                    }
                } else {
                    _tapped = false;
                }
            }
        }
#ifdef SIMPLE_TDECK
        }
#endif
    }
    _touchedOld = touched;

    // fire TAP event when no 2nd tap occured within time
    if (_tapped && (time_t(millis()) - _start) > TIME_LONG_PRESS - 50) {
#ifdef SIMPLE_TDECK
		if (!screen->keyboardLockMode) {
#endif
        _tapped = false;
        e.touchEvent = static_cast<char>(TOUCH_ACTION_TAP);
        LOG_DEBUG("action TAP(%d/%d)\n", _last_x, _last_y);
#ifdef SIMPLE_TDECK
		}
#endif
    }

    // fire LONG_PRESS event without the need for release
    if (touched && (time_t(millis()) - _start) > TIME_LONG_PRESS) {
#ifdef SIMPLE_TDECK
		if (!screen->keyboardLockMode) {
#endif
        // tricky: prevent reoccurring events and another touch event when releasing
        _start = millis() + 30000;
        e.touchEvent = static_cast<char>(TOUCH_ACTION_LONG_PRESS);
        LOG_DEBUG("action LONG PRESS(%d/%d)\n", _last_x, _last_y);
#ifdef SIMPLE_TDECK
		}
#endif
    }

    if (e.touchEvent != TOUCH_ACTION_NONE) {
        e.source = this->_originName;
        e.x = _last_x;
        e.y = _last_y;
        onEvent(e);
    }

    return interval;
}

void TouchScreenBase::hapticFeedback()
{
#ifdef T_WATCH_S3
    drv.setWaveform(0, 75);
    drv.setWaveform(1, 0); // end waveform
    drv.go();
#endif
}
