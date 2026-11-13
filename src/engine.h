#pragma once

#include <QObject>
#include <QString>
#include <QVector>

struct whisper_context;

// Whisper large-v3-turbo through whisper.cpp on the GPU (Vulkan). The model is
// loaded once and stays resident, so a dictation pays only for inference.
// Lives on its own thread: loading and transcribing block.
class Engine : public QObject {
    Q_OBJECT

public:
    struct Options {
        QString model;
        QString language = QStringLiteral("tr");
        bool gpu = true;
        int threads = 8;
    };

    explicit Engine(Options options, QObject *parent = nullptr);
    ~Engine() override;

    // Runs on the calling thread; for the CLI's one-shot `soz transcribe`.
    bool loadNow(QString *error, double *ms);
    QString transcribeNow(const QVector<float> &audio, double *ms);

public slots:
    void load();
    void transcribe(int session, const QVector<float> &audio);

signals:
    void loaded(bool ok, const QString &error, double ms);
    void transcribed(int session, const QString &text, double ms, double audioSeconds);

private:
    QString decode(const QVector<float> &audio);

    Options m_options;
    whisper_context *m_ctx = nullptr;
};
