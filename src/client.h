#pragma once

#include <string>

// The shortcut path: a dictation key runs `soz start` or `soz stop`, which
// must return in a few milliseconds. This talks to the daemon over its socket
// with plain POSIX calls, before Qt is ever initialised.
namespace Client {

// Sends one command and prints the reply. Starts the service and waits for
// its socket when it is not running. Returns the process exit code.
int send(const std::string &command, bool startService);

// True when the daemon answers on its socket.
bool reachable();

}
