#pragma once

#include <QAudioFormat>
#include <QElapsedTimer>
#include <QObject>
#include <QString>
#include <QVector>

class QAudioSource;
class QIODevice;

// Captures the default microphone while a dictation runs and hands back
// 16 kHz mono samples, Whisper's input format.
class Recorder : public QObject {
    Q_OBJECT

public:
    explicit Recorder(QObject *parent = nullptr);
    ~Recorder() override;

    bool start(QString *error);
    QVector<float> stop();
    void cancel();
    bool recording() const { return m_source != nullptr; }
    QString deviceName() const { return m_deviceName; }
    double openMs() const { return m_openMs; }

private:
    void drain();
    void append(const QByteArray &bytes);

    QAudioSource *m_source = nullptr;
    QIODevice *m_io = nullptr;
    QAudioFormat m_format;
    QVector<float> m_samples;
    QString m_deviceName;
    double m_openMs = 0;
};
