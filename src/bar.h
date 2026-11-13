#pragma once

#include <QString>

// Adds the TR switch to the Omarchy bar or takes it off again, by editing
// ~/.config/omarchy/shell.json with jq so the rest of the file keeps its
// order and formatting. The shell reloads the file on save.
namespace Bar {

// The QML file the bar loads; SOZ_BAR_QML overrides it for tests and
// development builds.
QString widgetSource();

int on(bool quiet);
int off(bool quiet);

}
