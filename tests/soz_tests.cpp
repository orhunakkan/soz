#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTemporaryDir>
#include <QtTest>

#include "bar.h"
#include "paths.h"
#include "text.h"

class SozTests : public QObject {
    Q_OBJECT

    static QJsonObject readShell() {
        QFile file(Paths::shellJson());
        if (!file.open(QIODevice::ReadOnly))
            return {};
        return QJsonDocument::fromJson(file.readAll()).object();
    }

    static QJsonArray right() {
        return readShell().value(QStringLiteral("bar")).toObject().value(QStringLiteral("layout")).toObject()
            .value(QStringLiteral("right")).toArray();
    }

private slots:
    void cleanTrimsAndCollapses() {
        QCOMPARE(Text::clean(QStringLiteral("  Merhaba,   nasılsın?\n")), QStringLiteral("Merhaba, nasılsın?"));
    }

    void cleanKeepsTurkishLetters() {
        const QString text = QStringLiteral("Çağrı ılık şöyle güzel İstanbul");
        QCOMPARE(Text::clean(text), text);
    }

    void cleanDropsSilenceCaptions() {
        QCOMPARE(Text::clean(QStringLiteral(" Altyazı M.K.")), QString());
        QCOMPARE(Text::clean(QStringLiteral("İzlediğiniz için teşekkürler.")), QString());
        QCOMPARE(Text::clean(QStringLiteral(" ... ")), QString());
    }

    void cleanKeepsRealSentencesMentioningCaptions() {
        const QString text = QStringLiteral("Altyazı dosyasını yarın göndereceğim.");
        QCOMPARE(Text::clean(text), text);
    }

    void resamplePassesMono16k() {
        const QVector<float> in{0.1f, 0.2f, 0.3f};
        QCOMPARE(Text::resample(in, 16000, 1), in);
    }

    void resampleMixesStereo() {
        const QVector<float> in{1.0f, 0.0f, 0.5f, 0.5f};
        QCOMPARE(Text::resample(in, 16000, 2), (QVector<float>{0.5f, 0.5f}));
    }

    void resampleAveragesDown() {
        // 48 kHz to 16 kHz: every output sample is the mean of three inputs.
        const QVector<float> in{0.0f, 0.3f, 0.6f, 1.0f, 1.0f, 1.0f};
        const QVector<float> out = Text::resample(in, 48000, 1);
        QCOMPARE(out.size(), 2);
        QVERIFY(qAbs(out[0] - 0.3f) < 1e-6f);
        QVERIFY(qAbs(out[1] - 1.0f) < 1e-6f);
    }

    void resampleRejectsNonsense() {
        QVERIFY(Text::resample({1.0f}, 0, 1).isEmpty());
        QVERIFY(Text::resample({1.0f}, 16000, 0).isEmpty());
    }

    void trimDropsSilence() {
        QVERIFY(Text::trimSilence(QVector<float>(16000, 0.0f)).isEmpty());
        QVector<float> hiss(16000);
        for (int i = 0; i < hiss.size(); ++i)
            hiss[i] = (i % 2 ? 0.001f : -0.001f);
        QVERIFY(Text::trimSilence(hiss).isEmpty());
    }

    void trimKeepsSpeechWithMargins() {
        // 1 s silence, 0.5 s "speech", 1 s silence.
        QVector<float> audio(16000 * 5 / 2, 0.0f);
        for (int i = 16000; i < 16000 + 8000; ++i)
            audio[i] = (i % 40 < 20) ? 0.3f : -0.3f;
        const QVector<float> out = Text::trimSilence(audio);
        // 250 ms before + 500 ms speech + 300 ms after.
        QCOMPARE(out.size(), qsizetype(4000 + 8000 + 4800));
        QCOMPARE(out[4000], 0.3f);
    }

    void trimKeepsQuietSpeech() {
        QVector<float> audio(16000, 0.0f);
        for (int i = 4000; i < 12000; ++i)
            audio[i] = (i % 40 < 20) ? 0.02f : -0.02f;
        QVERIFY(!Text::trimSilence(audio).isEmpty());
    }

    void pathsFollowXdg() {
        QTemporaryDir dir;
        qputenv("XDG_RUNTIME_DIR", dir.path().toUtf8());
        qputenv("XDG_CONFIG_HOME", (dir.path() + "/config").toUtf8());
        QCOMPARE(Paths::socket(), dir.path() + "/soz/soz.sock");
        QCOMPARE(Paths::shortcutsLua(), dir.path() + "/config/soz/shortcuts.lua");
        QCOMPARE(Paths::backendShortcutsLua(QStringLiteral("superwhisper")),
                 dir.path() + "/config/superwhisper/shortcuts.lua");
        QCOMPARE(Paths::shellJson(), dir.path() + "/config/omarchy/shell.json");
    }

    void barAddsAfterSuperwhisperOnce() {
        QTemporaryDir dir;
        qputenv("XDG_CONFIG_HOME", dir.path().toUtf8());
        qputenv("OMARCHY_PATH", (dir.path() + "/none").toUtf8());
        qputenv("SOZ_BAR_QML", "/tmp/Soz.qml");
        QVERIFY(Paths::writeAtomically(Paths::shellJson(),
            R"({"bar":{"layout":{"left":[],"right":[{"id":"omarchy.tray"},{"id":"superwhisper.panel"},)"
            R"({"id":"omarchy.bluetooth"}]}},"idle":{"lock":600}})"));
        QCOMPARE(Bar::on(true), 0);
        QCOMPARE(Bar::on(true), 0);
        const QJsonArray items = right();
        QCOMPARE(items.size(), 4);
        QCOMPARE(items[2].toObject().value(QStringLiteral("id")).toString(), QStringLiteral("soz"));
        QCOMPARE(items[2].toObject().value(QStringLiteral("source")).toString(), QStringLiteral("/tmp/Soz.qml"));
        QCOMPARE(readShell().value(QStringLiteral("idle")).toObject().value(QStringLiteral("lock")).toInt(), 600);

        QCOMPARE(Bar::off(true), 0);
        QCOMPARE(right().size(), 3);
    }

    void barFallsBackToBluetooth() {
        QTemporaryDir dir;
        qputenv("XDG_CONFIG_HOME", dir.path().toUtf8());
        qputenv("OMARCHY_PATH", (dir.path() + "/none").toUtf8());
        QVERIFY(Paths::writeAtomically(Paths::shellJson(),
            R"({"bar":{"layout":{"right":[{"id":"omarchy.tray"},{"id":"omarchy.bluetooth"},{"id":"omarchy.power"}]}}})"));
        QCOMPARE(Bar::on(true), 0);
        QCOMPARE(right()[1].toObject().value(QStringLiteral("id")).toString(), QStringLiteral("soz"));
    }

    void writeAtomicallyReplaces() {
        QTemporaryDir dir;
        const QString path = dir.path() + "/a/b/phase";
        QVERIFY(Paths::writeAtomically(path, "idle"));
        QVERIFY(Paths::writeAtomically(path, "recording"));
        QFile file(path);
        QVERIFY(file.open(QIODevice::ReadOnly));
        QCOMPARE(file.readAll(), QByteArray("recording"));
    }
};

QTEST_GUILESS_MAIN(SozTests)
#include "soz_tests.moc"
