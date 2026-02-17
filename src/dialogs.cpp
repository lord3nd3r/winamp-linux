#include "dialogs.h"
#include "translator.h"
#include "bookmarkmanager.h"
#include "skinutils.h"

#include <QCoreApplication>
#include <QFile>
#include <QTextStream>
#include <QSizePolicy>
#include <QSpacerItem>
#include <QSet>
#include <cmath>
#include <cstdlib>

// ============================================================
// JumpToFileDialog
// ============================================================

JumpToFileDialog::JumpToFileDialog(const QStringList &tracks, QWidget *parent)
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

void JumpToFileDialog::filterList(const QString &text)
{
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

void JumpToFileDialog::onItemSelected(QListWidgetItem *)
{
    if (resultList->currentRow() >= 0 && resultList->currentRow() < filteredIndices.size())
        selectedIndex = filteredIndices[resultList->currentRow()];
    accept();
}

// ============================================================
// FileInfoDialog
// ============================================================

FileInfoDialog::FileInfoDialog(const QString &filePath, QMediaPlayer *player, QWidget *parent)
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
        QMediaMetaData meta = m_player->metaData();

        // Audio bitrate
        QVariant br = meta.value(QMediaMetaData::AudioBitRate);
        if (br.isValid()) {
            techLayout->addRow("Bitrate:", new QLabel(QString::number(br.toInt() / 1000) + " kbps"));
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

void FileInfoDialog::onSave()
{
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

// ============================================================
// AboutDialog
// ============================================================

AboutDialog::AboutDialog(const QString &skinPath, QWidget *parent) : QDialog(parent)
{
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
    credits = {
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

void AboutDialog::paintEvent(QPaintEvent *)
{
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

void AboutDialog::keyPressEvent(QKeyEvent *e)
{
    if (e->key() == Qt::Key_Escape || e->key() == Qt::Key_Return)
        accept();
}

void AboutDialog::mousePressEvent(QMouseEvent *)
{
    accept();
}

void AboutDialog::tick()
{
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

// ============================================================
// PlayLocationDialog
// ============================================================

PlayLocationDialog::PlayLocationDialog(QWidget *parent) : QDialog(parent)
{
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

QString PlayLocationDialog::getUrl() const
{
    return urlLineEdit->text();
}

// ============================================================
// PreferencesDialog
// ============================================================

PreferencesDialog::PreferencesDialog(QWidget *parent) : QDialog(parent)
{
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
    addPage(setupItem, "Titles", createTitlesPage());
    addPage(setupItem, "Language", createLanguagePage());

    // Skins category
    QTreeWidgetItem *skinsItem = addPage(nullptr, "Skins", createSkinsPage());
    addPage(skinsItem, "Classic Skins", createClassicSkinsPage());
    addPage(skinsItem, "Modern Skins", createModernSkinsPage());

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

QWidget *PreferencesDialog::createGeneralPage()
{
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

QWidget *PreferencesDialog::createFileTypesPage()
{
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

QWidget *PreferencesDialog::createTitlesPage()
{
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

QWidget *PreferencesDialog::createLanguagePage()
{
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
    langNames["es"] = QString::fromUtf8("Español (Spanish)");
    langNames["fr"] = QString::fromUtf8("Français (French)");
    langNames["pt"] = QString::fromUtf8("Português (Portuguese)");
    langNames["ru"] = QString::fromUtf8("Русский (Russian)");
    langNames["ja"] = QString::fromUtf8("日本語 (Japanese)");
    langNames["zh"] = QString::fromUtf8("中文 (Chinese)");

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

QWidget *PreferencesDialog::createSkinsPage()
{
    QWidget *page = new QWidget();
    QVBoxLayout *layout = new QVBoxLayout(page);
    layout->addWidget(new QLabel("<b>Skins</b>"));
    layout->addSpacing(10);
    layout->addWidget(new QLabel("Select a skin category on the left.\n\n"
                                 "Classic skins (.wsz) use bitmap sprite sheets.\n"
                                 "Modern skins (.wal) use XML-based layouts."));
    layout->addStretch();
    return page;
}

QWidget *PreferencesDialog::createClassicSkinsPage()
{
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

QWidget *PreferencesDialog::createModernSkinsPage()
{
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

QWidget *PreferencesDialog::createPlaybackPage()
{
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

QWidget *PreferencesDialog::createPlaylistPrefsPage()
{
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

QWidget *PreferencesDialog::createBookmarksPage()
{
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

QWidget *PreferencesDialog::createVisualizationPage()
{
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

QWidget *PreferencesDialog::createPluginsPage()
{
    QWidget *page = new QWidget();
    QVBoxLayout *layout = new QVBoxLayout(page);
    layout->addWidget(new QLabel("<b>Plug-ins</b>"));
    layout->addSpacing(10);
    layout->addWidget(new QLabel("Plug-in architecture is not yet available on Linux.\n\n"
                                 "Currently using:\n"
                                 "  • Qt6 Multimedia for audio decoding & output\n"
                                 "  • projectM for Milkdrop visualization\n\n"
                                 "Future support planned for:\n"
                                 "  • Input plug-ins (in_*.so)\n"
                                 "  • Output plug-ins (out_*.so)\n"
                                 "  • DSP/Effect plug-ins (dsp_*.so)\n"
                                 "  • General purpose plug-ins (gen_*.so)\n"
                                 "  • Visualization plug-ins (vis_*.so)"));
    layout->addStretch();
    return page;
}

void PreferencesDialog::populateSkins()
{
    if (!skinListWidget) return;
    // Find the built-in default skin path
    QString appDir = QCoreApplication::applicationDirPath();
    QStringList defaultCandidates = {
        appDir + "/../skins/default",
        appDir + "/../../skins/default",
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

void PreferencesDialog::populateModernSkins()
{
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

void PreferencesDialog::onModernSkinSelected(QListWidgetItem *item)
{
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

void PreferencesDialog::onSkinSelected(QListWidgetItem *item)
{
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
