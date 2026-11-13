#include "text.h"

#include <QRegularExpression>

#include <algorithm>
#include <cmath>
#include <QStringList>

namespace Text {

namespace {

// Compared without case, spaces or punctuation.
QString key(const QString &text) {
    QString out;
    for (const QChar c : text.toLower())
        if (c.isLetterOrNumber())
            out += c;
    return out;
}

bool isHallucination(const QString &text) {
    static const QStringList known = {
        QStringLiteral("altyazımk"),
        QStringLiteral("altyazı"),
        QStringLiteral("izlediğiniziçinteşekkürler"),
        QStringLiteral("izlediğiniziçinteşekkürederim"),
        QStringLiteral("aboneolmayıunutmayın"),
        QStringLiteral("altyazımkizlediğiniziçinteşekkürler"),
    };
    const QString k = key(text);
    return k.isEmpty() || known.contains(k);
}

}

QString clean(const QString &raw) {
    QString text = raw;
    text.replace(QRegularExpression(QStringLiteral("\\s+")), QStringLiteral(" "));
    text = text.trimmed();
    if (isHallucination(text))
        return QString();
    return text;
}

QVector<float> trimSilence(const QVector<float> &audio) {
    constexpr qsizetype frame = 320;          // 20 ms at 16 kHz
    constexpr qsizetype leadMargin = 16000 / 4;   // 250 ms
    constexpr qsizetype tailMargin = 16000 * 3 / 10; // 300 ms
    constexpr float speechFloor = 0.004f;     // RMS; a quiet room sits well below

    const qsizetype frames = audio.size() / frame;
    if (frames == 0)
        return {};
    QVector<float> rms(frames);
    float peak = 0;
    for (qsizetype f = 0; f < frames; ++f) {
        double sum = 0;
        for (qsizetype i = 0; i < frame; ++i) {
            const float s = audio[f * frame + i];
            sum += double(s) * s;
        }
        rms[f] = float(std::sqrt(sum / frame));
        peak = std::max(peak, rms[f]);
    }
    if (peak < speechFloor)
        return {};

    // Relative to the loudest moment, so a loud microphone's hiss and a quiet
    // speaker both land on the right side of the line.
    const float threshold = std::max(speechFloor, peak * 0.05f);
    qsizetype first = 0;
    while (first < frames && rms[first] < threshold)
        ++first;
    qsizetype last = frames - 1;
    while (last > first && rms[last] < threshold)
        --last;

    const qsizetype from = std::max<qsizetype>(0, first * frame - leadMargin);
    const qsizetype to = std::min<qsizetype>(audio.size(), (last + 1) * frame + tailMargin);
    return audio.mid(from, to - from);
}

QVector<float> resample(const QVector<float> &input, int fromRate, int channels) {
    if (channels < 1 || fromRate < 1)
        return {};

    // Mix down to mono first.
    const qsizetype frames = input.size() / channels;
    QVector<float> mono(frames);
    for (qsizetype i = 0; i < frames; ++i) {
        float sum = 0;
        for (int c = 0; c < channels; ++c)
            sum += input[i * channels + c];
        mono[i] = sum / channels;
    }
    if (fromRate == 16000)
        return mono;

    const double step = double(fromRate) / 16000.0;
    const qsizetype outFrames = qsizetype(frames / step);
    QVector<float> out(outFrames);
    if (step > 1.0) {
        // Downsampling: average every input sample that falls in the output
        // sample's window. The box filter keeps the band above 8 kHz from
        // folding back into speech as noise.
        for (qsizetype i = 0; i < outFrames; ++i) {
            const qsizetype a = qsizetype(i * step);
            const qsizetype b = qMin(qsizetype((i + 1) * step), frames);
            float sum = 0;
            for (qsizetype j = a; j < b; ++j)
                sum += mono[j];
            out[i] = b > a ? sum / float(b - a) : mono[qMin(a, frames - 1)];
        }
        return out;
    }
    for (qsizetype i = 0; i < outFrames; ++i) {
        const double pos = i * step;
        const qsizetype a = qsizetype(pos);
        const qsizetype b = qMin(a + 1, frames - 1);
        const float t = float(pos - a);
        out[i] = mono[a] * (1 - t) + mono[b] * t;
    }
    return out;
}

}
