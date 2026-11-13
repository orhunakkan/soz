#pragma once

#include <QString>
#include <QVector>

// Pure helpers with no device or model behind them, so tests cover them directly.
namespace Text {

// Whisper's raw output joined from its segments: trims the leading space each
// segment carries, collapses runs of whitespace, and drops the captions
// Whisper invents over silence in Turkish ("Altyazı M.K.", "İzlediğiniz için
// teşekkürler"), which come from subtitled videos in its training data.
QString clean(const QString &raw);

// Cuts the silence before the first and after the last speech, keeping a
// short margin so no syllable is clipped. Returns nothing when the whole
// recording is below speech level (a key pressed and released in silence).
QVector<float> trimSilence(const QVector<float> &audio);

// Linear resampling to 16 kHz mono, Whisper's only input format. Used when the
// microphone will not open at 16 kHz itself.
QVector<float> resample(const QVector<float> &input, int fromRate, int channels);

}
