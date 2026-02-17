#ifndef TRANSLATOR_H
#define TRANSLATOR_H

#include <QString>
#include <QStringList>
#include <QMap>
#include <QFile>
#include <QTextStream>
#include <QDir>
#include <QFileInfo>
#include <QStringConverter>

class Translator {
public:
    static Translator& instance() {
        static Translator inst;
        return inst;
    }
    
    QString tr(const QString &key, const QString &defaultValue = QString()) {
        if (strings.contains(key)) {
            return strings[key];
        }
        return defaultValue.isEmpty() ? key : defaultValue;
    }
    
    void loadLanguage(const QString &langCode);
    QString getCurrentLanguage() const { return currentLang; }
    QStringList getAvailableLanguages();
    
private:
    Translator() { loadEnglishDefaults(); }
    
    void loadFromFile(const QString &filePath);
    void loadEnglishDefaults();
    
    QMap<QString, QString> strings;
    QString currentLang;
};

// Convenience macro
#define TR(key, def) Translator::instance().tr(key, def)

#endif // TRANSLATOR_H
