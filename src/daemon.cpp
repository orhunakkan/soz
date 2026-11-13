#include "daemon.h"

#include "cues.h"
#include "engine.h"
#include "inserter.h"
#include "paths.h"

#include <QDateTime>
#include <QDir>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLocalServer>
#include <QLocalSocket>

namespace {

// Shorter than this is a tap on the key, not speech.
constexpr int kMinimumSamples = 16000 * 3 / 10;

QByteArray reply(bool ok, const QString &message, const QJsonObject &extra = {}) {
    QJsonObject object = extra;
    object.insert(QStringLiteral("ok"), ok);
    if (!message.isEmpty())
        object.insert(QStringLiteral("message"), message);
    return QJsonDocument(object).toJson(QJsonDocument::Compact) + '\n';
}

}

Daemon::Daemon(QObject *parent) : QObject(parent) {
    m_clock.start();
    qRegisterMetaType<QVector<float>>();

    Engine::Options options;
    options.model = Paths::modelFile();
    options.threads = qBound(1, QThread::idealThreadCount() / 2, 8);
    m_engine = new Engine(options);
    m_engine->moveToThread(&m_thread);
    connect(&m_thread, &QThread::finished, m_engine, &QObject::deleteLater);
    connect(this, &Daemon::transcribeRequested, m_engine, &Engine::transcribe);
    connect(m_engine, &Engine::loaded, this, &Daemon::onLoaded);
    connect(m_engine, &Engine::transcribed, this, &Daemon::onTranscribed);
    m_thread.start();
}

Daemon::~Daemon() {
    m_thread.quit();
    m_thread.wait();
    QFile::remove(Paths::phaseFile());
}

bool Daemon::listen(QString *error) {
    QDir().mkpath(Paths::runtimeDir());
    Cues::prepare();

    // The socket opens before the model loads, so a shortcut pressed while
    // it loads still starts recording; the text follows once it is ready.
    QLocalServer::removeServer(Paths::socket());
    m_server = new QLocalServer(this);
    m_server->setSocketOptions(QLocalServer::UserAccessOption);
    if (!m_server->listen(Paths::socket())) {
        *error = QStringLiteral("Could not listen on %1: %2").arg(Paths::socket(), m_server->errorString());
        return false;
    }
    connect(m_server, &QLocalServer::newConnection, this, [this] {
        while (QLocalSocket *socket = m_server->nextPendingConnection()) {
            connect(socket, &QLocalSocket::disconnected, socket, &QObject::deleteLater);
            connect(socket, &QLocalSocket::readyRead, socket, [this, socket] {
                if (!socket->canReadLine())
                    return;
                socket->write(handle(socket->readLine().trimmed()));
                socket->flush();
                socket->disconnectFromServer();
            });
        }
    });

    setPhase(QStringLiteral("loading"));
    QMetaObject::invokeMethod(m_engine, &Engine::load, Qt::QueuedConnection);
    return true;
}

QByteArray Daemon::handle(const QByteArray &command) {
    if (command == "start")
        return start();
    if (command == "stop")
        return stop();
    if (command == "toggle")
        return m_recorder.recording() ? stop() : start();
    if (command == "cancel")
        return cancel();
    if (command == "status")
        return status();
    return reply(false, QStringLiteral("Unknown command: %1").arg(QString::fromUtf8(command)));
}

QByteArray Daemon::start() {
    if (m_recorder.recording())
        return reply(true, QStringLiteral("Already recording."));
    if (!m_loadError.isEmpty()) {
        Cues::error();
        return reply(false, m_loadError);
    }
    QString error;
    if (!m_recorder.start(&error)) {
        Cues::error();
        setPhase(QStringLiteral("error"));
        return reply(false, error);
    }
    Cues::start();
    setPhase(QStringLiteral("recording"));
    return reply(true, QString(), {{QStringLiteral("device"), m_recorder.deviceName()},
                                   {QStringLiteral("open_ms"), m_recorder.openMs()}});
}

QByteArray Daemon::stop() {
    if (!m_recorder.recording())
        return reply(true, QStringLiteral("Not recording."));
    const qint64 released = m_clock.nsecsElapsed();
    QVector<float> audio = m_recorder.stop();
    Cues::stop();
    if (audio.size() < kMinimumSamples) {
        setPhase(currentPhase());
        return reply(true, QStringLiteral("Too short to transcribe."));
    }
    const int session = ++m_session;
    m_releasedAt.insert(session, released);
    ++m_pending;
    setPhase(currentPhase());
    // Queued to the engine's thread; if the model is still loading, it runs
    // as soon as loading finishes, in order.
    emit transcribeRequested(session, audio);
    return reply(true, QString(), {{QStringLiteral("session"), session},
                                   {QStringLiteral("audio_seconds"), audio.size() / 16000.0}});
}

QByteArray Daemon::cancel() {
    const bool was = m_recorder.recording();
    m_recorder.cancel();
    setPhase(currentPhase());
    return reply(true, was ? QStringLiteral("Cancelled.") : QStringLiteral("Not recording."));
}

QByteArray Daemon::status() const {
    QJsonObject object{
        {QStringLiteral("phase"), m_phase},
        {QStringLiteral("ready"), m_ready},
        {QStringLiteral("model"), Paths::modelFile()},
        {QStringLiteral("load_ms"), m_loadMs},
        {QStringLiteral("pending"), m_pending},
    };
    if (!m_loadError.isEmpty())
        object.insert(QStringLiteral("error"), m_loadError);
    return reply(true, QString(), object);
}

void Daemon::onLoaded(bool ok, const QString &error, double ms) {
    m_ready = ok;
    m_loadError = error;
    m_loadMs = ms;
    if (!ok) {
        fprintf(stderr, "soz: %s\n", qPrintable(error));
        setPhase(QStringLiteral("error"));
        return;
    }
    fprintf(stderr, "soz: model ready in %.0f ms\n", ms);
    setPhase(currentPhase());
}

void Daemon::onTranscribed(int session, const QString &text, double ms, double audioSeconds) {
    --m_pending;
    const qint64 released = m_releasedAt.take(session);
    const double releaseToText = (m_clock.nsecsElapsed() - released) / 1e6;

    // SOZ_DRY_RUN keeps test runs from typing into whatever window has focus.
    static const bool dryRun = qEnvironmentVariableIsSet("SOZ_DRY_RUN");
    const QString insertError = dryRun ? QString() : Inserter::type(text);
    const double releaseToDelivery = (m_clock.nsecsElapsed() - released) / 1e6;
    if (!insertError.isEmpty()) {
        fprintf(stderr, "soz: %s\n", qPrintable(insertError));
        Cues::error();
    }

    const QJsonObject last{
        {QStringLiteral("session"), session},
        {QStringLiteral("text"), text},
        {QStringLiteral("audio_seconds"), audioSeconds},
        {QStringLiteral("asr_ms"), ms},
        {QStringLiteral("release_to_text_ms"), releaseToText},
        {QStringLiteral("release_to_delivery_ms"), releaseToDelivery},
        {QStringLiteral("delivered"), insertError.isEmpty()},
        {QStringLiteral("created"), QDateTime::currentSecsSinceEpoch()},
    };
    Paths::writeAtomically(Paths::lastFile(), QJsonDocument(last).toJson(QJsonDocument::Compact));
    fprintf(stderr, "soz: %.1fs of audio -> text in %.0f ms (model %.0f ms), typed in %.0f ms\n",
            audioSeconds, releaseToText, ms, releaseToDelivery);
    setPhase(currentPhase());
}

QString Daemon::currentPhase() const {
    if (!m_loadError.isEmpty())
        return QStringLiteral("error");
    if (m_recorder.recording())
        return QStringLiteral("recording");
    if (m_pending > 0)
        return QStringLiteral("transcribing");
    if (!m_ready)
        return QStringLiteral("loading");
    return QStringLiteral("idle");
}

void Daemon::setPhase(const QString &phase) {
    if (phase == m_phase)
        return;
    m_phase = phase;
    Paths::writeAtomically(Paths::phaseFile(), phase.toUtf8());
}
