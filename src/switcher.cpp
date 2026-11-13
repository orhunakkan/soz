#include "switcher.h"

#include "paths.h"

#include <QFile>
#include <QFileInfo>
#include <QProcess>
#include <QRegularExpression>
#include <QTextStream>

#include <cstdio>

namespace {

const QString kSelf = QStringLiteral("soz");
const QString kFallback = QStringLiteral("superwhisper");

QString run(const QString &program, const QStringList &args, int *exitCode = nullptr) {
    QProcess process;
    process.start(program, args);
    process.waitForFinished(15000);
    if (exitCode)
        *exitCode = process.exitStatus() == QProcess::NormalExit ? process.exitCode() : 1;
    return QString::fromUtf8(process.readAllStandardOutput() + process.readAllStandardError()).trimmed();
}

QString readFile(const QString &path) {
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        return QString();
    return QString::fromUtf8(file.readAll()).trimmed();
}

// Lua string literal for a path: backslashes and quotes escaped.
QByteArray luaString(const QString &value) {
    QByteArray out = value.toUtf8();
    out.replace("\\", "\\\\").replace("\"", "\\\"");
    return "\"" + out + "\"";
}

// Omarchy loads ~/.config/<backend>/shortcuts.lua for the selected backend
// when Hyprland starts. A backend such as Superwhisper keeps extra keys there
// (Ctrl + Alt + Space, Escape), so soz's file loads the replaced backend's
// file: its extra shortcuts keep working even if Hyprland restarts while soz
// is on.
void writeShortcutsLua(const QString &previous) {
    QByteArray lua =
        "-- Written by soz. Omarchy loads this while soz is the dictation backend.\n"
        "-- soz needs no shortcuts beyond Omarchy's dictation keys, so it keeps the\n"
        "-- shortcuts of the backend it replaced loaded.\n";
    if (!previous.isEmpty()) {
        lua += "local path = " + luaString(Paths::backendShortcutsLua(previous)) + "\n"
               "local file = io.open(path, \"r\")\n"
               "if file then\n"
               "  file:close()\n"
               "  dofile(path)\n"
               "end\n";
    }
    if (readFile(Paths::shortcutsLua()).toUtf8() != lua.trimmed())
        Paths::writeAtomically(Paths::shortcutsLua(), lua);
}

bool validBackend(const QString &name) {
    static const QRegularExpression pattern(QStringLiteral("^[a-z0-9][a-z0-9-]*$"));
    return pattern.match(name).hasMatch();
}

void setEnabledFile(bool on) {
    Paths::writeAtomically(Paths::enabledFile(), on ? "1" : "0");
}

}

namespace Switcher {

QString currentBackend() {
    int code = 0;
    const QString backend = run(QStringLiteral("omarchy-default-dictation"), {}, &code);
    return code == 0 ? backend : QString();
}

bool enabled() { return currentBackend() == kSelf; }

int on() {
    if (!QFileInfo::exists(Paths::modelFile())) {
        fprintf(stderr, "The speech model is missing (%s). Run `soz download` first.\n",
                qPrintable(Paths::modelFile()));
        return 1;
    }

    QString previous = currentBackend();
    if (previous == kSelf || !validBackend(previous))
        previous = readFile(Paths::previousBackendFile());
    if (!validBackend(previous) || previous == kSelf)
        previous = kFallback;
    Paths::writeAtomically(Paths::previousBackendFile(), previous.toUtf8());

    writeShortcutsLua(previous);

    // Loading the model takes a second or two; recording works meanwhile.
    run(QStringLiteral("systemctl"), {QStringLiteral("--user"), QStringLiteral("start"),
                                      QStringLiteral("--no-block"), QStringLiteral("soz.service")});

    int code = 0;
    const QString output = run(QStringLiteral("omarchy-dictation-use"), {kSelf}, &code);
    if (code != 0) {
        fprintf(stderr, "Could not make soz the dictation backend: %s\n", qPrintable(output));
        return 1;
    }
    setEnabledFile(true);
    printf("on\n");
    return 0;
}

int off(bool quiet) {
    if (currentBackend() == kSelf) {
        QString previous = readFile(Paths::previousBackendFile());
        if (!validBackend(previous) || previous == kSelf)
            previous = kFallback;
        int code = 0;
        const QString output = run(QStringLiteral("omarchy-dictation-use"), {previous}, &code);
        if (code != 0) {
            fprintf(stderr, "Could not hand dictation back to %s: %s\n", qPrintable(previous), qPrintable(output));
            return 1;
        }
    }
    run(QStringLiteral("systemctl"), {QStringLiteral("--user"), QStringLiteral("stop"),
                                      QStringLiteral("--no-block"), QStringLiteral("soz.service")});
    setEnabledFile(false);
    QFile::remove(Paths::phaseFile());
    if (!quiet)
        printf("off\n");
    return 0;
}

int toggle() { return enabled() ? off(false) : on(); }

int printState() {
    const bool isOn = enabled();
    const QString phase = readFile(Paths::phaseFile());
    printf("%s %s\n", isOn ? "on" : "off", qPrintable(phase.isEmpty() ? QStringLiteral("stopped") : phase));
    return 0;
}

}
