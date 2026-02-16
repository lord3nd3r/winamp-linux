/**
 * main_linux.cpp - Linux entry point for Winamp
 * 
 * Replaces WinMain with standard main() and Qt application initialization
 */

#include <QApplication>
#include <QCommandLineParser>
#include <QLibraryInfo>
#include <QLocale>
#include <QTranslator>
#include <iostream>

#include "../Main.h"
#include "../application.h"
#include "main_window_qt.h"

// Forward declarations from original Winamp
extern "C" {
    extern int InitInstance(void);
    extern void QuitInstance(void);
}

// Global application instance
QApplication *g_qtApp = nullptr;
MainWindowQt *g_mainWindow = nullptr;

// Application info
static const char *APP_NAME = "Winamp";
static const char *APP_VERSION = "5.9.0";
static const char *APP_ORG = "Winamp";

int main(int argc, char *argv[])
{
    // Enable high DPI support
    QApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
    QApplication::setAttribute(Qt::AA_UseHighDpiPixmaps);
    
    // Create Qt application
    QApplication app(argc, argv);
    g_qtApp = &app;
    
    // Set application metadata
    QCoreApplication::setApplicationName(APP_NAME);
    QCoreApplication::setApplicationVersion(APP_VERSION);
    QCoreApplication::setOrganizationName(APP_ORG);
    QCoreApplication::setOrganizationDomain("winamp.com");
    
    // Load translations
    QTranslator translator;
    const QStringList uiLanguages = QLocale::system().uiLanguages();
    for (const QString &locale : uiLanguages) {
        const QString baseName = "winamp_" + QLocale(locale).name();
        if (translator.load(":/i18n/" + baseName)) {
            app.installTranslator(&translator);
            break;
        }
    }
    
    // Parse command line
    QCommandLineParser parser;
    parser.setApplicationDescription("Winamp - The legendary media player on Linux");
    parser.addHelpOption();
    parser.addVersionOption();
    
    // Winamp-specific options
    parser.addOption({{"p", "play"}, "Start playing immediately"});
    parser.addOption({{"e", "enqueue"}, "Enqueue files instead of playing"});
    parser.addOption({{"b", "bookmark"}, "Play bookmark <name>", "name"});
    parser.addPositionalArgument("files", "Media files to play", "[files...]");
    
    parser.process(app);
    
    // Initialize Winamp subsystems
    std::cout << "Winamp for Linux " << APP_VERSION << std::endl;
    std::cout << "Initializing..." << std::endl;
    
    // Call original Winamp initialization (adapted for Linux)
    // Note: This will need significant porting work
    // if (InitInstance() == 0) {
    //     std::cerr << "Failed to initialize Winamp" << std::endl;
    //     return 1;
    // }
    
    // Create main window
    g_mainWindow = new MainWindowQt();
    g_mainWindow->setWindowTitle(QString("%1 %2").arg(APP_NAME).arg(APP_VERSION));
    g_mainWindow->resize(275, 116); // Classic Winamp size
    g_mainWindow->show();
    
    // Handle command line files
    const QStringList files = parser.positionalArguments();
    if (!files.isEmpty()) {
        if (parser.isSet("enqueue")) {
            // Add to playlist
            for (const QString &file : files) {
                g_mainWindow->enqueueFile(file);
            }
        } else {
            // Play immediately
            g_mainWindow->loadFiles(files);
            if (parser.isSet("play")) {
                g_mainWindow->play();
            }
        }
    }
    
    // Enter Qt event loop
    int result = app.exec();
    
    // Cleanup
    delete g_mainWindow;
    g_mainWindow = nullptr;
    
    // Call original Winamp cleanup
    // QuitInstance();
    
    std::cout << "Winamp shutdown complete" << std::endl;
    
    return result;
}
