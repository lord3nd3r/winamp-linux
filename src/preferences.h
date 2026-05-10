// preferences.h — Winamp Preferences Dialog
#pragma once

#include <QDialog>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QPushButton>
#include <QTabWidget>
#include <QFormLayout>
#include <QGroupBox>
#include <QCheckBox>
#include <QComboBox>
#include <QSlider>
#include <QDir>
#include <QSettings>
#include <QColorDialog>
#include <QFileDialog>
#include "constants.h"
#include "translator.h"
#include <QTreeWidget>
#include <QStackedWidget>
#include <QSpinBox>
#include <QDesktopServices>
#include <QMessageBox>
#include <QTextStream>
#include <QStandardPaths>
#include <QUrl>

class PreferencesDialog : public QDialog {
    Q_OBJECT
public:
    PreferencesDialog(QWidget *parent = nullptr) : QDialog(parent) {
        setWindowTitle("Winamp Preferences");
        setMinimumSize(600, 450);
        setStyleSheet(
            "QDialog { background-color: #2b2b3d; color: #00ff00; }"
            "QTreeWidget { background-color: #1a1a2e; color: #00ff00; border: 1px solid #555; font-size: 9pt; }"
            "QTreeWidget::item:selected { background-color: #0000c6; }"
            "QStackedWidget { background-color: #2b2b3d; }"
            "QLabel { color: #00ff00; }"
            "QCheckBox { color: #00ff00; }"
            "QCheckBox::indicator { border: 1px solid #555; background: #000; width: 12px; height: 12px; }"
            "QCheckBox::indicator:checked { background: #00ff00; }"
            "QGroupBox { border: 1px solid #555; color: #00ff00; margin-top: 8px; padding-top: 10px; }"
            "QGroupBox::title { subcontrol-origin: margin; left: 10px; }"
            "QSpinBox, QComboBox, QLineEdit { background-color: #000; color: #00ff00; border: 1px solid #555; padding: 2px; }"
            "QPushButton { background-color: #3a3a4d; color: #00ff00; border: 1px solid #555; padding: 4px 12px; }"
            "QPushButton:hover { background-color: #4a4a5d; }"
            "QSlider::groove:horizontal { border: 1px solid #555; height: 4px; background: #000; }"
            "QSlider::handle:horizontal { background: #00ff00; border: 1px solid #555; width: 12px; margin: -4px 0; }"
            "QListWidget { background-color: #000; color: #00FF00; border: 1px solid #555; }"
            "QListWidget::item:selected { background-color: #0000C6; }"
        );

        QHBoxLayout *mainLayout = new QHBoxLayout(this);

        // Left: tree navigation (like Windows Winamp Options.cpp tree)
        treeWidget = new QTreeWidget(this);
        treeWidget->setFixedWidth(180);
        treeWidget->setHeaderHidden(true);
        treeWidget->setRootIsDecorated(true);
        treeWidget->setIndentation(16);

        // Right: stacked pages
        stackedWidget = new QStackedWidget(this);

        // -- Build preference tree items (matches Windows Winamp) --
        auto addPage = [&](QTreeWidgetItem *parent, const QString &label, QWidget *page) -> QTreeWidgetItem* {
            QTreeWidgetItem *item = parent ? new QTreeWidgetItem(parent) : new QTreeWidgetItem(treeWidget);
            item->setText(0, label);
            int idx = stackedWidget->addWidget(page);
            item->setData(0, Qt::UserRole, idx);
            return item;
        };

        // Setup category
        QTreeWidgetItem *setupItem = addPage(nullptr, "Setup", createGeneralPage());
        addPage(setupItem, "File Types", createFileTypesPage());
        addPage(setupItem, "Language", createLanguagePage());

        // Skins category
        QTreeWidgetItem *skinsItem = addPage(nullptr, "Skins", createSkinsPage());
        addPage(skinsItem, "Classic Skins", createClassicSkinsPage());

        // Playback category
        QTreeWidgetItem *playbackItem = addPage(nullptr, "Playback", createPlaybackPage());

        // Playlist category
        QTreeWidgetItem *playlistItem = addPage(nullptr, "Playlist", createPlaylistPrefsPage());

        // Bookmarks
        addPage(nullptr, "Bookmarks", createBookmarksPage());

        // Visualization category
        QTreeWidgetItem *visItem = addPage(nullptr, "Visualization", createVisualizationPage());

        // Plug-ins category
        QTreeWidgetItem *pluginsItem = addPage(nullptr, "Plug-ins", createPluginsPage());

        treeWidget->expandAll();
        treeWidget->setCurrentItem(setupItem);

        connect(treeWidget, &QTreeWidget::currentItemChanged, this, [this](QTreeWidgetItem *current) {
            if (current) {
                int idx = current->data(0, Qt::UserRole).toInt();
                stackedWidget->setCurrentIndex(idx);
            }
        });

        mainLayout->addWidget(treeWidget);
        mainLayout->addWidget(stackedWidget, 1);

        // Close button at bottom
        QVBoxLayout *rightLayout = new QVBoxLayout();
        rightLayout->addWidget(stackedWidget, 1);
        QHBoxLayout *btnLayout = new QHBoxLayout();
        QPushButton *closeBtn = new QPushButton("Close", this);
        connect(closeBtn, &QPushButton::clicked, this, &QDialog::accept);
        btnLayout->addStretch();
        btnLayout->addWidget(closeBtn);
        rightLayout->addLayout(btnLayout);

        // Redo layout
        delete mainLayout;
        QHBoxLayout *newMain = new QHBoxLayout(this);
        newMain->addWidget(treeWidget);
        QWidget *rightPanel = new QWidget(this);
        rightPanel->setLayout(rightLayout);
        newMain->addWidget(rightPanel, 1);
    }

signals:
    void skinChanged(const QString &skinPath);
    void settingChanged(const QString &key, const QVariant &value);

private:
    QTreeWidget *treeWidget;
    QStackedWidget *stackedWidget;
    QString defaultSkinPath;

    // ---- General/Setup page ----
    QWidget *createGeneralPage() {
        QWidget *page = new QWidget();
        QVBoxLayout *layout = new QVBoxLayout(page);
        layout->addWidget(new QLabel("<b>General Preferences</b>"));
        layout->addSpacing(10);

        QCheckBox *aotCheck = new QCheckBox("Always on top", page);
        QCheckBox *trayCheck = new QCheckBox("Show in system tray", page);
        QCheckBox *minToTrayCheck = new QCheckBox("Minimize to system tray", page);
        QCheckBox *notifyCheck = new QCheckBox("Show song change notifications", page);
        notifyCheck->setChecked(true); // Default enabled
        QCheckBox *tooltipCheck = new QCheckBox("Show tooltips", page);
        QCheckBox *snapCheck = new QCheckBox("Snap windows together", page);
        snapCheck->setChecked(true);

        QHBoxLayout *snapDistLayout = new QHBoxLayout();
        snapDistLayout->addWidget(new QLabel("Snap distance:"));
        QSpinBox *snapDistSpin = new QSpinBox(page);
        snapDistSpin->setRange(1, 50);
        snapDistSpin->setValue(15);
        snapDistSpin->setSuffix(" px");
        snapDistLayout->addWidget(snapDistSpin);
        snapDistLayout->addStretch();

        QCheckBox *dsizeCheck = new QCheckBox("Double size mode", page);
        QCheckBox *splashCheck = new QCheckBox("Show splash screen on startup", page);
        splashCheck->setChecked(true);

        layout->addWidget(aotCheck);
        layout->addWidget(trayCheck);
        layout->addWidget(minToTrayCheck);
        layout->addWidget(notifyCheck);
        layout->addWidget(tooltipCheck);
        layout->addWidget(snapCheck);
        layout->addLayout(snapDistLayout);
        layout->addWidget(dsizeCheck);
        layout->addWidget(splashCheck);
        layout->addStretch();

        connect(aotCheck, &QCheckBox::toggled, this, [this](bool v) { emit settingChanged("aot", v); });
        connect(trayCheck, &QCheckBox::toggled, this, [this](bool v) { emit settingChanged("showTray", v); });
        connect(minToTrayCheck, &QCheckBox::toggled, this, [this](bool v) { emit settingChanged("minToTray", v); });
        connect(notifyCheck, &QCheckBox::toggled, this, [this](bool v) { emit settingChanged("showNotifications", v); });
        connect(dsizeCheck, &QCheckBox::toggled, this, [this](bool v) { emit settingChanged("doubleSize", v); });

        return page;
    }

    // ---- File Types page ----
    QWidget *createFileTypesPage() {
        QWidget *page = new QWidget();
        QVBoxLayout *layout = new QVBoxLayout(page);
        layout->addWidget(new QLabel("<b>File Type Associations</b>"));
        layout->addSpacing(10);

        QLabel *desc = new QLabel("Configure which file types Winamp handles.\n"
                                  "On Linux, .desktop file registration is used.", page);
        desc->setWordWrap(true);
        layout->addWidget(desc);

        QPushButton *registerBtn = new QPushButton("Register File Types", page);
        connect(registerBtn, &QPushButton::clicked, this, [this]() {
            // Create .desktop file for Winamp
            QString desktopPath = QStandardPaths::writableLocation(QStandardPaths::ApplicationsLocation) + "/winamp.desktop";
            QFile file(desktopPath);
            if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
                QTextStream out(&file);
                out << "[Desktop Entry]\n"
                    << "Type=Application\n"
                    << "Name=Winamp\n"
                    << "Comment=Winamp Media Player for Linux\n"
                    << "Exec=winamp %F\n"
                    << "MimeType=audio/mpeg;audio/x-wav;audio/flac;audio/ogg;audio/aac;audio/mp4;\n"
                    << "Categories=AudioVideo;Audio;Player;\n"
                    << "Terminal=false\n";
                file.close();
                QMessageBox::information(this, "File Types", "Desktop file created at:\n" + desktopPath);
            }
        });
        layout->addWidget(registerBtn);
        layout->addStretch();
        return page;
    }

    // ---- Titles page ----
    QWidget *createTitlesPage() {
        QWidget *page = new QWidget();
        QVBoxLayout *layout = new QVBoxLayout(page);
        layout->addWidget(new QLabel("<b>Title Formatting</b>"));
        layout->addSpacing(10);

        QLabel *desc = new QLabel("Advanced title formatting string.\nUse metadata fields to customize how track titles appear.", page);
        desc->setWordWrap(true);
        layout->addWidget(desc);

        QLineEdit *fmtEdit = new QLineEdit(page);
        fmtEdit->setText("%artist% - %title%");
        fmtEdit->setPlaceholderText("[%artist% - ]$if2(%title%,$filepart(%filename%))");
        layout->addWidget(new QLabel("Title format:"));
        layout->addWidget(fmtEdit);

        QCheckBox *showNums = new QCheckBox("Show track numbers in playlist", page);
        showNums->setChecked(true);
        QCheckBox *zeroPad = new QCheckBox("Zero-pad track numbers", page);
        layout->addWidget(showNums);
        layout->addWidget(zeroPad);
        layout->addStretch();
        return page;
    }

    // ---- Language page ----
    QWidget *createLanguagePage() {
        QWidget *page = new QWidget();
        QVBoxLayout *layout = new QVBoxLayout(page);
        layout->addWidget(new QLabel("<b>Language</b>"));
        layout->addSpacing(10);
        
        QLabel *infoLabel = new QLabel(
            "Select your preferred language for the Winamp interface.\n"
            "Language packs are loaded from ~/.winamp/lang/ directory."
        );
        infoLabel->setWordWrap(true);
        layout->addWidget(infoLabel);
        
        layout->addSpacing(10);
        
        QHBoxLayout *langLayout = new QHBoxLayout();
        langLayout->addWidget(new QLabel("Language:"));
        
        QComboBox *langCombo = new QComboBox(page);
        
        // Language map
        QMap<QString, QString> langNames;
        langNames["en"] = "English";
        langNames["de"] = "Deutsch (German)";
        langNames["es"] = "Español (Spanish)";
        langNames["fr"] = "Français (French)";
        langNames["pt"] = "Português (Portuguese)";
        langNames["ru"] = "Русский (Russian)";
        langNames["ja"] = "日本語 (Japanese)";
        langNames["zh"] = "中文 (Chinese)";
        
        // Add available languages
        QStringList availableLangs = Translator::instance().getAvailableLanguages();
        for (const QString &code : availableLangs) {
            QString name = langNames.contains(code) ? langNames[code] : code.toUpper();
            langCombo->addItem(name, code);
        }
        
        // Select current language
        QString currentLang = Translator::instance().getCurrentLanguage();
        int currentIdx = langCombo->findData(currentLang);
        if (currentIdx >= 0) {
            langCombo->setCurrentIndex(currentIdx);
        }
        
        langLayout->addWidget(langCombo);
        langLayout->addStretch();
        layout->addLayout(langLayout);
        
        layout->addSpacing(10);
        
        QLabel *noteLabel = new QLabel(
            "<i>Note: Winamp must be restarted for language changes to take effect.</i>"
        );
        noteLabel->setWordWrap(true);
        layout->addWidget(noteLabel);
        
        // Save language preference on change
        connect(langCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [langCombo](int index) {
            QString langCode = langCombo->itemData(index).toString();
            QSettings settings(QDir::homePath() + "/.config/winamp/winamp.conf", QSettings::IniFormat);
            settings.setValue("language", langCode);
        });
        
        layout->addStretch();
        return page;
    }

    // ---- Skins overview page ----
    QWidget *createSkinsPage() {
        QWidget *page = new QWidget();
        QVBoxLayout *layout = new QVBoxLayout(page);
        layout->addWidget(new QLabel("<b>Skins</b>"));
        layout->addSpacing(10);
        layout->addWidget(new QLabel("Select a skin category on the left.\n\n"
                                     "Classic skins (.wsz) use bitmap sprite sheets.\n"
                                     "Modern skins are currently disabled because they can break the UI."));
        layout->addStretch();
        return page;
    }

    // ---- Classic Skins page (moved from old Skins tab) ----
    QWidget *createClassicSkinsPage() {
        QWidget *page = new QWidget();
        QVBoxLayout *layout = new QVBoxLayout(page);
        layout->addWidget(new QLabel("<b>Classic Skins</b>"));

        skinListWidget = new QListWidget(page);
        populateSkins();
        connect(skinListWidget, &QListWidget::itemDoubleClicked, this, &PreferencesDialog::onSkinSelected);
        layout->addWidget(skinListWidget);

        QHBoxLayout *btnRow = new QHBoxLayout();
        QPushButton *openDirBtn = new QPushButton("Open Skins Folder", page);
        connect(openDirBtn, &QPushButton::clicked, this, []() {
            QString skinsDir = QDir::homePath() + "/.winamp/skins";
            QDir().mkpath(skinsDir);
            QDesktopServices::openUrl(QUrl::fromLocalFile(skinsDir));
        });
        btnRow->addWidget(openDirBtn);
        btnRow->addStretch();
        layout->addLayout(btnRow);
        return page;
    }

    // ---- Playback page ----
    QWidget *createPlaybackPage() {
        QWidget *page = new QWidget();
        QVBoxLayout *layout = new QVBoxLayout(page);
        layout->addWidget(new QLabel("<b>Playback Settings</b>"));
        layout->addSpacing(10);

        QGroupBox *priorityGroup = new QGroupBox("Priority", page);
        QVBoxLayout *priLayout = new QVBoxLayout(priorityGroup);
        QComboBox *priorityCombo = new QComboBox(page);
        priorityCombo->addItems({"Idle", "Lowest", "Below Normal", "Normal", "Above Normal", "Highest"});
        priorityCombo->setCurrentIndex(3);
        priLayout->addWidget(new QLabel("Playback thread priority:"));
        priLayout->addWidget(priorityCombo);
        layout->addWidget(priorityGroup);

        QGroupBox *advGroup = new QGroupBox("Advanced", page);
        QVBoxLayout *advLayout = new QVBoxLayout(advGroup);
        QCheckBox *stopAfterCheck = new QCheckBox("Stop after current track", page);
        QCheckBox *alwaysContinue = new QCheckBox("Continue playback on startup", page);
        QCheckBox *fadeOnStop = new QCheckBox("Fade on stop/pause", page);
        advLayout->addWidget(stopAfterCheck);
        advLayout->addWidget(alwaysContinue);
        advLayout->addWidget(fadeOnStop);
        layout->addWidget(advGroup);

        connect(stopAfterCheck, &QCheckBox::toggled, this, [this](bool v) { emit settingChanged("stopAfterCurrent", v); });

        layout->addStretch();
        return page;
    }

    // ---- Playlist preferences page ----
    QWidget *createPlaylistPrefsPage() {
        QWidget *page = new QWidget();
        QVBoxLayout *layout = new QVBoxLayout(page);
        layout->addWidget(new QLabel("<b>Playlist Settings</b>"));
        layout->addSpacing(10);

        QGroupBox *fontGroup = new QGroupBox("Font", page);
        QVBoxLayout *fontLayout = new QVBoxLayout(fontGroup);
        QCheckBox *customFont = new QCheckBox("Use custom playlist font", page);
        QComboBox *fontCombo = new QComboBox(page);
        fontCombo->addItems({"Courier New", "Tahoma", "Arial", "Verdana", "Segoe UI", "DejaVu Sans Mono"});
        QSpinBox *fontSizeSpin = new QSpinBox(page);
        fontSizeSpin->setRange(6, 24);
        fontSizeSpin->setValue(8);
        fontSizeSpin->setSuffix(" pt");
        fontLayout->addWidget(customFont);
        QHBoxLayout *fontRow = new QHBoxLayout();
        fontRow->addWidget(fontCombo);
        fontRow->addWidget(fontSizeSpin);
        fontLayout->addLayout(fontRow);
        layout->addWidget(fontGroup);

        QCheckBox *recycleBin = new QCheckBox("Send removed files to playlist recycle bin", page);
        QCheckBox *showNumbers = new QCheckBox("Show track numbers in playlist", page);
        showNumbers->setChecked(true);
        layout->addWidget(recycleBin);
        layout->addWidget(showNumbers);
        layout->addStretch();
        return page;
    }

    // ---- Bookmarks page ----
    QWidget *createBookmarksPage() {
        QWidget *page = new QWidget();
        QVBoxLayout *layout = new QVBoxLayout(page);
        layout->addWidget(new QLabel("<b>Bookmarks</b>"));

        QListWidget *bmList = new QListWidget(page);
        auto &mgr = BookmarkManager::instance();
        for (const auto &bm : mgr.bookmarks) {
            bmList->addItem(bm.title + " — " + bm.path);
        }

        QHBoxLayout *btnRow = new QHBoxLayout();
        QPushButton *addBtn = new QPushButton("Add...", page);
        QPushButton *removeBtn = new QPushButton("Remove", page);
        connect(addBtn, &QPushButton::clicked, this, [this, bmList]() {
            QString path = QFileDialog::getOpenFileName(this, "Add Bookmark", "",
                "Audio Files (*.mp3 *.wav *.flac *.ogg *.m4a);;All Files (*)");
            if (!path.isEmpty()) {
                QString title = QFileInfo(path).baseName();
                BookmarkManager::instance().addBookmark(title, path);
                bmList->addItem(title + " — " + path);
            }
        });
        connect(removeBtn, &QPushButton::clicked, this, [bmList]() {
            int row = bmList->currentRow();
            if (row >= 0) {
                BookmarkManager::instance().removeBookmark(row);
                delete bmList->takeItem(row);
            }
        });
        btnRow->addWidget(addBtn);
        btnRow->addWidget(removeBtn);
        btnRow->addStretch();

        layout->addWidget(bmList);
        layout->addLayout(btnRow);
        return page;
    }

    // ---- Visualization page ----
    QWidget *createVisualizationPage() {
        QWidget *page = new QWidget();
        QVBoxLayout *layout = new QVBoxLayout(page);
        layout->addWidget(new QLabel("<b>Visualization Settings</b>"));
        layout->addSpacing(10);

        QGroupBox *saGroup = new QGroupBox("Spectrum Analyzer", page);
        QVBoxLayout *saLayout = new QVBoxLayout(saGroup);

        QHBoxLayout *falloffRow = new QHBoxLayout();
        falloffRow->addWidget(new QLabel("Analyzer falloff speed:"));
        QComboBox *falloffCombo = new QComboBox(page);
        falloffCombo->addItems({"Slow", "Medium", "Fast", "Fastest"});
        falloffCombo->setCurrentIndex(1);
        falloffRow->addWidget(falloffCombo);
        saLayout->addLayout(falloffRow);

        QHBoxLayout *peakRow = new QHBoxLayout();
        peakRow->addWidget(new QLabel("Peak falloff speed:"));
        QComboBox *peakCombo = new QComboBox(page);
        peakCombo->addItems({"Slow", "Medium", "Fast", "Fastest"});
        peakCombo->setCurrentIndex(1);
        peakRow->addWidget(peakCombo);
        saLayout->addLayout(peakRow);

        QCheckBox *peaksCheck = new QCheckBox("Show peak dots", page);
        peaksCheck->setChecked(true);
        saLayout->addWidget(peaksCheck);

        layout->addWidget(saGroup);

        connect(falloffCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
            [this](int idx) { emit settingChanged("saFalloff", idx); });
        connect(peakCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
            [this](int idx) { emit settingChanged("saPeakFalloff", idx); });
        connect(peaksCheck, &QCheckBox::toggled, this,
            [this](bool v) { emit settingChanged("saPeaks", v); });

        layout->addStretch();
        return page;
    }

    // ---- Plugins page — full management UI ----
    QListWidget *pluginListWidget = nullptr;

    QWidget *createPluginsPage() {
        QWidget *page = new QWidget();
        QVBoxLayout *layout = new QVBoxLayout(page);
        layout->addWidget(new QLabel("<b>Plug-ins (Python)</b>"));
        layout->addSpacing(5);

        QLabel *desc = new QLabel(
            "Python plugins are loaded from <b>~/.config/winamp/plugins/</b><br>"
            "Each .py file that defines <code>on_winamp_start(api)</code> is a plugin.<br>"
            "Restart Winamp after making changes.", page);
        desc->setWordWrap(true);
        desc->setTextFormat(Qt::RichText);
        layout->addWidget(desc);
        layout->addSpacing(5);

        pluginListWidget = new QListWidget(page);
        pluginListWidget->setSelectionMode(QAbstractItemView::SingleSelection);
        populatePlugins();
        layout->addWidget(pluginListWidget, 1);

        // Status label
        QLabel *statusLabel = new QLabel("", page);
        statusLabel->setStyleSheet("color: #aaffaa; font-size: 8pt;");
        layout->addWidget(statusLabel);

        // Button row
        QHBoxLayout *btnRow = new QHBoxLayout();

        QPushButton *enableBtn = new QPushButton("Enable/Disable", page);
        QPushButton *configBtn = new QPushButton("Configure", page);
        QPushButton *addBtn = new QPushButton("Add...", page);
        QPushButton *removeBtn = new QPushButton("Remove", page);
        QPushButton *refreshBtn = new QPushButton("Refresh", page);
        QPushButton *openDirBtn = new QPushButton("Open Folder", page);

        btnRow->addWidget(enableBtn);
        btnRow->addWidget(configBtn);
        btnRow->addWidget(addBtn);
        btnRow->addWidget(removeBtn);
        btnRow->addWidget(refreshBtn);
        btnRow->addWidget(openDirBtn);
        layout->addLayout(btnRow);

        QString pluginDir = QStandardPaths::writableLocation(QStandardPaths::ConfigLocation) + "/winamp/plugins";

        // Enable/Disable toggle
        connect(enableBtn, &QPushButton::clicked, this, [this, statusLabel, pluginDir]() {
            QListWidgetItem *item = pluginListWidget->currentItem();
            if (!item) return;
            QString filename = item->data(Qt::UserRole).toString();
            QString filepath = pluginDir + "/" + filename;
            QString disabledPath = filepath + ".disabled";

            if (filename.endsWith(".disabled")) {
                // Re-enable: rename from .py.disabled to .py
                QString enabledPath = filepath.chopped(9); // strip .disabled
                if (QFile::rename(filepath, enabledPath)) {
                    statusLabel->setText("Enabled: " + QFileInfo(enabledPath).fileName() + " (restart to apply)");
                    populatePlugins();
                }
            } else {
                // Disable: rename from .py to .py.disabled
                if (QFile::rename(filepath, disabledPath)) {
                    statusLabel->setText("Disabled: " + filename + " (restart to apply)");
                    populatePlugins();
                }
            }
        });

        // Configure — open associated .json config or the .py file itself
        connect(configBtn, &QPushButton::clicked, this, [this, statusLabel, pluginDir]() {
            QListWidgetItem *item = pluginListWidget->currentItem();
            if (!item) return;
            QString filename = item->data(Qt::UserRole).toString();
            // Strip .disabled suffix if present
            QString baseName = filename;
            if (baseName.endsWith(".disabled")) baseName.chop(9);
            QString stem = QFileInfo(baseName).completeBaseName();

            // Look for a matching .json config file
            QString jsonPath = pluginDir + "/" + stem + ".json";
            QString pyPath = pluginDir + "/" + filename;

            if (QFile::exists(jsonPath)) {
                QDesktopServices::openUrl(QUrl::fromLocalFile(jsonPath));
                statusLabel->setText("Opened config: " + stem + ".json");
            } else {
                // Fall back to opening the .py file
                QDesktopServices::openUrl(QUrl::fromLocalFile(pyPath));
                statusLabel->setText("Opened script: " + filename + " (no .json config found)");
            }
        });

        // Add — copy a .py file into the plugins directory
        connect(addBtn, &QPushButton::clicked, this, [this, statusLabel, pluginDir]() {
            QString srcFile = QFileDialog::getOpenFileName(this, "Add Python Plugin",
                QDir::homePath(), "Python Plugins (*.py);;All Files (*)");
            if (srcFile.isEmpty()) return;

            QString destFile = pluginDir + "/" + QFileInfo(srcFile).fileName();
            if (QFile::exists(destFile)) {
                statusLabel->setText("Error: " + QFileInfo(srcFile).fileName() + " already exists");
                return;
            }
            if (QFile::copy(srcFile, destFile)) {
                statusLabel->setText("Added: " + QFileInfo(srcFile).fileName() + " (restart to load)");
                populatePlugins();
            } else {
                statusLabel->setText("Error: failed to copy plugin file");
            }
        });

        // Remove — delete the plugin file
        connect(removeBtn, &QPushButton::clicked, this, [this, statusLabel, pluginDir]() {
            QListWidgetItem *item = pluginListWidget->currentItem();
            if (!item) return;
            QString filename = item->data(Qt::UserRole).toString();
            QString filepath = pluginDir + "/" + filename;

            if (QFile::remove(filepath)) {
                statusLabel->setText("Removed: " + filename);
                populatePlugins();
            } else {
                statusLabel->setText("Error: could not remove " + filename);
            }
        });

        // Refresh — rescan the plugins directory
        connect(refreshBtn, &QPushButton::clicked, this, [this, statusLabel]() {
            populatePlugins();
            statusLabel->setText("Plugin list refreshed");
        });

        // Open plugins folder
        connect(openDirBtn, &QPushButton::clicked, this, [pluginDir]() {
            QDir().mkpath(pluginDir);
            QDesktopServices::openUrl(QUrl::fromLocalFile(pluginDir));
        });

        return page;
    }

    void populatePlugins() {
        if (!pluginListWidget) return;
        pluginListWidget->clear();

        QString pluginDir = QStandardPaths::writableLocation(QStandardPaths::ConfigLocation) + "/winamp/plugins";
        QDir dir(pluginDir);
        if (!dir.exists()) {
            dir.mkpath(".");
            return;
        }

        // List both .py (enabled) and .py.disabled (disabled) files
        QStringList filters;
        filters << "*.py" << "*.py.disabled";
        QStringList files = dir.entryList(filters, QDir::Files, QDir::Name);

        for (const QString &filename : files) {
            bool disabled = filename.endsWith(".disabled");
            QString displayName = filename;
            if (disabled) {
                displayName = filename.chopped(9); // strip .disabled
            }

            // Try to read a description from the first docstring or comment
            QString desc = "";
            QFile f(dir.absoluteFilePath(filename));
            if (f.open(QIODevice::ReadOnly | QIODevice::Text)) {
                QTextStream in(&f);
                // Read first few lines looking for a comment or docstring
                for (int i = 0; i < 5 && !in.atEnd(); ++i) {
                    QString line = in.readLine().trimmed();
                    if (line.startsWith("#") && !line.startsWith("#!")) {
                        desc = line.mid(1).trimmed();
                        if (desc.startsWith("-")) desc = desc.mid(1).trimmed();
                        break;
                    }
                }
                f.close();
            }

            // Check if a .json config exists
            QString stem = QFileInfo(displayName).completeBaseName();
            bool hasConfig = QFile::exists(pluginDir + "/" + stem + ".json");

            QString label = displayName;
            if (!desc.isEmpty()) label += "  —  " + desc;
            if (disabled) label += "  [DISABLED]";
            if (hasConfig) label += "  [⚙]";

            QListWidgetItem *item = new QListWidgetItem(label);
            item->setData(Qt::UserRole, filename); // store real filename
            if (disabled) {
                item->setForeground(QColor(120, 120, 120));
            } else {
                item->setForeground(QColor(0, 255, 0));
            }
            pluginListWidget->addItem(item);
        }
    }

    // ---- Skin list helpers ----
    QListWidget *skinListWidget = nullptr;

    void populateSkins() {
        if (!skinListWidget) return;
        // Find the built-in default skin path (check all known locations)
        QString appDir = QCoreApplication::applicationDirPath();
        QStringList defaultCandidates = {
            appDir + "/../skins/default",
            appDir + "/../../skins/default",
            appDir + "/../assets",
            appDir + "/../../assets",
            appDir + "/../share/winamp/skins/default",
            appDir + "/../share/winamp/resource",
            "/usr/share/winamp/skins/default",
            "/usr/share/winamp/resource",
            "/usr/local/share/winamp/skins/default",
            "/usr/local/share/winamp/resource",
            QDir::homePath() + "/.local/share/winamp/skins/default",
            QDir::homePath() + "/.winamp/skins/default"
        };
        for (const QString &p : defaultCandidates) {
            QDir d(p);
            if (d.exists()) {
                QStringList bmps = d.entryList(QStringList() << "*.bmp" << "*.BMP", QDir::Files);
                if (!bmps.isEmpty()) {
                    defaultSkinPath = d.absolutePath();
                    break;
                }
            }
        }
        
        // Always show "Winamp Default" as the first entry
        skinListWidget->addItem("Winamp Default");
        
        QDir skinsDir(QDir::homePath() + "/.winamp/skins");
        if (!skinsDir.exists()) {
            skinsDir.mkpath(".");
        }
        // List directories (unzipped skins), skip "default" since we already show it
        QStringList skinFolders = skinsDir.entryList(QDir::Dirs | QDir::NoDotAndDotDot);
        for (const QString &folder : skinFolders) {
            if (folder.toLower() != "default")
                skinListWidget->addItem(folder);
        }
        
        // List .wsz and .zip archives (skip .wal — modern/Bento skins, not compatible)
        QStringList archiveFilters;
        archiveFilters << "*.wsz" << "*.WSZ" << "*.zip" << "*.ZIP";
        QStringList archiveFiles = skinsDir.entryList(archiveFilters, QDir::Files);
        for (const QString &f : archiveFiles) {
            QFileInfo fi(f);
            skinListWidget->addItem(fi.fileName());
        }
    }

    // ---- Modern Skins page ----
    QListWidget *modernSkinListWidget = nullptr;

    QWidget *createModernSkinsPage() {
        QWidget *page = new QWidget();
        QVBoxLayout *layout = new QVBoxLayout(page);
        layout->addWidget(new QLabel("<b>Modern Skins (XML)</b>"));

        modernSkinListWidget = new QListWidget(page);
        populateModernSkins();
        connect(modernSkinListWidget, &QListWidget::itemDoubleClicked, this, &PreferencesDialog::onModernSkinSelected);
        layout->addWidget(modernSkinListWidget);

        QHBoxLayout *btnRow = new QHBoxLayout();
        QPushButton *openDirBtn = new QPushButton("Open Skins Folder", page);
        connect(openDirBtn, &QPushButton::clicked, this, []() {
            QString skinsDir = QDir::homePath() + "/.winamp/skins";
            QDir().mkpath(skinsDir);
            QDesktopServices::openUrl(QUrl::fromLocalFile(skinsDir));
        });
        btnRow->addWidget(openDirBtn);
        btnRow->addStretch();
        layout->addLayout(btnRow);
        return page;
    }

    void populateModernSkins() {
        if (!modernSkinListWidget) return;

        // Scan built-in modern skins from source tree
        QString appDir = QCoreApplication::applicationDirPath();
        QStringList builtinDirs = {
            appDir + "/../Src/resources/skins",
            appDir + "/../../Src/resources/skins"
        };
        QSet<QString> addedNames;
        for (const QString &base : builtinDirs) {
            QDir dir(base);
            if (!dir.exists()) continue;
            QStringList folders = dir.entryList(QDir::Dirs | QDir::NoDotAndDotDot);
            for (const QString &folder : folders) {
                QString fullPath = dir.absoluteFilePath(folder);
                if (QFile::exists(fullPath + "/skin.xml") && !addedNames.contains(folder)) {
                    addedNames.insert(folder);
                    modernSkinListWidget->addItem(folder);
                    modernSkinListWidget->item(modernSkinListWidget->count() - 1)->setData(Qt::UserRole, fullPath);
                }
            }
        }

        // List modern skins in user's skins directory (directories with skin.xml)
        QDir skinsDir(QDir::homePath() + "/.winamp/skins");
        if (skinsDir.exists()) {
            QStringList folders = skinsDir.entryList(QDir::Dirs | QDir::NoDotAndDotDot);
            for (const QString &folder : folders) {
                if (addedNames.contains(folder)) continue;
                QString fullPath = skinsDir.absolutePath() + "/" + folder;
                if (QFile::exists(fullPath + "/skin.xml")) {
                    addedNames.insert(folder);
                    modernSkinListWidget->addItem(folder);
                    modernSkinListWidget->item(modernSkinListWidget->count() - 1)->setData(Qt::UserRole, fullPath);
                }
            }

            // List .wal archives
            QStringList walFiles = skinsDir.entryList(QStringList() << "*.wal" << "*.WAL", QDir::Files);
            for (const QString &f : walFiles) {
                modernSkinListWidget->addItem(f);
                modernSkinListWidget->item(modernSkinListWidget->count() - 1)->setData(Qt::UserRole, skinsDir.absolutePath() + "/" + f);
            }
        }
    }

    void onModernSkinSelected(QListWidgetItem *item) {
        QString skinPath = item->data(Qt::UserRole).toString();
        qDebug() << "ModernSkin: selected item" << item->text() << "path:" << skinPath;
        if (skinPath.isEmpty()) return;

        // If it's a .wal file, extract it first (ZIP archive)
        if (skinPath.endsWith(".wal", Qt::CaseInsensitive)) {
            QString extracted = extractSkinArchive(skinPath);
            if (!extracted.isEmpty())
                skinPath = extracted;
            else
                return;
        }

        if (QFile::exists(skinPath + "/skin.xml")) {
            qDebug() << "ModernSkin: emitting skinChanged for" << skinPath;
            emit skinChanged(skinPath);
        } else {
            qDebug() << "ModernSkin: skin.xml NOT found at" << skinPath + "/skin.xml";
        }
    }

    void onSkinSelected(QListWidgetItem *item) {
        QString skinName = item->text();
        
        // Handle "Winamp Default" entry
        if (skinName == "Winamp Default") {
            if (!defaultSkinPath.isEmpty()) {
                emit skinChanged(defaultSkinPath);
            }
            return;
        }
        
        QString skinsBase = QDir::homePath() + "/.winamp/skins/";
        QString fullPath = skinsBase + skinName;
        
        // Check if it's an archive (.wsz or .zip)
        if (skinName.endsWith(".wsz", Qt::CaseInsensitive) ||
            skinName.endsWith(".zip", Qt::CaseInsensitive)) {
            QString extracted = extractSkinArchive(fullPath);
            if (!extracted.isEmpty()) {
                emit skinChanged(extracted);
            }
        } else {
            emit skinChanged(fullPath);
        }
    }
};

