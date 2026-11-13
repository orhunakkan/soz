#include "paths.h"

#include <QDir>
#include <QFileInfo>
#include <QSaveFile>
#include <QStandardPaths>
#include <QtGlobal>

#include <unistd.h>

namespace {

QString envOr(const char *name, const QString &fallback) {
    const QString value = qEnvironmentVariable(name);
    return value.isEmpty() ? fallback : value;
}

QString home() { return QDir::homePath(); }

}

namespace Paths {

QString runtimeDir() {
    return envOr("XDG_RUNTIME_DIR", QStringLiteral("/run/user/%1").arg(getuid())) + "/soz";
}

QString socket() { return runtimeDir() + "/soz.sock"; }
QString enabledFile() { return runtimeDir() + "/enabled"; }
QString phaseFile() { return runtimeDir() + "/phase"; }
QString lastFile() { return runtimeDir() + "/last.json"; }

QString modelDir() {
    const QString override = qEnvironmentVariable("SOZ_MODEL_DIR");
    if (!override.isEmpty())
        return override;
    return envOr("XDG_DATA_HOME", home() + "/.local/share") + "/soz/models";
}

QString modelFile() {
    const QString override = qEnvironmentVariable("SOZ_MODEL");
    return override.isEmpty() ? modelDir() + "/ggml-large-v3-turbo-q8_0.bin" : override;
}

QString stateDir() { return envOr("XDG_STATE_HOME", home() + "/.local/state") + "/soz"; }
QString previousBackendFile() { return stateDir() + "/previous-backend"; }

QString shortcutsLua() { return envOr("XDG_CONFIG_HOME", home() + "/.config") + "/soz/shortcuts.lua"; }

QString backendShortcutsLua(const QString &backend) {
    return envOr("XDG_CONFIG_HOME", home() + "/.config") + "/" + backend + "/shortcuts.lua";
}

QString shellJson() { return envOr("XDG_CONFIG_HOME", home() + "/.config") + "/omarchy/shell.json"; }

bool writeAtomically(const QString &path, const QByteArray &data) {
    QDir().mkpath(QFileInfo(path).absolutePath());
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly))
        return false;
    file.write(data);
    return file.commit();
}

}
