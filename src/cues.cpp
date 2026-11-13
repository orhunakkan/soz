#include "cues.h"

#include "paths.h"

#include <QByteArray>
#include <QFile>
#include <QProcess>
#include <QStandardPaths>
#include <QtEndian>

#include <algorithm>
#include <cmath>

namespace {

QString file(const char *name) { return Paths::runtimeDir() + "/" + name + ".wav"; }

// Two soft sine notes with a short fade, as a 16-bit mono WAV.
QByteArray tone(double first, double second, double seconds) {
    const int rate = 48000;
    const int frames = int(rate * seconds);
    QByteArray pcm(frames * 2, 0);
    for (int i = 0; i < frames; ++i) {
        const double t = double(i) / rate;
        const double freq = i < frames / 2 ? first : second;
        const double fade = std::min({1.0, i / (rate * 0.005), (frames - i) / (rate * 0.02)});
        const double value = std::sin(2 * M_PI * freq * t) * 0.18 * fade;
        qToLittleEndian<qint16>(qint16(value * 32767), pcm.data() + i * 2);
    }

    QByteArray wav;
    auto u32 = [&](quint32 v) { char b[4]; qToLittleEndian(v, b); wav.append(b, 4); };
    auto u16 = [&](quint16 v) { char b[2]; qToLittleEndian(v, b); wav.append(b, 2); };
    wav.append("RIFF");
    u32(36 + pcm.size());
    wav.append("WAVEfmt ");
    u32(16); u16(1); u16(1); u32(rate); u32(rate * 2); u16(2); u16(16);
    wav.append("data");
    u32(pcm.size());
    wav.append(pcm);
    return wav;
}

void play(const char *name) {
    static const QString player = QStandardPaths::findExecutable(QStringLiteral("pw-play"));
    if (!player.isEmpty())
        QProcess::startDetached(player, {file(name)});
}

}

namespace Cues {

void prepare() {
    Paths::writeAtomically(file("start"), tone(660, 880, 0.09));
    Paths::writeAtomically(file("stop"), tone(880, 660, 0.09));
    Paths::writeAtomically(file("error"), tone(330, 247, 0.18));
}

void start() { play("start"); }
void stop() { play("stop"); }
void error() { play("error"); }

}
