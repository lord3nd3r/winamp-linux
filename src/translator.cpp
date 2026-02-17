#include "translator.h"

void Translator::loadLanguage(const QString &langCode) {
    strings.clear();
    currentLang = langCode;
    
    // Try ~/.winamp/lang/ first, then fallback to bundled location
    QStringList langPaths = {
        QDir::homePath() + "/.winamp/lang",
        "lang"
    };
    
    for (const QString &basePath : langPaths) {
        QString langFile = basePath + "/" + langCode + ".lang";
        if (QFile::exists(langFile)) {
            loadFromFile(langFile);
            return;
        }
    }
    
    // No language file found, use built-in English
    loadEnglishDefaults();
}

QStringList Translator::getAvailableLanguages() {
    QStringList langs;
    langs << "en" << "de" << "fr" << "es" << "pt" << "ru" << "ja" << "zh";
    
    // Scan for installed language files
    QStringList langPaths = {
        QDir::homePath() + "/.winamp/lang",
        "lang"
    };
    
    for (const QString &basePath : langPaths) {
        QDir dir(basePath);
        if (dir.exists()) {
            QStringList files = dir.entryList(QStringList() << "*.lang", QDir::Files);
            for (const QString &file : files) {
                QString code = QFileInfo(file).baseName();
                if (!langs.contains(code)) {
                    langs << code;
                }
            }
        }
    }
    
    return langs;
}

void Translator::loadFromFile(const QString &filePath) {
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return;
    }
    
    QTextStream in(&file);
    in.setEncoding(QStringConverter::Utf8);
    
    while (!in.atEnd()) {
        QString line = in.readLine().trimmed();
        
        // Skip comments and empty lines
        if (line.isEmpty() || line.startsWith('#') || line.startsWith(';')) {
            continue;
        }
        
        // Parse KEY=Value format
        int eqPos = line.indexOf('=');
        if (eqPos > 0) {
            QString key = line.left(eqPos).trimmed();
            QString value = line.mid(eqPos + 1).trimmed();
            
            // Unescape basic sequences
            value.replace("\\n", "\n");
            value.replace("\\t", "\t");
            
            strings[key] = value;
        }
    }
    
    file.close();
}

void Translator::loadEnglishDefaults() {
    // Window titles
    strings["win.main.title"] = "Winamp 5.666 for Linux";
    strings["win.playlist.title"] = "Winamp Playlist Editor";
    strings["win.equalizer.title"] = "Winamp Equalizer";
    strings["win.video.title"] = "Winamp Video";
    strings["win.library.title"] = "Winamp Library";
    strings["win.milkdrop.title"] = "Milkdrop Visualization";
    strings["win.preferences.title"] = "Winamp Preferences";
    strings["win.about.title"] = "About Winamp";
    strings["win.fileinfo.title"] = "File Info";
    strings["win.jumpto.title"] = "Jump to File";
    strings["win.playlocation.title"] = "Play Location";
    strings["win.plgen.title"] = "Playlist Generator";
    
    // Menu items - File
    strings["menu.file"] = "File";
    strings["menu.file.play"] = "Play";
    strings["menu.file.playfile"] = "Play file...\\tL";
    strings["menu.file.playlocation"] = "Play location...\\tCtrl+L";
    strings["menu.options"] = "Options";
    strings["menu.playback"] = "Playback";
    strings["menu.windows"] = "Windows";
    strings["menu.visualization"] = "Visualization";
    
    // Common buttons
    strings["button.ok"] = "OK";
    strings["button.cancel"] = "Cancel";
    strings["button.apply"] = "Apply";
    strings["button.close"] = "Close";
    strings["button.generate"] = "Generate";
    
    // Playlist generator
    strings["plgen.numtracks"] = "Number of tracks:";
    strings["plgen.replace"] = "Replace current playlist (otherwise add to current)";
    strings["plgen.nofound"] = "No audio files found in";
    
    currentLang = "en";
}
