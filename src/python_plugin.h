// python_plugin.h — Sandbox-Isolated Python Plugin Host Manager
#pragma once

#include <QObject>
#include <QProcess>
#include <QJsonObject>
#include <QJsonValue>

class WinampWindow;

class PythonPluginManager : public QObject {
    Q_OBJECT
public:
    PythonPluginManager(WinampWindow* w);
    ~PythonPluginManager();

private slots:
    void handleReadyRead();
    void handleProcessError(QProcess::ProcessError error);
    void handleProcessFinished(int exitCode, QProcess::ExitStatus exitStatus);

private:
    void handleRequest(const QJsonObject &req);
    void sendResponse(int id, const QJsonValue &result);
    void sendEvent(const QString &eventName);
    void writeHostScript();

    WinampWindow* winamp;
    QProcess* process;
    QString hostScriptPath;
};
