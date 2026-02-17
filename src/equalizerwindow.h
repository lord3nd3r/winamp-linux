#pragma once
#include <QWidget>
#include <QPainter>
#include <QMouseEvent>
#include <QSettings>
#include <QMenu>
#include <QFileDialog>
#include <QInputDialog>
#include <QMessageBox>
#include <QDir>
#include <QFile>
#include <QTextStream>
#include <QFileInfo>

class WinampWindow;

class EqualizerWindow : public QWidget {
public:
    EqualizerWindow(WinampWindow *parent = nullptr);
    
    void setMainWindow(WinampWindow *main) { mainWindow = main; }
    
    void followMain();
    void checkSnap();
    
    // Get EQ band gain in dB (for audio processing)
    float getBandGainDb(int band) const {
        if (band < 0 || band >= 10) return 0.0f;
        if (!eqEnabled) return 0.0f;
        return (32 - eqValues[band]) * 12.0f / 32.0f;
    }
    float getPreampGainDb() const {
        if (!eqEnabled) return 0.0f;
        return (32 - preampValue) * 12.0f / 32.0f;
    }
    
    // Raw slider accessors for EQ DSP engine
    int getBandValue(int band) const {
        if (band < 0 || band >= 10) return 31;
        return eqValues[band];
    }
    int getPreampValue() const { return preampValue; }
    
    bool isEnabled() const { return eqEnabled; }
    
    // Auto-load EQ preset based on filename
    void autoLoadPreset(const QString &filePath) {
        if (!autoEnabled) return;
        QString baseName = QFileInfo(filePath).completeBaseName();
        QString presetDir = QDir::homePath() + "/.config/winamp/eqpresets";
        QString perFilePreset = presetDir + "/" + baseName + ".eqf";
        if (QFile::exists(perFilePreset)) {
            loadPresetFile(perFilePreset);
            return;
        }
        QString defaultPreset = presetDir + "/Default.eqf";
        if (QFile::exists(defaultPreset)) {
            loadPresetFile(defaultPreset);
        }
    }
    
    bool isAutoEnabled() const { return autoEnabled; }
    
    // Window shade mode
    void toggleShadeMode() {
        shadeMode = !shadeMode;
        if (shadeMode) {
            setFixedSize(275, 14);
        } else {
            setFixedSize(275, 116);
        }
        update();
    }
    bool isShadeMode() const { return shadeMode; }
    
    void showPresetsMenu(QPoint globalPos);
    
    void saveSettings(QSettings &s);
    void loadSettings(QSettings &s);
    
    bool isSnapped() const { return isSnappedToMain; }

protected:
    void paintEvent(QPaintEvent *) override;
    void drawEqFrequencyCurve(QPainter &p);
    void drawEqSlider(QPainter &p, int which, int destX);
    void paintModernEQ(QPainter &p);
    void mousePressEvent(QMouseEvent *event) override;
    void updateSliderFromY(int y);
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void mouseDoubleClickEvent(QMouseEvent *event) override;

private:
    int eqValues[10];
    int preampValue;
    bool eqEnabled = true;
    bool autoEnabled = false;
    bool shadeMode = false;
    int draggingSlider = -1;
    QPoint dragPosition;
    bool isDragging = false;
    WinampWindow *mainWindow = nullptr;
    bool isSnappedToMain = false;
    
    // EQ preset file I/O
    void loadPresetFromFile();
    void loadPresetFile(const QString &path);
    void savePresetToFile();
    void deletePresetFile();
};
