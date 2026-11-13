#pragma once

#include <QElapsedTimer>
#include <QHash>
#include <QObject>
#include <QString>
#include <QThread>
#include <QVector>

#include "recorder.h"

class Engine;
class QLocalServer;
class QLocalSocket;

// The long-running service behind the shortcuts: keeps the model loaded,
// records on start, transcribes on stop and types the result. Commands arrive
// as one line on a local socket from `soz start|stop|toggle|cancel|status`.
class Daemon : public QObject {
    Q_OBJECT

public:
    explicit Daemon(QObject *parent = nullptr);
    ~Daemon() override;

    bool listen(QString *error);

signals:
    void transcribeRequested(int session, const QVector<float> &audio);

private:
    QByteArray handle(const QByteArray &command);
    QByteArray start();
    QByteArray stop();
    QByteArray cancel();
    QByteArray status() const;

    void onLoaded(bool ok, const QString &error, double ms);
    void onTranscribed(int session, const QString &text, double ms, double audioSeconds);
    void setPhase(const QString &phase);
    QString currentPhase() const;

    QLocalServer *m_server = nullptr;
    QThread m_thread;
    Engine *m_engine = nullptr;
    Recorder m_recorder;

    bool m_ready = false;
    QString m_loadError;
    double m_loadMs = 0;
    int m_session = 0;
    int m_pending = 0;            // transcriptions on their way
    QString m_phase;
    QElapsedTimer m_clock;
    QHash<int, qint64> m_releasedAt;   // session -> when its stop arrived (ns on m_clock)
};
