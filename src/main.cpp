// soz — offline Turkish dictation for Omarchy, Whisper on the GPU.

#include <cstdio>
#include <cstring>
#include <string>

#include <QCoreApplication>
#include <QFile>
#include <QFileInfo>
#include <QProcess>
#include <QSocketNotifier>
#include <QStringList>

#include <csignal>
#include <sys/socket.h>
#include <unistd.h>

#include "bar.h"
#include "client.h"
#include "daemon.h"
#include "engine.h"
#include "paths.h"
#include "switcher.h"

namespace {

constexpr const char *kVersion = "0.1.0";

const char *kUsage =
    "soz — offline Turkish dictation\n"
    "\n"
    "Usage:\n"
    "  soz on                 Take over Alt + Space and Right Alt for Turkish\n"
    "  soz off                Hand the dictation keys back to the previous backend\n"
    "  soz switch             Turn Turkish dictation on or off\n"
    "  soz state              Print on|off and what the daemon is doing\n"
    "  soz bar on|off         Add the TR switch to the Omarchy bar, or remove it\n"
    "  soz start|stop|toggle  Record (what the dictation keys run)\n"
    "  soz cancel             Drop the recording in progress\n"
    "  soz status             The daemon's status as JSON\n"
    "  soz transcribe FILE    Transcribe an audio file and print timings\n"
    "  soz download           Download the speech model (about 875 MB)\n"
    "  soz daemon             Run the service (started by systemd)\n";

int sigtermFd[2];

void onSignal(int) {
    char c = 1;
    [[maybe_unused]] auto n = ::write(sigtermFd[0], &c, 1);
}

int runDaemon(int argc, char *argv[]) {
    QCoreApplication app(argc, argv);
    app.setApplicationName(QStringLiteral("soz"));

    // systemd stops the service with SIGTERM; leave through the event loop so
    // the engine thread and the phase file are cleaned up.
    ::socketpair(AF_UNIX, SOCK_STREAM, 0, sigtermFd);
    QSocketNotifier notifier(sigtermFd[1], QSocketNotifier::Read);
    QObject::connect(&notifier, &QSocketNotifier::activated, &app, &QCoreApplication::quit);
    std::signal(SIGTERM, onSignal);
    std::signal(SIGINT, onSignal);

    Daemon daemon;
    QString error;
    if (!daemon.listen(&error)) {
        fprintf(stderr, "soz: %s\n", qPrintable(error));
        return 1;
    }
    return app.exec();
}

int transcribeFile(int argc, char *argv[]) {
    QCoreApplication app(argc, argv);
    QStringList args = app.arguments().mid(2);
    int repeat = 1;
    const int at = args.indexOf(QStringLiteral("--repeat"));
    if (at >= 0 && at + 1 < args.size()) {
        repeat = qMax(1, args.at(at + 1).toInt());
        args.remove(at, 2);
    }
    if (args.size() != 1) {
        fprintf(stderr, "Usage: soz transcribe FILE [--repeat N]\n");
        return 1;
    }

    // ffmpeg turns any audio file into Whisper's 16 kHz mono floats.
    QProcess ffmpeg;
    ffmpeg.start(QStringLiteral("ffmpeg"), {"-nostdin", "-loglevel", "error", "-i", args.first(), "-ar", "16000",
                                            "-ac", "1", "-f", "f32le", "-"});
    if (!ffmpeg.waitForFinished(60000) || ffmpeg.exitCode() != 0) {
        fprintf(stderr, "Could not read %s: %s\n", qPrintable(args.first()), ffmpeg.readAllStandardError().constData());
        return 1;
    }
    const QByteArray bytes = ffmpeg.readAllStandardOutput();
    QVector<float> audio(bytes.size() / 4);
    std::memcpy(audio.data(), bytes.constData(), audio.size() * 4);

    Engine::Options options;
    options.model = Paths::modelFile();
    options.threads = qBound(1, QThread::idealThreadCount() / 2, 8);
    if (qEnvironmentVariableIsSet("SOZ_CPU"))
        options.gpu = false;
    if (qEnvironmentVariableIsSet("SOZ_LANGUAGE"))
        options.language = qEnvironmentVariable("SOZ_LANGUAGE");
    Engine engine(options);
    QString error;
    double loadMs = 0;
    if (!engine.loadNow(&error, &loadMs)) {
        fprintf(stderr, "%s\n", qPrintable(error));
        return 1;
    }
    printf("model loaded and warmed in %.0f ms\n", loadMs);
    for (int i = 0; i < repeat; ++i) {
        double ms = 0;
        const QString text = engine.transcribeNow(audio, &ms);
        printf("%.1f s of audio -> %.0f ms: %s\n", audio.size() / 16000.0, ms, qPrintable(text));
    }
    return 0;
}

int download() {
    struct Model { const char *file; const char *url; };
    const Model models[] = {
        {"ggml-large-v3-turbo-q8_0.bin",
         "https://huggingface.co/ggerganov/whisper.cpp/resolve/main/ggml-large-v3-turbo-q8_0.bin"},
    };
    const QString dir = Paths::modelDir();
    for (const Model &model : models) {
        const QString target = dir + "/" + model.file;
        if (QFileInfo::exists(target)) {
            printf("%s is already downloaded.\n", model.file);
            continue;
        }
        printf("Downloading %s ...\n", model.file);
        fflush(stdout);
        const int code = QProcess::execute(QStringLiteral("curl"), {"-fL", "--retry", "3", "--create-dirs", "-#",
                                                                   "-o", target + ".part", model.url});
        if (code != 0 || !QFile::rename(target + ".part", target)) {
            fprintf(stderr, "Could not download %s.\n", model.file);
            return 1;
        }
    }
    return 0;
}

}

int main(int argc, char *argv[]) {
    const std::string command = argc > 1 ? argv[1] : "help";

    // The dictation keys come through here; keep them free of Qt start-up.
    if (command == "start" || command == "toggle")
        return Client::send(command, true);
    if (command == "stop" || command == "cancel")
        return Client::reachable() ? Client::send(command, false) : 0;
    if (command == "status")
        return Client::send(command, false);

    if (command == "daemon")
        return runDaemon(argc, argv);
    if (command == "transcribe")
        return transcribeFile(argc, argv);

    QCoreApplication app(argc, argv);
    if (command == "on")
        return Switcher::on();
    if (command == "off")
        return Switcher::off(argc > 2 && std::strcmp(argv[2], "--quiet") == 0);
    if (command == "switch")
        return Switcher::toggle();
    if (command == "state")
        return Switcher::printState();
    if (command == "bar") {
        const std::string action = argc > 2 ? argv[2] : "";
        if (action == "on")
            return Bar::on(false);
        if (action == "off")
            return Bar::off(false);
        fprintf(stderr, "Usage: soz bar on|off\n");
        return 1;
    }
    if (command == "download")
        return download();
    if (command == "version" || command == "--version") {
        printf("soz %s\n", kVersion);
        return 0;
    }
    if (command == "help" || command == "--help" || command == "-h") {
        fputs(kUsage, stdout);
        return 0;
    }
    fprintf(stderr, "Unknown command: %s\n\n%s", command.c_str(), kUsage);
    return 1;
}
