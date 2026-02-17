#ifndef MILKDROPWINDOW_H
#define MILKDROPWINDOW_H

#include <QOpenGLWidget>
#include <QOpenGLFunctions>
#include <QTimer>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QCloseEvent>
#include <QShowEvent>
#include <QFile>
#include <QDebug>
#include <libprojectM/projectM.hpp>

class MilkdropWindow : public QOpenGLWidget, protected QOpenGLFunctions {
    Q_OBJECT
public:
    MilkdropWindow(QWidget *parent = nullptr);
    ~MilkdropWindow() override;
    
    void feedPCMInt16(const qint16 *data, int frames, int channels);
    void feedPCMFloat(const float *data, int frames, int channels);
    void nextPreset();
    void prevPreset();
    void randomPreset();
    void toggleLock();
    
protected:
    void initializeGL() override;
    void resizeGL(int w, int h) override;
    void paintGL() override;
    void keyPressEvent(QKeyEvent *e) override;
    void mouseDoubleClickEvent(QMouseEvent *e) override;
    void showEvent(QShowEvent *e) override;
    void closeEvent(QCloseEvent *e) override;
    
signals:
    void fullscreenChanged(bool fs);
    
private:
    void toggleFullScreen();
    
    projectM *pm = nullptr;
    QTimer *renderTimer;
};

#endif // MILKDROPWINDOW_H
