#include <QtTest>
#include <QObject>
#include <cmath>
#include "../src/eq_dsp.h"
#include "../src/pcm_feed.h"
#include "../src/python_plugin.h"

class TestEqDsp : public QObject {
    Q_OBJECT

private slots:
    void initTestCase();
    void cleanupTestCase();
    
    void testValToDb();
    void testDb2Gain();
    void testSetup();
    void testSetGain();
    void testProcessf();
    void testBalanceReachesOutputChain();
    void testPluginHostRestartLimit();
    void testMilkdropChannelCounts();
    void testShortAudioSinkWrite();
};

void TestEqDsp::initTestCase() {
    // Initialization code
}

void TestEqDsp::cleanupTestCase() {
    // Cleanup code
}

void TestEqDsp::testValToDb() {
    // Slider pos: 63 = top = +12dB, 32 = center = 0dB (Flat / default), 0 = -12dB.
    QCOMPARE(eq10_valtodb(32), 0.0);
    QCOMPARE(eq10_valtodb(63), 12.0);
    QCOMPARE(eq10_valtodb(0), -12.0);
    // One notch below center is a cut, not silence-with-a-boost.
    QCOMPARE(eq10_valtodb(31), -12.0 / 32.0);

    // Bounds checks
    QCOMPARE(eq10_valtodb(-5), -12.0);
    QCOMPARE(eq10_valtodb(100), 12.0);
}

void TestEqDsp::testDb2Gain() {
    // 0dB -> gain ratio should be 0.0 (multiplier = 1.0)
    QCOMPARE(eq10_db2gain(0.0), 0.0);
    // +6dB -> gain ratio should be pow(10, 6/20) - 1.0 = ~0.995
    QVERIFY(qAbs(eq10_db2gain(6.0) - 0.9952623) < 0.0001);
}

void TestEqDsp::testSetup() {
    eq10_t eq[2];
    eq10_setup(eq, 2, 44100.0);
    
    QCOMPARE(eq[0].rate, 44100.0);
    QCOMPARE(eq[1].rate, 44100.0);
    
    // Check frequency mappings
    for (int i = 0; i < EQ10_NOFBANDS; i++) {
        // Bands should have wide Q cut and narrow Q boost initialized
        QVERIFY(eq[0].band[i].da0 > 0.0);
        QVERIFY(eq[0].band[i].ua0 > 0.0);
    }
}

void TestEqDsp::testSetGain() {
    eq10_t eq[2];
    eq10_setup(eq, 2, 44100.0);
    
    eq10_setgain(eq, 2, 3, 6.0); // Set band 3 to +6dB
    
    double expectedGain = eq10_db2gain(6.0);
    QCOMPARE(eq[0].band[3].gain, expectedGain);
    QCOMPARE(eq[1].band[3].gain, expectedGain);
}

void TestEqDsp::testProcessf() {
    eq10_t eq[2];
    eq10_setup(eq, 2, 44100.0);
    
    // Initialize input buffer with stereo audio data (flat signal)
    const int frameCount = 100;
    float input[frameCount * 2];
    float output[frameCount * 2];
    for (int i = 0; i < frameCount * 2; i++) {
        input[i] = 0.5f;
        output[i] = 0.0f;
    }
    
    // Try processing with flat EQ (all gains at 0.0)
    for (int i = 0; i < EQ10_NOFBANDS; i++) {
        eq10_setgain(eq, 2, i, 0.0);
    }
    
    eq10_processf(&eq[0], input, output, frameCount, 0, 2); // left channel
    eq10_processf(&eq[1], input, output, frameCount, 1, 2); // right channel
    
    // With flat EQ, output should be identical to input
    for (int i = 0; i < frameCount * 2; i++) {
        QCOMPARE(output[i], input[i]);
    }
    
    // Now boost band 4 (1 kHz) to +12dB and process
    eq10_setgain(eq, 2, 4, 12.0);
    
    // Reset output
    memset(output, 0, sizeof(output));
    
    eq10_processf(&eq[0], input, output, frameCount, 0, 2);
    eq10_processf(&eq[1], input, output, frameCount, 1, 2);
    
    // Verify processing completed without NaNs or infs
    for (int i = 0; i < frameCount * 2; i++) {
        QVERIFY(!std::isnan(output[i]));
        QVERIFY(!std::isinf(output[i]));
    }
}

void TestEqDsp::testPluginHostRestartLimit() {
    QVERIFY(PluginHostRestart::shouldRestart(0));
    QVERIFY(PluginHostRestart::shouldRestart(PluginHostRestart::kMaxRestarts - 1));
    QVERIFY(!PluginHostRestart::shouldRestart(PluginHostRestart::kMaxRestarts));
    QCOMPARE(PluginHostRestart::backoffMs(0), 500);
    QCOMPARE(PluginHostRestart::backoffMs(1), 1000);
    QCOMPARE(PluginHostRestart::backoffMs(2), 2000);
    QCOMPARE(PluginHostRestart::backoffMs(3), 4000);
    QCOMPARE(PluginHostRestart::backoffMs(4), 8000);
    QCOMPARE(PluginHostRestart::backoffMs(20), 8000);
}

void TestEqDsp::testMilkdropChannelCounts() {
    const float monoIn[3] = {1.f, 2.f, 3.f};
    float monoOut[3] = {};
    QCOMPARE(copyMonoFloat(monoIn, 3, 1, monoOut, 3), 3);
    QCOMPARE(monoOut[0], 1.f);
    QCOMPARE(monoOut[1], 2.f);
    QCOMPARE(monoOut[2], 3.f);

    const float stereo[4] = {1.f, 3.f, 2.f, 6.f};
    float down[2] = {};
    QCOMPARE(copyMonoFloat(stereo, 2, 2, down, 2), 2);
    QCOMPARE(down[0], 2.f);
    QCOMPARE(down[1], 4.f);

    const int16_t mono16[3] = {1, 2, 3};
    int16_t stereo16[6] = {};
    QCOMPARE(copyStereoInt16(mono16, 3, 1, stereo16, 3), 3);
    QCOMPARE(stereo16[0], int16_t(1));
    QCOMPARE(stereo16[1], int16_t(1));
    QCOMPARE(stereo16[4], int16_t(3));
    QCOMPARE(stereo16[5], int16_t(3));

    const int16_t multi[6] = {1, 2, 9, 3, 4, 9};
    int16_t pair[4] = {};
    QCOMPARE(copyStereoInt16(multi, 2, 3, pair, 2), 2);
    QCOMPARE(pair[0], int16_t(1));
    QCOMPARE(pair[1], int16_t(2));
    QCOMPARE(pair[2], int16_t(3));
    QCOMPARE(pair[3], int16_t(4));
}

void TestEqDsp::testShortAudioSinkWrite() {
    const char data[] = "abcdefghij";
    QByteArray accepted;
    qint64 budget = 4;
    const qint64 n = writeAcceptedBytes(data, 10, [&](const char *p, qint64 len) -> qint64 {
        if (budget <= 0)
            return 0;
        const qint64 take = qMin(qint64(3), qMin(len, budget));
        accepted.append(p, static_cast<int>(take));
        budget -= take;
        return take;
    });
    QCOMPARE(n, qint64(4));
    QCOMPARE(accepted, QByteArray("abcd"));
}

void TestEqDsp::testBalanceReachesOutputChain() {
    QVERIFY(!playbackDspNeeded(false, 0));
    QVERIFY(playbackDspNeeded(true, 0));
    QVERIFY(playbackDspNeeded(false, -40));
    QVERIFY(playbackDspNeeded(false, 40));

    const PlaybackOutputGain center = playbackOutputGain(255, 0);
    QCOMPARE(center.left, 1.0f);
    QCOMPARE(center.right, 1.0f);

    const PlaybackOutputGain left = playbackOutputGain(255, -127);
    QCOMPARE(left.left, 1.0f);
    QVERIFY(left.right < 0.001f);

    const PlaybackOutputGain right = playbackOutputGain(255, 127);
    QVERIFY(right.left < 0.001f);
    QCOMPARE(right.right, 1.0f);

    const PlaybackOutputGain quiet = playbackOutputGain(0, -127);
    QCOMPARE(quiet.left, 0.0f);
    QCOMPARE(quiet.right, 0.0f);
}

QTEST_MAIN(TestEqDsp)
#include "test_eq_dsp.moc"
