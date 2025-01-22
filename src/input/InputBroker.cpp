#include "InputBroker.h"
#include "PowerFSM.h" // needed for event trigger
extern bool keyboardLockMode;

InputBroker *inputBroker = nullptr;

InputBroker::InputBroker(){};

void InputBroker::registerSource(Observable<const InputEvent *> *source)
{
    this->inputEventObserver.observe(source);
}

int InputBroker::handleInputEvent(const InputEvent *event)
{
#ifdef SIMPLE_TDECK
	// if (keyboardLockMode) powerFSM.trigger(EVENT_DARK);
	// else powerFSM.trigger(EVENT_INPUT);
	if (!keyboardLockMode) powerFSM.trigger(EVENT_INPUT);
#else
    powerFSM.trigger(EVENT_INPUT);
#endif
    this->notifyObservers(event);
    return 0;
}
