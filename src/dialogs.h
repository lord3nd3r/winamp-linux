// dialogs.h — Various dialog boxes (Jump to File, File Info, About, Play Location)
#pragma once

#include <QDialog>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QTextEdit>
#include <QListWidget>
#include <QPushButton>
#include <QTabWidget>
#include <QFormLayout>
#include <QGroupBox>
#include <QComboBox>
#include <QFileInfo>
#include <QDateTime>
#include <QMediaPlayer>
#include <QMessageBox>
#include <QDesktopServices>
#include <QUrl>
#include <QEvent>
#include <QKeyEvent>
#include <QPainter>
#include <QTimer>
#include <QSignalBlocker>
#include <cmath>
#include "constants.h"
#include "translator.h"
#include "tag_metadata.h"

class JumpToFileDialog : public QDialog {
    Q_OBJECT
public:
    JumpToFileDialog(const QStringList &tracks, QWidget *parent = nullptr)
        : QDialog(parent), allTracks(tracks)
    {
        setWindowTitle("Jump to File");
        setMinimumSize(400, 350);
        setStyleSheet("background-color: #2b2b3d; color: #00ff00;");

        QVBoxLayout *layout = new QVBoxLayout(this);

        QLabel *label = new QLabel("Search:", this);
        searchEdit = new QLineEdit(this);
        searchEdit->setStyleSheet("background-color: #000; color: #00FF00; border: 1px solid #555; padding: 4px;");
        searchEdit->setPlaceholderText("Type to filter playlist...");
        connect(searchEdit, &QLineEdit::textChanged, this, &JumpToFileDialog::filterList);

        resultList = new QListWidget(this);
        resultList->setStyleSheet(
            "QListWidget { background-color: #000; color: #00FF00; border: 1px solid #555; }"
            "QListWidget::item:selected { background-color: #0000C6; }"
        );
        connect(resultList, &QListWidget::itemDoubleClicked, this, &JumpToFileDialog::onItemSelected);

        QHBoxLayout *btnLayout = new QHBoxLayout();
        QPushButton *playBtn = new QPushButton("Play", this);
        QPushButton *queueBtn = new QPushButton("Queue", this);
        QPushButton *cancelBtn = new QPushButton("Close", this);
        connect(playBtn, &QPushButton::clicked, this, [this]() {
            if (resultList->currentRow() >= 0 && resultList->currentRow() < filteredIndices.size())
                selectedIndex = filteredIndices[resultList->currentRow()];
            accept();
        });
        connect(queueBtn, &QPushButton::clicked, this, [this]() {
            if (resultList->currentRow() >= 0 && resultList->currentRow() < filteredIndices.size()) {
                int idx = filteredIndices[resultList->currentRow()];
                emit queueTrack(idx);
            }
        });
        connect(cancelBtn, &QPushButton::clicked, this, &QDialog::reject);

        btnLayout->addStretch();
        btnLayout->addWidget(playBtn);
        btnLayout->addWidget(queueBtn);
        btnLayout->addWidget(cancelBtn);

        layout->addWidget(label);
        layout->addWidget(searchEdit);
        layout->addWidget(resultList);
        layout->addLayout(btnLayout);

        // Populate initially with all tracks
        filterList("");
        searchEdit->setFocus();
        selectedIndex = -1;
    }

    int getSelectedIndex() const { return selectedIndex; }

signals:
    void queueTrack(int index);

private slots:
    void filterList(const QString &text) {
        resultList->clear();
        filteredIndices.clear();
        for (int i = 0; i < allTracks.size(); i++) {
            QString display = QString("%1. %2").arg(i + 1).arg(QFileInfo(allTracks[i]).fileName());
            if (text.isEmpty() || display.contains(text, Qt::CaseInsensitive) ||
                allTracks[i].contains(text, Qt::CaseInsensitive)) {
                resultList->addItem(display);
                filteredIndices.append(i);
            }
        }
        if (resultList->count() > 0)
            resultList->setCurrentRow(0);
    }

    void onItemSelected(QListWidgetItem *) {
        if (resultList->currentRow() >= 0 && resultList->currentRow() < filteredIndices.size())
            selectedIndex = filteredIndices[resultList->currentRow()];
        accept();
    }

private:
    QStringList allTracks;
    QList<int> filteredIndices;
    QLineEdit *searchEdit;
    QListWidget *resultList;
    int selectedIndex = -1;
};

// Classic Winamp 2.x File Info / ID3 editor (Alt+3).
// Loads and saves tags via TagLib (ID3v1+ID3v2 for MP3; native tags for other formats).
class FileInfoDialog : public QDialog {
    Q_OBJECT
public:
    FileInfoDialog(const QString &filePath, QMediaPlayer *player = nullptr, QWidget *parent = nullptr)
        : QDialog(parent), m_filePath(filePath), m_player(player)
    {
        Q_UNUSED(m_player);
        setWindowTitle(TR("win.fileinfo.title", "File Info") + " - " + QFileInfo(filePath).fileName());
        setMinimumSize(480, 440);
        setStyleSheet("background-color: #2b2b3d; color: #00ff00;");

        QVBoxLayout *mainLayout = new QVBoxLayout(this);

        QLabel *fileLabel = new QLabel("<b>File:</b> " + filePath, this);
        fileLabel->setWordWrap(true);
        fileLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
        mainLayout->addWidget(fileLabel);

        QTabWidget *tabs = new QTabWidget(this);
        tabs->setStyleSheet(
            "QTabWidget::pane { border: 1px solid #555; background: #1a1a2e; }"
            "QTabBar::tab { background: #333; color: #00ff00; padding: 6px 12px; margin-right: 2px; }"
            "QTabBar::tab:selected { background: #0000c6; font-weight: bold; }"
        );

        // --- ID3 / Metadata tab (Winamp 2.x id3 editor fields) ---
        QWidget *metadataTab = new QWidget();
        QFormLayout *metaLayout = new QFormLayout(metadataTab);
        metaLayout->setLabelAlignment(Qt::AlignRight);

        const QString editStyle =
            "background-color: #000; color: #00ff00; border: 1px solid #555; padding: 4px;";
        const QString comboStyle =
            "background-color: #000; color: #00ff00; border: 1px solid #555; padding: 2px;"
            "selection-background-color: #0000c6;";

        titleEdit = new QLineEdit(metadataTab);
        artistEdit = new QLineEdit(metadataTab);
        albumEdit = new QLineEdit(metadataTab);
        yearEdit = new QLineEdit(metadataTab);
        yearEdit->setMaxLength(4);
        yearEdit->setMaximumWidth(80);
        trackEdit = new QLineEdit(metadataTab);
        trackEdit->setMaximumWidth(80);
        genreCombo = new QComboBox(metadataTab);
        genreCombo->setEditable(true);
        genreCombo->setInsertPolicy(QComboBox::NoInsert);
        genreCombo->addItem(QString()); // empty genre
        genreCombo->addItems(TagMetadata::id3v1Genres());
        commentEdit = new QTextEdit(metadataTab);
        commentEdit->setMaximumHeight(80);

        titleEdit->setStyleSheet(editStyle);
        artistEdit->setStyleSheet(editStyle);
        albumEdit->setStyleSheet(editStyle);
        yearEdit->setStyleSheet(editStyle);
        trackEdit->setStyleSheet(editStyle);
        genreCombo->setStyleSheet(comboStyle);
        commentEdit->setStyleSheet(editStyle);

        // Winamp 2.x order: Title, Artist, Album, Year, Track #, Genre, Comment
        metaLayout->addRow("Title:", titleEdit);
        metaLayout->addRow("Artist:", artistEdit);
        metaLayout->addRow("Album:", albumEdit);
        metaLayout->addRow("Year:", yearEdit);
        metaLayout->addRow("Track #:", trackEdit);
        metaLayout->addRow("Genre:", genreCombo);
        metaLayout->addRow("Comment:", commentEdit);

        id3StatusLabel = new QLabel(metadataTab);
        id3StatusLabel->setStyleSheet("color: #888; font-size: 9pt;");
        metaLayout->addRow(QString(), id3StatusLabel);

        // TagLib is the source of truth (file on disk), matching classic in_mp3 File Info.
        loadFromDisk();

        tabs->addTab(metadataTab, "ID3");

        // --- Technical tab ---
        QWidget *techTab = new QWidget();
        QFormLayout *techLayout = new QFormLayout(techTab);
        techLayout->setLabelAlignment(Qt::AlignRight);

        QFileInfo fi(filePath);
        techLayout->addRow("File size:",
            new QLabel(QString::number(fi.size() / 1024.0, 'f', 1) + " KB"));
        techLayout->addRow("Modified:",
            new QLabel(fi.lastModified().toString("yyyy-MM-dd hh:mm:ss")));
        techLayout->addRow("Location:", new QLabel(fi.absolutePath()));

        MediaAudioInfo ainfo = TagMetadata::readAudioInfo(filePath);
        if (ainfo.valid) {
            techLayout->addRow("Format:", new QLabel(ainfo.format));
            if (!ainfo.details.isEmpty())
                techLayout->addRow("Details:", new QLabel(ainfo.details));
            if (ainfo.bitrateKbps > 0)
                techLayout->addRow("Bitrate:",
                    new QLabel(QString::number(ainfo.bitrateKbps) + " kbps"));
            if (ainfo.sampleRate > 0)
                techLayout->addRow("Sample rate:",
                    new QLabel(QString::number(ainfo.sampleRate) + " Hz"));
            if (ainfo.channels > 0) {
                QString ch = (ainfo.channels == 1) ? "Mono"
                           : (ainfo.channels == 2) ? "Stereo"
                           : QString::number(ainfo.channels) + " ch";
                techLayout->addRow("Channels:", new QLabel(ch));
            }
            if (ainfo.lengthSeconds > 0) {
                int secs = ainfo.lengthSeconds;
                int mins = secs / 60;
                secs %= 60;
                techLayout->addRow("Length:",
                    new QLabel(QString("%1:%2").arg(mins).arg(secs, 2, 10, QChar('0'))));
            }
            QString tagTypes;
            if (ainfo.hasId3v1) tagTypes += "ID3v1 ";
            if (ainfo.hasId3v2) tagTypes += "ID3v2 ";
            if (tagTypes.isEmpty()) tagTypes = "(none / non-ID3)";
            techLayout->addRow("Tags on disk:", new QLabel(tagTypes.trimmed()));
        } else if (isRemoteMediaLocation(filePath)) {
            techLayout->addRow("Note:",
                new QLabel("Remote stream — tags cannot be written."));
        } else {
            techLayout->addRow("Note:",
                new QLabel("Could not read audio properties for this file."));
        }

        techLayout->addItem(new QSpacerItem(1, 1, QSizePolicy::Minimum, QSizePolicy::Expanding));
        tabs->addTab(techTab, "MPEG Info");

        mainLayout->addWidget(tabs);

        // Buttons: Update (save), Remove tag, Cancel — Winamp-style actions
        QHBoxLayout *btnLayout = new QHBoxLayout();
        QPushButton *updateBtn = new QPushButton("Update", this);
        QPushButton *removeBtn = new QPushButton("Remove tag", this);
        QPushButton *cancelBtn = new QPushButton("Cancel", this);
        updateBtn->setStyleSheet("background: #0000c6; color: #fff; padding: 6px 16px;");
        removeBtn->setStyleSheet("background: #333; color: #00ff00; padding: 6px 16px;");
        cancelBtn->setStyleSheet("background: #333; color: #00ff00; padding: 6px 16px;");
        updateBtn->setDefault(true);

        const bool writable = TagMetadata::canTagFile(filePath) && QFileInfo(filePath).isWritable();
        updateBtn->setEnabled(writable);
        removeBtn->setEnabled(writable);
        if (!writable && !isRemoteMediaLocation(filePath)) {
            id3StatusLabel->setText("File is not writable — view only.");
        } else if (isRemoteMediaLocation(filePath)) {
            id3StatusLabel->setText("Remote URL — tags cannot be edited.");
            titleEdit->setReadOnly(true);
            artistEdit->setReadOnly(true);
            albumEdit->setReadOnly(true);
            yearEdit->setReadOnly(true);
            trackEdit->setReadOnly(true);
            genreCombo->setEnabled(false);
            commentEdit->setReadOnly(true);
        }

        connect(updateBtn, &QPushButton::clicked, this, &FileInfoDialog::onSave);
        connect(removeBtn, &QPushButton::clicked, this, &FileInfoDialog::onRemoveTag);
        connect(cancelBtn, &QPushButton::clicked, this, &QDialog::reject);

        btnLayout->addWidget(removeBtn);
        btnLayout->addStretch();
        btnLayout->addWidget(updateBtn);
        btnLayout->addWidget(cancelBtn);
        mainLayout->addLayout(btnLayout);
    }

signals:
    // Emitted after tags are written so playlist / ticker can refresh (IPC-style).
    void tagsSaved(const QString &filePath);

private slots:
    void onSave() {
        if (!TagMetadata::canTagFile(m_filePath)) {
            QMessageBox::warning(this, "File Info",
                "Cannot write tags for this location.");
            return;
        }
        MediaTags tags = tagsFromUi();
        QString err;
        if (!TagMetadata::writeTags(m_filePath, tags, &err)) {
            QMessageBox::critical(this, "File Info",
                "Failed to save tags:\n" + err);
            return;
        }
        emit tagsSaved(m_filePath);
        accept();
    }

    void onRemoveTag() {
        if (!TagMetadata::canTagFile(m_filePath))
            return;
        auto reply = QMessageBox::question(this, "Remove tag",
            "Remove all embedded tags from this file?\n"
            "(ID3v1 and ID3v2 on MPEG; clears native tags on other formats.)",
            QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
        if (reply != QMessageBox::Yes)
            return;
        QString err;
        if (!TagMetadata::stripTags(m_filePath, &err)) {
            QMessageBox::critical(this, "File Info",
                "Failed to remove tags:\n" + err);
            return;
        }
        // Clear UI to match disk
        titleEdit->clear();
        artistEdit->clear();
        albumEdit->clear();
        yearEdit->clear();
        trackEdit->clear();
        genreCombo->setCurrentIndex(0);
        genreCombo->setEditText(QString());
        commentEdit->clear();
        id3StatusLabel->setText("Tags removed.");
        emit tagsSaved(m_filePath);
    }

private:
    void loadFromDisk() {
        MediaTags tags = TagMetadata::readTags(m_filePath);
        if (!tags.valid && !isRemoteMediaLocation(m_filePath)) {
            id3StatusLabel->setText("No tags found (or format not supported).");
            return;
        }
        titleEdit->setText(tags.title);
        artistEdit->setText(tags.artist);
        albumEdit->setText(tags.album);
        yearEdit->setText(tags.year);
        trackEdit->setText(tags.track > 0 ? QString::number(tags.track) : QString());
        {
            QSignalBlocker b(genreCombo);
            int idx = genreCombo->findText(tags.genre, Qt::MatchFixedString);
            if (idx >= 0)
                genreCombo->setCurrentIndex(idx);
            else {
                genreCombo->setCurrentIndex(0);
                genreCombo->setEditText(tags.genre);
            }
        }
        commentEdit->setPlainText(tags.comment);

        MediaAudioInfo ainfo = TagMetadata::readAudioInfo(m_filePath);
        QStringList bits;
        if (ainfo.hasId3v1) bits << "ID3v1";
        if (ainfo.hasId3v2) bits << "ID3v2";
        if (bits.isEmpty() && tags.valid &&
            (!tags.title.isEmpty() || !tags.artist.isEmpty() || !tags.album.isEmpty()))
            bits << "embedded tag";
        if (!bits.isEmpty())
            id3StatusLabel->setText("Present: " + bits.join(", "));
        else
            id3StatusLabel->setText("No ID3 tags yet — Update will create them.");
    }

    MediaTags tagsFromUi() const {
        MediaTags t;
        t.title = titleEdit->text().trimmed();
        t.artist = artistEdit->text().trimmed();
        t.album = albumEdit->text().trimmed();
        t.year = yearEdit->text().trimmed();
        t.genre = genreCombo->currentText().trimmed();
        t.comment = commentEdit->toPlainText().trimmed();
        bool ok = false;
        int trk = trackEdit->text().trimmed().toInt(&ok);
        t.track = (ok && trk > 0) ? trk : 0;
        t.valid = true;
        return t;
    }

    QString m_filePath;
    QMediaPlayer *m_player = nullptr;

    QLineEdit *titleEdit = nullptr;
    QLineEdit *artistEdit = nullptr;
    QLineEdit *albumEdit = nullptr;
    QLineEdit *yearEdit = nullptr;
    QLineEdit *trackEdit = nullptr;
    QComboBox *genreCombo = nullptr;
    QTextEdit *commentEdit = nullptr;
    QLabel *id3StatusLabel = nullptr;
};

class AboutDialog : public QDialog {
    Q_OBJECT
public:
    AboutDialog(const QString &skinPath, QWidget *parent = nullptr) : QDialog(parent) {
        setWindowTitle("About Winamp");
        setFixedSize(480, 360);

        // Load splash2.bmp and team.bmp
        QStringList searchPaths;
        searchPaths << skinPath << skinPath + "/../skins/default" << skinPath + "/../../skins/default";
        for (const auto &p : searchPaths) {
            if (splashImg.isNull()) {
                splashImg = QImage(p + "/splash2.bmp");
                if (splashImg.isNull()) splashImg = QImage(p + "/Splash2.bmp");
            }
            if (teamImg.isNull()) {
                teamImg = QImage(p + "/team.bmp");
                if (teamImg.isNull()) teamImg = QImage(p + "/Team.bmp");
            }
        }
        if (!splashImg.isNull()) splashImg = splashImg.convertToFormat(QImage::Format_ARGB32);
        if (!teamImg.isNull()) teamImg = teamImg.convertToFormat(QImage::Format_ARGB32);

        // Build team frames (32x32 tiles stacked vertically)
        if (!teamImg.isNull()) {
            int nFrames = teamImg.height() / 32;
            for (int i = 0; i < nFrames; i++)
                teamFrames.append(teamImg.copy(0, i * 32, 32, 32));
        }

        // Credits text (from original creditsrend.c)
        credits = QStringList{
            QString("Winamp v%1\n    The Credits").arg(QString::fromUtf8(kWinampVersion)),
            "Linux Qt6 Port:\n    Kristopher Craig",
            "Winamp for Linux\n    Qt6 Native Port",
            "Original Development:\n Quentin Hebette, Thierry Honore,\n Lionel Peeters, Hakan Danisik,\n Eddy Richman, Jef Mauguit",
            "QA, Engineering & Support:\n    DJ Egg",
            "Freeform Skin Engine:\n    Linus Brolin",
            "Bento Skin:\n    Martin Pohlmann, Taber Buhl,\n    Ben Allison, Victor Brocaz",
            "Winamp Hall-of-Fame:\n    Justin Frankel,\n    Christophe Thibault,\n    Francis Gastellu,\n    Brennan Underwood",
            "    Peter Pawlowski, Tom Pepper,\n    Ryan Geiss, Will Fisher,\n    Maksim Tyrtyshny, Darren Owen,\n    Ben Allison",
            "Modern Skin:\n    Sven Kistner",
            "PCM EQ magic:\n    4Front Technologies / George Yohng",
            "Intro sound:\n    JJ McKay",
            "Credits rendered with Plush:\n    http://www.cockos.com/wdl/\n    (8bpp foreva)",
            "Thanks:\n    NS Beta Team & Craig Freer,\n    Our lowly forum moderators,\n    Our precious skin reviewers",
            QString::fromUtf8("Copyright \u00A9 1997-2026 Winamp SA\n    www.winamp.com"),
            "It really whips\n    the llama's ass!",
        };

        // Init starfield
        for (int i = 0; i < NUM_STARS; i++) {
            stars[i].x = (rand() % 2000 - 1000) / 1000.0;
            stars[i].y = (rand() % 2000 - 1000) / 1000.0;
            stars[i].z = (rand() % 1000) / 1000.0;
            stars[i].speed = 0.003 + (rand() % 100) / 10000.0;
        }

        // Init warp lookup table (sqrt table for radial distance)
        for (int i = 0; i < 65536; i++)
            sqTable[i] = (int)sqrt((double)i);

        // Init credit state
        creditIndex = 0;
        creditFrame = 0;
        creditX = rand() % 160 + 20;
        creditY = rand() % 80 + 40;

        // Animation timer — 33fps like the original
        animTimer = new QTimer(this);
        connect(animTimer, &QTimer::timeout, this, &AboutDialog::tick);
        animTimer->start(30);
        frameCount = 0;
        warpPhase = 0;
    }

protected:
    void paintEvent(QPaintEvent *) override {
        QPainter p(this);
        p.setRenderHint(QPainter::SmoothPixmapTransform, false);

        int w = width(), h = height();

        // Black background
        p.fillRect(0, 0, w, h, Qt::black);

        // === Layer 1: Starfield ===
        for (int i = 0; i < NUM_STARS; i++) {
            double sx = stars[i].x / stars[i].z;
            double sy = stars[i].y / stars[i].z;
            int px = (int)(w / 2 + sx * w / 2);
            int py = (int)(h / 2 + sy * h / 2);
            if (px >= 0 && px < w && py >= 0 && py < h) {
                int brightness = (int)(255 * (1.0 - stars[i].z));
                brightness = qBound(40, brightness, 255);
                int sz = (stars[i].z < 0.3) ? 2 : 1;
                p.fillRect(px, py, sz, sz, QColor(brightness, brightness, brightness + 40));
            }
        }

        // === Layer 2: Warped splash image (sinusoidal zoom from original ABOUT.cpp) ===
        if (!splashImg.isNull()) {
            int sw = splashImg.width(), sh = splashImg.height();
            int dw = 280, dh = (int)(280.0 * sh / sw);
            int dx = (w - dw) / 2, dy = 30;

            // Create warped version
            QImage warpedImg(dw, dh, QImage::Format_ARGB32);
            warpedImg.fill(Qt::transparent);

            double maxD = sqrt(dw * dw / 4.0 + dh * dh / 4.0);
            double wt = warpPhase / 128.0; // 0..1 cycle
            double dpos = 1.0 + sin(wt * M_PI);

            for (int y = 0; y < dh; y++) {
                QRgb *scanline = (QRgb *)warpedImg.scanLine(y);
                for (int x = 0; x < dw; x++) {
                    double fx = x - dw / 2.0;
                    double fy = y - dh / 2.0;
                    double dist = sqrt(fx * fx + fy * fy);

                    // Sinusoidal radial distortion
                    double scale;
                    if (dist < 1.0) {
                        scale = 1.0;
                    } else {
                        scale = pow(sin(dist / maxD * M_PI / 2.0), dpos) * 1.5 * maxD / (dist + 1.0);
                        scale = qBound(0.1, scale, 3.0);
                    }

                    int srcX = (int)(sw / 2.0 + fx * sw / (dw * scale));
                    int srcY = (int)(sh / 2.0 + fy * sh / (dh * scale));
                    srcX = qBound(0, srcX, sw - 1);
                    srcY = qBound(0, srcY, sh - 1);
                    scanline[x] = splashImg.pixel(srcX, srcY);
                }
            }

            // Draw with slight alpha pulsing
            int alpha = 180 + (int)(75.0 * sin(frameCount * 0.05));
            p.setOpacity(alpha / 255.0);
            p.drawImage(dx, dy, warpedImg);
            p.setOpacity(1.0);
        }

        // === Layer 3: Rotating team cube frames ===
        if (!teamFrames.isEmpty()) {
            int fi = (frameCount / 8) % teamFrames.size();
            QImage frame = teamFrames[fi].scaled(64, 64, Qt::KeepAspectRatio);

            // Orbit position
            double angle = frameCount * 0.03;
            int cx = w / 2 + (int)(140 * cos(angle));
            int cy = h / 2 + (int)(50 * sin(angle * 0.7));

            // Slight 3D rotation perspective (fake via shear)
            p.save();
            p.translate(cx, cy);
            double rot = sin(frameCount * 0.04) * 15.0;
            p.rotate(rot);
            double scaleF = 0.8 + 0.2 * sin(frameCount * 0.025);
            p.scale(scaleF, scaleF);
            p.setOpacity(0.85);
            p.drawImage(-32, -32, frame);
            p.restore();
            p.setOpacity(1.0);
        }

        // === Layer 4: Glowing fire spheres ===
        for (int s = 0; s < 2; s++) {
            double angle = frameCount * (s == 0 ? 0.02 : -0.025) + s * M_PI;
            int sx = w / 2 + (int)(180 * cos(angle));
            int sy = h / 2 + (int)(80 * sin(angle * 1.3));
            int radius = 12 + (int)(4 * sin(frameCount * 0.06 + s));

            // Fire gradient
            QRadialGradient grad(sx, sy, radius * 3);
            if (s == 0) {
                grad.setColorAt(0.0, QColor(255, 200, 80, 200));
                grad.setColorAt(0.3, QColor(255, 120, 20, 150));
                grad.setColorAt(0.6, QColor(200, 40, 0, 80));
                grad.setColorAt(1.0, QColor(0, 0, 0, 0));
            } else {
                grad.setColorAt(0.0, QColor(100, 180, 255, 200));
                grad.setColorAt(0.3, QColor(40, 100, 255, 150));
                grad.setColorAt(0.6, QColor(20, 40, 200, 80));
                grad.setColorAt(1.0, QColor(0, 0, 0, 0));
            }
            p.setBrush(grad);
            p.setPen(Qt::NoPen);
            p.drawEllipse(QPoint(sx, sy), radius * 3, radius * 3);
        }

        // === Layer 5: Credits text (fade in/out at random positions) ===
        if (creditIndex < credits.size()) {
            int opacity = 0;
            // 128-frame cycle per credit: 0-15 hidden, 16-31 fade in, 32-111 visible, 112-127 fade out
            if (creditFrame < 16) {
                opacity = 0;
            } else if (creditFrame < 32) {
                opacity = (creditFrame - 16) * 255 / 16;
            } else if (creditFrame < 112) {
                opacity = 255;
            } else {
                opacity = (127 - creditFrame) * 255 / 16;
            }

            if (opacity > 0) {
                p.setOpacity(opacity / 255.0);
                QFont font("Tahoma", 11);
                font.setBold(true);
                p.setFont(font);

                // Drop shadow
                p.setPen(QColor(0, 0, 0));
                p.drawText(QRect(creditX + 1, creditY + 1, w - 40, h - 40),
                           Qt::AlignLeft | Qt::TextWordWrap, credits[creditIndex]);
                // Green text
                p.setPen(QColor(0, 255, 0));
                p.drawText(QRect(creditX, creditY, w - 40, h - 40),
                           Qt::AlignLeft | Qt::TextWordWrap, credits[creditIndex]);
                p.setOpacity(1.0);
            }
        }

        // === FPS counter (bottom left, like the original) ===
        p.setPen(QColor(80, 80, 80));
        p.setFont(QFont("Courier", 8));
        p.drawText(5, h - 5, QString("%1 fps").arg(currentFps, 0, 'f', 0));

        // === Bottom bar: product version (matches GitHub releases) ===
        p.setPen(QColor(100, 100, 100));
        p.setFont(QFont("Tahoma", 8));
        p.drawText(0, h - 18, w, 15, Qt::AlignCenter,
                   QString("%1 — Qt Native Port").arg(QString::fromUtf8(kWinampAboutLine)));
    }

    void keyPressEvent(QKeyEvent *e) override {
        if (e->key() == Qt::Key_Escape || e->key() == Qt::Key_Return)
            accept();
    }
    void mousePressEvent(QMouseEvent *) override { accept(); }

private slots:
    void tick() {
        frameCount++;

        // Update starfield
        for (int i = 0; i < NUM_STARS; i++) {
            stars[i].z -= stars[i].speed;
            if (stars[i].z <= 0.001) {
                stars[i].x = (rand() % 2000 - 1000) / 1000.0;
                stars[i].y = (rand() % 2000 - 1000) / 1000.0;
                stars[i].z = 1.0;
                stars[i].speed = 0.003 + (rand() % 100) / 10000.0;
            }
        }

        // Update warp phase (0-255 cycle)
        warpPhase = (warpPhase + 1) & 0xFF;

        // Update credits (128-frame cycle per credit block)
        creditFrame++;
        if (creditFrame >= 128) {
            creditFrame = 0;
            creditIndex++;
            if (creditIndex >= credits.size()) creditIndex = 0;
            creditX = rand() % (width() / 2) + 20;
            creditY = rand() % (height() / 3) + (height() / 3);
        }

        // FPS calculation
        if (frameCount % 30 == 0) {
            qint64 now = QDateTime::currentMSecsSinceEpoch();
            if (lastFpsTime > 0)
                currentFps = 30000.0 / (now - lastFpsTime);
            lastFpsTime = now;
        }

        update();
    }

private:
    static constexpr int NUM_STARS = 200;
    struct Star { double x, y, z, speed; };
    Star stars[NUM_STARS];

    QImage splashImg, teamImg;
    QList<QImage> teamFrames;
    QStringList credits;

    QTimer *animTimer;
    int frameCount = 0;
    int warpPhase = 0;
    int sqTable[65536];

    int creditIndex = 0;
    int creditFrame = 0;
    int creditX = 100, creditY = 150;

    double currentFps = 0;
    qint64 lastFpsTime = 0;
};

class PlayLocationDialog : public QDialog {
public:
    PlayLocationDialog(QWidget *parent = nullptr) : QDialog(parent) {
        setWindowTitle("Play Location");
        setFixedSize(300, 120);
        setStyleSheet("background-color: #2b2b3d; color: #00ff00;");

        QVBoxLayout *layout = new QVBoxLayout(this);
        QLabel *label = new QLabel("Enter a URL to play:", this);
        urlLineEdit = new QLineEdit(this);
        urlLineEdit->setStyleSheet("background-color: #000; color: #00FF00; border: 1px solid #555;");

        QHBoxLayout *buttonLayout = new QHBoxLayout();
        QPushButton *okButton = new QPushButton("Open", this);
        QPushButton *cancelButton = new QPushButton("Cancel", this);
        
        connect(okButton, &QPushButton::clicked, this, &QDialog::accept);
        connect(cancelButton, &QPushButton::clicked, this, &QDialog::reject);

        buttonLayout->addStretch();
        buttonLayout->addWidget(okButton);
        buttonLayout->addWidget(cancelButton);

        layout->addWidget(label);
        layout->addWidget(urlLineEdit);
        layout->addLayout(buttonLayout);
        setLayout(layout);
    }

    QString getUrl() const {
        return urlLineEdit->text();
    }

private:
    QLineEdit *urlLineEdit;
};

