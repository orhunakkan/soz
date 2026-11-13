#pragma once

#include <QString>

// Types the dictation into the focused window through the Wayland virtual
// keyboard (wtype), which handles ç, ğ, ı, İ, ö, ş and ü and leaves the
// clipboard alone.
namespace Inserter {

// Returns an empty string on success, otherwise what went wrong.
QString type(const QString &text);

}
