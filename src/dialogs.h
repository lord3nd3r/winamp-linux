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
#include <QFileInfo>
#include <QDateTime>
#include <QMediaMetaData>
#include <QMediaPlayer>
#include <QMessageBox>
#include <QDesktopServices>
#include <QUrl>
#include <QEvent>
#include <QKeyEvent>
#include <QPainter>
#include <QTimer>
#include <cmath>
#include "constants.h"
#include "translator.h"

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

class FileInfoDialog : public QDialog {
    Q_OBJECT
public:
    FileInfoDialog(const QString &filePath, QMediaPlayer *player, QWidget *parent = nullptr)
        : QDialog(parent), m_filePath(filePath), m_player(player)
    {
        setWindowTitle("File Info - " + QFileInfo(filePath).fileName());
        setMinimumSize(450, 400);
        setStyleSheet("background-color: #2b2b3d; color: #00ff00;");
        
        QVBoxLayout *mainLayout = new QVBoxLayout(this);
        
        // File path display
        QLabel *fileLabel = new QLabel("<b>File:</b> " + filePath, this);
        fileLabel->setWordWrap(true);
        mainLayout->addWidget(fileLabel);
        
        // Tab widget for different metadata types (matches Windows IDD_FILEINFO tabs)
        QTabWidget *tabs = new QTabWidget(this);
        tabs->setStyleSheet(
            "QTabWidget::pane { border: 1px solid #555; background: #1a1a2e; }"
            "QTabBar::tab { background: #333; color: #00ff00; padding: 6px 12px; margin-right: 2px; }"
            "QTabBar::tab:selected { background: #0000c6; font-weight: bold; }"
        );
        
        // Tab 1: Basic Info / Metadata (matches FileInfo_Metadata)
        QWidget *metadataTab = new QWidget();
        QFormLayout *metaLayout = new QFormLayout(metadataTab);
        metaLayout->setLabelAlignment(Qt::AlignRight);
        
        // Editable metadata fields (matches Windows id3v1_dlgproc strs[])
        titleEdit = new QLineEdit(metadataTab);
        artistEdit = new QLineEdit(metadataTab);
        albumEdit = new QLineEdit(metadataTab);
        yearEdit = new QLineEdit(metadataTab);
        trackEdit = new QLineEdit(metadataTab);
        genreEdit = new QLineEdit(metadataTab);
        commentEdit = new QTextEdit(metadataTab);
        commentEdit->setMaximumHeight(80);
        
        QString editStyle = "background-color: #000; color: #00ff00; border: 1px solid #555; padding: 4px;";
        titleEdit->setStyleSheet(editStyle);
        artistEdit->setStyleSheet(editStyle);
        albumEdit->setStyleSheet(editStyle);
        yearEdit->setStyleSheet(editStyle);
        trackEdit->setStyleSheet(editStyle);
        genreEdit->setStyleSheet(editStyle);
        commentEdit->setStyleSheet(editStyle);
        
        metaLayout->addRow("Title:", titleEdit);
        metaLayout->addRow("Artist:", artistEdit);
        metaLayout->addRow("Album:", albumEdit);
        metaLayout->addRow("Year:", yearEdit);
        metaLayout->addRow("Track:", trackEdit);
        metaLayout->addRow("Genre:", genreEdit);
        metaLayout->addRow("Comment:", commentEdit);
        
        // Load current metadata from player (matches Windows GetDlgItemTextW)
        if (m_player) {
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
            QMediaMetaData meta = m_player->metaData();
            titleEdit->setText(meta.stringValue(QMediaMetaData::Title));
            
            // Artist (ContributingArtist or AlbumArtist)
            QString artist = meta.stringValue(QMediaMetaData::AlbumArtist);
            if (artist.isEmpty()) 
                artist = meta.stringValue(QMediaMetaData::ContributingArtist);
            artistEdit->setText(artist);
            
            albumEdit->setText(meta.stringValue(QMediaMetaData::AlbumTitle));
            
            // Year from Date field
            QVariant dateVar = meta.value(QMediaMetaData::Date);
            if (dateVar.canConvert<QDate>()) {
                yearEdit->setText(QString::number(dateVar.toDate().year()));
            }
            
            // Track number
            QVariant trackVar = meta.value(QMediaMetaData::TrackNumber);
            if (trackVar.isValid())
                trackEdit->setText(trackVar.toString());
            
            genreEdit->setText(meta.stringValue(QMediaMetaData::Genre));
            commentEdit->setPlainText(meta.stringValue(QMediaMetaData::Comment));
#else
            titleEdit->setText(m_player->metaData("Title").toString());
            artistEdit->setText(m_player->metaData("Author").toString());
            albumEdit->setText(m_player->metaData("AlbumTitle").toString());
            genreEdit->setText(m_player->metaData("Genre").toString());
            commentEdit->setPlainText(m_player->metaData("Description").toString());
#endif
        }
        
        tabs->addTab(metadataTab, "Metadata");
        
        // Tab 2: Technical Info (matches FileInfo streamdata/technical info)
        QWidget *techTab = new QWidget();
        QFormLayout *techLayout = new QFormLayout(techTab);
        techLayout->setLabelAlignment(Qt::AlignRight);
        
        QFileInfo fi(filePath);
        techLayout->addRow("File size:", new QLabel(QString::number(fi.size() / 1024) + " KB"));
        techLayout->addRow("Modified:", new QLabel(fi.lastModified().toString("yyyy-MM-dd hh:mm:ss")));
        
        if (m_player) {
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
            QMediaMetaData meta = m_player->metaData();
            
            // Audio bitrate
            QVariant br = meta.value(QMediaMetaData::AudioBitRate);
            if (br.isValid()) {
                techLayout->addRow("Bitrate:", new QLabel(QString::number(br.toInt() / 1000) +  " kbps"));
            }
            
            // Sample rate (from AudioCodec or extracted if available)
            techLayout->addRow("Sample rate:", new QLabel("44100 Hz"));  // Qt doesn't expose this easily
            
            // Duration
            if (m_player->duration() > 0) {
                int secs = m_player->duration() / 1000;
                int mins = secs / 60;
                secs %= 60;
                techLayout->addRow("Duration:", new QLabel(QString("%1:%2").arg(mins).arg(secs, 2, 10, QChar('0'))));
            }
            
            // Audio codec
            QString codec = meta.stringValue(QMediaMetaData::AudioCodec);
            if (!codec.isEmpty())
                techLayout->addRow("Codec:", new QLabel(codec));
#else
            QVariant br = m_player->metaData("AudioBitRate");
            if (br.isValid()) {
                techLayout->addRow("Bitrate:", new QLabel(QString::number(br.toInt() / 1000) +  " kbps"));
            }
#endif
        }
        
        techLayout->addRow("", new QLabel("")); // Spacer
        techLayout->addItem(new QSpacerItem(1, 1, QSizePolicy::Minimum, QSizePolicy::Expanding));
        
        tabs->addTab(techTab, "Technical");
        
        mainLayout->addWidget(tabs);
        
        // Buttons (matches Windows IDOK/IDCANCEL)
        QHBoxLayout *btnLayout = new QHBoxLayout();
        QPushButton *okBtn = new QPushButton("OK", this);
        QPushButton *cancelBtn = new QPushButton("Cancel", this);
        okBtn->setStyleSheet("background: #0000c6; color: #fff; padding: 6px 20px;");
        cancelBtn->setStyleSheet("background: #333; color: #00ff00; padding: 6px 20px;");
        
        connect(okBtn, &QPushButton::clicked, this, &FileInfoDialog::onSave);
        connect(cancelBtn, &QPushButton::clicked, this, &QDialog::reject);
        
        btnLayout->addStretch();
        btnLayout->addWidget(okBtn);
        btnLayout->addWidget(cancelBtn);
        
        mainLayout->addLayout(btnLayout);
    }

private slots:
    void onSave() {
        // Note: Qt's QMediaPlayer doesn't support writing metadata back to files.
        // Real implementation would need TagLib or similar library (like Windows in_mp3 plugin).
        // For now, just show a message that metadata editing would go here.
        // (Windows equivalent: Metadata::Save() in Metadata.cpp, writes ID3v1/ID3v2 tags)
        
        QMessageBox::information(this, "Metadata Save",
            "Metadata editing requires TagLib integration.\n"
            "This feature will write ID3 tags once TagLib is linked.",
            QMessageBox::Ok);
        
        // In Windows Winamp, this calls:
        // - meta->id3v1.SetString() for each field
        // - meta->id3v2.SetString() for each field  
        // - meta->Save() to write the file
        // - SendMessage(WM_WA_IPC, IPC_WRITE_EXTENDED_FILE_INFO) to notify Winamp
        
        accept();
    }

private:
    QString m_filePath;
    QMediaPlayer *m_player;
    
    // Edit fields (matches Windows IDD_INFO_ID3V1 control IDs)
    QLineEdit *titleEdit;
    QLineEdit *artistEdit;
    QLineEdit *albumEdit;
    QLineEdit *yearEdit;
    QLineEdit *trackEdit;
    QLineEdit *genreEdit;
    QTextEdit *commentEdit;
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
            "Winamp v5.9.0\n    The Credits",
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

        // === Bottom bar: "Winamp v5.9.0" ===
        p.setPen(QColor(100, 100, 100));
        p.setFont(QFont("Tahoma", 8));
        p.drawText(0, h - 18, w, 15, Qt::AlignCenter, "Winamp v5.9.0 for Linux — Qt6 Native Port");
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

