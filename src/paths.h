#pragma once

#include <QString>

// Every file soz reads or writes, in one place so the daemon, the client,
// the switch and the bar widget agree on them.
namespace Paths {

// $XDG_RUNTIME_DIR/soz: the socket and the live state the bar watches.
// Cleared at logout, which is what keeps "on" from surviving a reboot.
QString runtimeDir();
QString socket();
QString enabledFile();   // "1" while Turkish dictation owns the shortcuts
QString phaseFile();     // idle | loading | recording | transcribing | error
QString lastFile();      // the most recent dictation as JSON

// ~/.local/share/soz/models
QString modelDir();
QString modelFile();

// ~/.local/state/soz: the backend soz replaced, restored when it turns off.
QString stateDir();
QString previousBackendFile();

// ~/.config/soz/shortcuts.lua, which Omarchy loads for the active backend.
QString shortcutsLua();

// A dictation backend's own shortcut integration, which soz keeps loaded.
QString backendShortcutsLua(const QString &backend);

// The Omarchy shell's settings, which hold the bar layout.
QString shellJson();

// Writes text beside the target and renames it into place, so the bar's
// file watcher never reads a half-written file.
bool writeAtomically(const QString &path, const QByteArray &data);

}
