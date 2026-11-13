#include "recorder.h"

#include "text.h"

#include <QAudioDevice>
#include <QAudioSource>
#include <QMediaDevices>

Recorder::Recorder(QObject *parent) : QObject(parent) {}

Recorder::~Recorder() { cancel(); }

bool Recorder::start(QString *error) {
    cancel();
    QElapsedTimer timer;
    timer.start();

    const QAudioDevice device = QMediaDevices::defaultAudioInput();
    if (device.isNull()) {
        *error = QStringLiteral("No microphone is connected.");
        return false;
    }
    m_deviceName = device.description();

    // Ask for Whisper's format directly so the sound server does the
    // conversion; fall back to whatever the microphone offers and convert here.
    QAudioFormat wanted;
    wanted.setSampleRate(16000);
    wanted.setChannelCount(1);
    wanted.setSampleFormat(QAudioFormat::Float);
    m_format = device.isFormatSupported(wanted) ? wanted : device.preferredFormat();

    m_samples.clear();
    m_samples.reserve(16000 * 60);
    m_source = new QAudioSource(device, m_format, this);
    // Small buffers keep the tail of the dictation from waiting in the pipe
    // when the key comes up.
    m_source->setBufferSize(m_format.bytesForDuration(20000));
    m_io = m_source->start();
    if (!m_io) {
        *error = QStringLiteral("Could not open the microphone %1.").arg(m_deviceName);
        cancel();
        return false;
    }
    connect(m_io, &QIODevice::readyRead, this, &Recorder::drain);
    m_openMs = timer.nsecsElapsed() / 1e6;
    return true;
}

void Recorder::drain() {
    if (m_io)
        append(m_io->readAll());
}

void Recorder::append(const QByteArray &bytes) {
    if (bytes.isEmpty())
        return;
    const int bytesPerSample = m_format.bytesPerSample();
    const qsizetype count = bytes.size() / bytesPerSample;
    const char *data = bytes.constData();
    const qsizetype start = m_samples.size();
    m_samples.resize(start + count);
    for (qsizetype i = 0; i < count; ++i)
        m_samples[start + i] = m_format.normalizedSampleValue(data + i * bytesPerSample);
}

QVector<float> Recorder::stop() {
    if (!m_source)
        return {};
    drain();
    m_source->stop();
    drain();
    const QAudioFormat format = m_format;
    QVector<float> raw = std::move(m_samples);
    cancel();

    if (format.sampleRate() == 16000 && format.channelCount() == 1)
        return raw;
    return Text::resample(raw, format.sampleRate(), format.channelCount());
}

void Recorder::cancel() {
    if (m_source) {
        m_source->stop();
        m_source->deleteLater();
    }
    m_source = nullptr;
    m_io = nullptr;
    m_samples.clear();
}
