#pragma once
#include "Observer.h"

#define ANYKEY 0xFF
#define MATRIXKEY 0xFE
#define SIMPLE_TDECK2

typedef struct _InputEvent {
    const char *source;
    char inputEvent;
    char kbchar;
    uint16_t touchX;
    uint16_t touchY;
} InputEvent;
class InputBroker : public Observable<const InputEvent *>
{
    CallbackObserver<InputBroker, const InputEvent *> inputEventObserver =
        CallbackObserver<InputBroker, const InputEvent *>(this, &InputBroker::handleInputEvent);

  public:
    InputBroker();
    void registerSource(Observable<const InputEvent *> *source);
#ifdef SIMPLE_TDECK2
    int handleInputEvent(const InputEvent *event);
#endif

#ifndef SIMPLE_TDECK2
  protected:
    int handleInputEvent(const InputEvent *event);
#endif
};

extern InputBroker *inputBroker;
