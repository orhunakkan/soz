#include "inserter.h"

#include <QProcess>
#include <QStandardPaths>

namespace Inserter {

QString type(const QString &text) {
    if (text.isEmpty())
        return QString();
    const QString wtype = QStandardPaths::findExecutable(QStringLiteral("wtype"));
    if (wtype.isEmpty())
        return QStringLiteral("wtype is not installed, so soz cannot type the text.");

    // The text is fed on standard input ("-"), never as an argument, so
    // nothing in it can be read as an option.
    QProcess process;
    process.start(wtype, {QStringLiteral("-")});
    if (!process.waitForStarted(2000))
        return QStringLiteral("Could not start wtype.");
    process.write(text.toUtf8());
    process.closeWriteChannel();
    if (!process.waitForFinished(10000) || process.exitCode() != 0)
        return QStringLiteral("wtype failed: %1").arg(QString::fromUtf8(process.readAllStandardError()).trimmed());
    return QString();
}

}
