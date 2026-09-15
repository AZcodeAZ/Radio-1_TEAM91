#pragma once
#include "status_led.h"

void radioControlBegin();
void radioSetMode(RadioMode mode);
RadioMode radioGetMode();