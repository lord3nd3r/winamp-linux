// python_plugin.h — Process-isolated Python plugin host.
// The host is a separate python3 process. That is a crash boundary, not an
// OS sandbox: plugins run with the user's normal privileges.
#pragma once

#include <QObject>
#include <QProcess>
#include <QJsonObject>
#include <QJsonValue>
#include <QElapsedTimer>

class WinampWindow;

// How many times a dead host is started again, and how long to wait.
// A plugin that exits immediately cannot spin: after kMaxRestarts failures
// the manager stops. A host that stays up for kStableResetMs resets the count.
struct PluginHostRestart {
    static constexpr int kMaxRestarts = 5;
    static constexpr int kStableResetMs = 30000;

    static bool shouldRestart(int restartsAlready) {
        return restartsAlready < kMaxRestarts;
    }

    // 500ms, 1s, 2s, 4s, 8s for attempts 0..4.
    static int backoffMs(int restartsAlready) {
        const int shift = restartsAlready < 0 ? 0 : (restartsAlready > 4 ? 4 : restartsAlready);
        return 500 * (1 << shift);
    }
};

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
    void startHostProcess();

    WinampWindow* winamp;
    QProcess* process;
    QString hostScriptPath;
    bool shuttingDown = false;
    int hostRestartCount = 0;
    QElapsedTimer hostRunningTimer;
};
