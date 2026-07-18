#include <QtTest>
#include <QObject>
#include <cmath>
#include "../src/eq_dsp.h"

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
};

void TestEqDsp::initTestCase() {
    // Initialization code
}

void TestEqDsp::cleanupTestCase() {
    // Cleanup code
}

void TestEqDsp::testValToDb() {
    // Slider pos: 63 = top = +12dB boost, 31 = center = 0dB, 0 = bottom = -11.625dB
    QCOMPARE(eq10_valtodb(31), 0.0);
    QCOMPARE(eq10_valtodb(63), 12.0);
    QCOMPARE(eq10_valtodb(0), -11.625);
    
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

QTEST_MAIN(TestEqDsp)
#include "test_eq_dsp.moc"
