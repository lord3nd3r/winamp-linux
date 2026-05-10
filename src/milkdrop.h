// milkdrop.h — Milkdrop Visualization Window (via projectM)
#pragma once

#include <QWidget>
#include <QPainter>
#include <QMouseEvent>
#include <QMenu>
#include <QTimer>
#include <QOpenGLWidget>
#include <QOpenGLFunctions>
#include <libprojectM/projectM.hpp>
#include "constants.h"

class MilkdropWindow : public QOpenGLWidget, protected QOpenGLFunctions {
    Q_OBJECT
public:
    MilkdropWindow(QWidget *parent = nullptr) : QOpenGLWidget(parent) {
        setWindowTitle("Milkdrop Visualization");
        setWindowFlags(Qt::Window);
        setAttribute(Qt::WA_DeleteOnClose);
        resize(640, 480);
        setMinimumSize(320, 240);
        setFocusPolicy(Qt::StrongFocus);
        
        // Timer driving projectM render at ~33fps
        renderTimer = new QTimer(this);
        connect(renderTimer, &QTimer::timeout, this, QOverload<>::of(&MilkdropWindow::update));
    }
    
    ~MilkdropWindow() override {
        makeCurrent();
        if (pm) { delete pm; pm = nullptr; }
        doneCurrent();
    }
    
    void feedPCMInt16(const qint16 *data, int frames, int channels) {
        if (!pm) return;
        // Convert interleaved int16 stereo to the format projectM expects:
        // addPCM16Data wants interleaved L,R,L,R... shorts
        int n = qMin(frames, 512);
        pm->pcm()->addPCM16Data(data, n);
    }
    
    void feedPCMFloat(const float *data, int frames, int channels) {
        if (!pm) return;
        // Convert interleaved float to mono float for addPCMfloat
        float mono[512];
        int n = qMin(frames, 512);
        for (int i = 0; i < n; i++)
            mono[i] = data[i * channels];
        pm->pcm()->addPCMfloat(mono, n);
    }
    
    void nextPreset() { if (pm) pm->selectNext(true); }
    void prevPreset() { if (pm) pm->selectPrevious(true); }
    void randomPreset() { if (pm) pm->selectRandom(true); }
    void toggleLock() {
        if (!pm) return;
        pm->setPresetLock(!pm->isPresetLocked());
    }
    
protected:
    void initializeGL() override {
        initializeOpenGLFunctions();
        glClearColor(0, 0, 0, 1);
        
        // Create projectM instance from settings struct
        projectM::Settings s;
        s.meshX = 32;
        s.meshY = 24;
        s.fps = 33;
        s.textureSize = 1024;
        s.windowWidth = width();
        s.windowHeight = height();
        s.presetURL = "/usr/share/projectM/presets";
        s.smoothPresetDuration = 5;
        s.presetDuration = 30;
        s.beatSensitivity = 1.0f;
        s.aspectCorrection = true;
        s.easterEgg = 1.0f;
        s.shuffleEnabled = true;
        s.softCutRatingsEnabled = false;
        
        // Find fonts
        std::string fontPath;
        for (auto &candidate : {std::string("/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf"),
                                 std::string("/usr/share/fonts/truetype/ttf-dejavu/DejaVuSans.ttf"),
                                 std::string("/usr/share/fonts/truetype/freefont/FreeSans.ttf")}) {
            if (QFile::exists(QString::fromStdString(candidate))) { fontPath = candidate; break; }
        }
        s.titleFontURL = fontPath;
        s.menuFontURL = fontPath;
        
        try {
            pm = new projectM(s, projectM::FLAG_NONE);
            pm->projectM_resetGL(width(), height());
            pm->projectM_setTitle("");  // Clear the default "projectM" title overlay
            pm->selectRandom(true);
            qDebug() << "Milkdrop (projectM) initialized with" << pm->getPlaylistSize() << "presets";
        } catch (const std::exception &e) {
            qWarning() << "Failed to initialize projectM:" << e.what();
            pm = nullptr;
        } catch (...) {
            qWarning() << "Failed to initialize projectM (unknown error)";
            pm = nullptr;
        }
        
        renderTimer->start(1000 / 33);
    }
    
    void resizeGL(int w, int h) override {
        if (pm) pm->projectM_resetGL(w, h);
    }
    
    void paintGL() override {
        if (pm) {
            pm->renderFrame();
        } else {
            glClear(GL_COLOR_BUFFER_BIT);
        }
    }
    
    void keyPressEvent(QKeyEvent *e) override {
        if (!pm) { QOpenGLWidget::keyPressEvent(e); return; }
        switch (e->key()) {
            case Qt::Key_Space: nextPreset(); break;
            case Qt::Key_Backspace: prevPreset(); break;
            case Qt::Key_L: toggleLock(); break;
            case Qt::Key_R: pm->setShuffleEnabled(!pm->isShuffleEnabled()); break;
            case Qt::Key_F:
            case Qt::Key_F11:
                toggleFullScreen();
                break;
            case Qt::Key_Escape:
                if (isFullScreen()) toggleFullScreen();
                else close();
                break;
            case Qt::Key_Q: close(); break;
            default: QOpenGLWidget::keyPressEvent(e); return;
        }
        e->accept();
    }
    
    void mouseDoubleClickEvent(QMouseEvent *e) override {
        toggleFullScreen();
    }
    
    void showEvent(QShowEvent *e) override {
        QOpenGLWidget::showEvent(e);
        setFocus();
    }
    
    void closeEvent(QCloseEvent *e) override {
        if (isFullScreen()) {
            showNormal();
            emit fullscreenChanged(false);
        }
        renderTimer->stop();
        e->accept();
    }
    
signals:
    void fullscreenChanged(bool fs);
    
private:
    void toggleFullScreen() {
        if (isFullScreen()) {
            showNormal();
            emit fullscreenChanged(false);
        } else {
            showFullScreen();
            emit fullscreenChanged(true);
        }
        setFocus();
    }
    
    projectM *pm = nullptr;
    QTimer *renderTimer;
};

