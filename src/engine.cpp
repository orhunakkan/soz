#include "engine.h"

#include "text.h"

#include <QElapsedTimer>
#include <QFileInfo>
#include <QByteArray>

#include <whisper.h>

namespace {

// whisper.cpp and ggml log every layer they load; keep the journal readable.
void quietLog(ggml_log_level level, const char *text, void *) {
    static const bool verbose = qEnvironmentVariableIsSet("SOZ_VERBOSE");
    if (verbose || level >= GGML_LOG_LEVEL_WARN)
        fputs(text, stderr);
}

}

Engine::Engine(Options options, QObject *parent) : QObject(parent), m_options(std::move(options)) {
    whisper_log_set(quietLog, nullptr);
    // By default ggml puts its working buffers in the host-visible window of
    // video memory (ReBAR). On RADV that makes every encoder op several times
    // slower: the 30 s encoder pass takes 560 ms there and 115 ms in plain
    // device memory on a Radeon RX 7900 XTX. Must be set before the first
    // Vulkan call; an explicit setting from the environment still wins.
    qputenv("GGML_VK_DISABLE_HOST_VISIBLE_VIDMEM", qgetenv("GGML_VK_DISABLE_HOST_VISIBLE_VIDMEM").isEmpty()
                                                     ? QByteArray("1")
                                                     : qgetenv("GGML_VK_DISABLE_HOST_VISIBLE_VIDMEM"));
}

Engine::~Engine() {
    if (m_ctx)
        whisper_free(m_ctx);
}

bool Engine::loadNow(QString *error, double *ms) {
    QElapsedTimer timer;
    timer.start();
    if (!QFileInfo::exists(m_options.model)) {
        *error = QStringLiteral("The speech model is missing: %1. Run `soz download`.").arg(m_options.model);
        return false;
    }

    whisper_context_params params = whisper_context_default_params();
    params.use_gpu = m_options.gpu;
    // Flash attention more than halves the encoder pass on Vulkan.
    params.flash_attn = true;
    m_ctx = whisper_init_from_file_with_params(m_options.model.toLocal8Bit().constData(), params);
    if (!m_ctx) {
        *error = QStringLiteral("Could not load the speech model %1.").arg(m_options.model);
        return false;
    }

    // The first inference builds the GPU pipelines, which takes far longer
    // than any later one. Pay for it now rather than on the first dictation.
    // Low noise rather than silence, which would be trimmed away unread.
    QVector<float> noise(16000);
    for (qsizetype i = 0; i < noise.size(); ++i)
        noise[i] = 0.01f * float((i * 7919) % 200 - 100) / 100.0f;
    decode(noise);

    *ms = timer.nsecsElapsed() / 1e6;
    return true;
}

QString Engine::transcribeNow(const QVector<float> &audio, double *ms) {
    QElapsedTimer timer;
    timer.start();
    // Silence before and after the speech costs decoding time and is where
    // Whisper invents text; a recording with no speech at all is dropped.
    const QVector<float> speech = Text::trimSilence(audio);
    const QString text = decode(speech);
    *ms = timer.nsecsElapsed() / 1e6;
    return text;
}

QString Engine::decode(const QVector<float> &audio) {
    if (!m_ctx || audio.isEmpty())
        return QString();

    const QByteArray language = m_options.language.toLatin1();

    whisper_full_params p = whisper_full_default_params(WHISPER_SAMPLING_GREEDY);
    p.language = language.constData();
    p.translate = false;
    p.detect_language = false;
    p.n_threads = m_options.threads;
    // Each dictation stands alone; carrying text over invites repeats.
    p.no_context = true;
    p.no_timestamps = true;
    p.suppress_nst = true;
    // A low-confidence pass is retried at a higher temperature. One retry
    // candidate instead of five keeps a hard sentence from costing a second.
    p.greedy.best_of = 1;
    p.print_progress = false;
    p.print_realtime = false;
    p.print_special = false;
    p.print_timestamps = false;

    static const bool verbose = qEnvironmentVariableIsSet("SOZ_VERBOSE");
    if (verbose)
        whisper_reset_timings(m_ctx);

    QString text;
    if (whisper_full(m_ctx, p, audio.constData(), int(audio.size())) == 0) {
        const int n = whisper_full_n_segments(m_ctx);
        for (int i = 0; i < n; ++i)
            text += QString::fromUtf8(whisper_full_get_segment_text(m_ctx, i));
    }
    if (verbose)
        whisper_print_timings(m_ctx);
    return Text::clean(text);
}

void Engine::load() {
    QString error;
    double ms = 0;
    const bool ok = loadNow(&error, &ms);
    emit loaded(ok, error, ms);
}

void Engine::transcribe(int session, const QVector<float> &audio) {
    double ms = 0;
    const QString text = transcribeNow(audio, &ms);
    emit transcribed(session, text, ms, audio.size() / 16000.0);
}
