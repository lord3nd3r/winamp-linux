// equalizer.h — Winamp Equalizer
#pragma once

#include <QWidget>
#include <QPainter>
#include <QMouseEvent>
#include <QMenu>
#include <QSettings>
#include "constants.h"

class WinampWindow;

class EqualizerWindow : public QWidget {
    Q_OBJECT
public:
    EqualizerWindow(WinampWindow *parent = nullptr);
    
    void setMainWindow(WinampWindow *main) { mainWindow = main; }
    
    void followMain();
    void checkSnap();
    bool isSnapped() const { return isSnappedToMain && snapEdge != 0; }
    void setSnapped(bool snapped) { isSnappedToMain = snapped; snapEdge = snapped ? 1 : 0; }
    // Place flush under the main window and mark docked (Winamp 2.x default).
    void dockBelowMain();
    
    float getBandGainDb(int band) const;
    float getPreampGainDb() const;
    int getBandValue(int band) const;
    int getPreampValue() const;
    bool isEnabled() const;
    void autoLoadPreset(const QString &filePath);
    bool isAutoEnabled() const;
    void toggleShadeMode();
    bool isShadeMode() const;
    
    void showPresetsMenu(QPoint globalPos);
    void saveSettings(QSettings &s);
    void loadSettings(QSettings &s);
    
protected:
    void paintEvent(QPaintEvent *) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void mouseDoubleClickEvent(QMouseEvent *event) override;
    
private:
    void drawEqFrequencyCurve(QPainter &p);
    void drawEqSlider(QPainter &p, int which, int destX);
    void paintModernEQ(QPainter &p);
    void updateSliderFromY(int y);
    void loadPresetFromFile();
    void loadPresetFile(const QString &path);
    void savePresetToFile();
    void deletePresetFile();

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
    int snapEdge = 0;  // 0=none, 1=below main, 2=right of main, 3=above main
};
