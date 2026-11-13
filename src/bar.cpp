#include "bar.h"

#include "paths.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QProcess>
#include <QStandardPaths>

#include <cstdio>

namespace {

// Placed right after Superwhisper's widget when there is one, otherwise just
// before the Bluetooth icon, otherwise at the end of the right section.
const char *kAddFilter = R"jq(
  def ids: map(.id? // "");
  if (.bar.layout == null) then .bar = ((.bar // {}) + {layout: ($defaults[0].bar.layout // {})}) else . end
  | .bar.layout.right = (.bar.layout.right // [])
  | if ([.bar.layout[] | arrays | .[] | .id? // ""] | index("soz")) != null then .
    else .bar.layout.right |= (
      (ids | if index("superwhisper.panel") != null then index("superwhisper.panel") + 1
             elif index("omarchy.bluetooth") != null then index("omarchy.bluetooth")
             else length end) as $i
      | .[:$i] + [{id: "soz", type: "qml", source: $source}] + .[$i:])
    end
)jq";

const char *kRemoveFilter = R"jq(
  if .bar.layout == null then .
  else .bar.layout |= with_entries(.value |= (if type == "array" then map(select((.id? // "") != "soz")) else . end))
  end
)jq";

QString defaultShellJson() {
    const QString root = qEnvironmentVariable("OMARCHY_PATH", QStringLiteral("/usr/share/omarchy"));
    return root + "/config/omarchy/shell.json";
}

// Runs a jq filter over shell.json and replaces the file only when jq
// succeeds, through a temporary file beside it.
bool edit(const char *filter, QString *error) {
    const QString jq = QStandardPaths::findExecutable(QStringLiteral("jq"));
    if (jq.isEmpty()) {
        *error = QStringLiteral("jq is not installed.");
        return false;
    }
    const QString path = Paths::shellJson();
    QString input = path;
    if (!QFileInfo::exists(path)) {
        // No user file yet: start from Omarchy's defaults, as the shell does.
        if (!QFileInfo::exists(defaultShellJson())) {
            *error = QStringLiteral("No Omarchy shell settings were found at %1.").arg(path);
            return false;
        }
        input = defaultShellJson();
    }
    const QString defaults = QFileInfo::exists(defaultShellJson()) ? defaultShellJson() : input;

    QProcess process;
    process.start(jq, {QStringLiteral("--arg"), QStringLiteral("source"), Bar::widgetSource(),
                       QStringLiteral("--slurpfile"), QStringLiteral("defaults"), defaults,
                       QString::fromUtf8(filter), input});
    if (!process.waitForFinished(10000) || process.exitCode() != 0) {
        *error = QStringLiteral("Could not edit %1: %2").arg(path, QString::fromUtf8(process.readAllStandardError()).trimmed());
        return false;
    }
    const QByteArray output = process.readAllStandardOutput();
    if (output.trimmed().isEmpty()) {
        *error = QStringLiteral("jq produced no output for %1.").arg(path);
        return false;
    }
    QFile current(path);
    if (current.open(QIODevice::ReadOnly) && current.readAll() == output)
        return true;
    if (!Paths::writeAtomically(path, output)) {
        *error = QStringLiteral("Could not write %1.").arg(path);
        return false;
    }
    return true;
}

}

namespace Bar {

QString widgetSource() {
    const QString override = qEnvironmentVariable("SOZ_BAR_QML");
    return override.isEmpty() ? QStringLiteral("/usr/share/soz/bar/Soz.qml") : override;
}

int on(bool quiet) {
    QString error;
    if (!edit(kAddFilter, &error)) {
        fprintf(stderr, "%s\n", qPrintable(error));
        return 1;
    }
    if (!quiet)
        printf("The TR switch is in the bar.\n");
    return 0;
}

int off(bool quiet) {
    if (!QFileInfo::exists(Paths::shellJson()))
        return 0;
    QString error;
    if (!edit(kRemoveFilter, &error)) {
        fprintf(stderr, "%s\n", qPrintable(error));
        return 1;
    }
    if (!quiet)
        printf("The TR switch is off the bar.\n");
    return 0;
}

}
