#pragma once

#include <QString>

// Turns Turkish dictation on and off. "On" means soz is Omarchy's dictation
// backend, so Alt + Space and Right Alt reach soz instead of Superwhisper;
// "off" hands them back to whatever backend soz replaced.
namespace Switcher {

bool enabled();
int on();
int off(bool quiet);
int toggle();

// One line for scripts and the bar: on|off plus the live phase.
int printState();

// Which backend Omarchy dictation uses now ("superwhisper", "soz", ...).
QString currentBackend();

}
