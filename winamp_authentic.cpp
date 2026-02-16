#include <QApplication>
#include <QWidget>
#include <QPainter>
#include <QMediaPlayer>
#include <QMediaMetaData>
#include <QAudioOutput>
#include <QAudioBufferOutput>
#include <QAudioBuffer>
#include <QFileDialog>
#include <QMouseEvent>
#include <QTimer>
#include <QRandomGenerator>
#include <QListWidget>
#include <QVBoxLayout>
#include <QPixmap>
#include <QImage>
#include <QIcon>
#include <QFile>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QMimeData>
#include <QSettings>
#include <QDir>
#include <QMenu>
#include <QAction>
#include <QCloseEvent>
#include <algorithm>
#include <random>
#include <cmath>
#include <cstring>
#include <QInputDialog>
#include <QTabWidget>
#include <QLabel>
#include <QPushButton>
#include <QLineEdit>
#include <QDebug>
#include <QFileInfo>
#include <QDateTime>
#include <QProcess>
#include <type_traits>

// Extract a .wsz or .zip skin archive to a cache directory.
// Returns the path to the extracted folder, or empty string on failure.
static QString extractSkinArchive(const QString &archivePath) {
    QFileInfo fi(archivePath);
    if (!fi.exists()) return {};
    
    // Cache dir: ~/.cache/winamp/skins/<basename_without_ext>
    QString cacheDirBase = QDir::homePath() + "/.cache/winamp/skins";
    QString skinName = fi.completeBaseName();
    QString extractDir = cacheDirBase + "/" + skinName;
    QDir().mkpath(extractDir);
    
    // Check if already extracted (any .bmp file exists)
    QDir ed(extractDir);
    QStringList bmps = ed.entryList(QStringList() << "*.bmp" << "*.BMP", QDir::Files);
    if (!bmps.isEmpty()) {
        return extractDir;
    }
    
    // Extract using unzip
    QProcess proc;
    proc.setWorkingDirectory(extractDir);
    proc.start("unzip", QStringList() << "-o" << "-j" << archivePath << "-d" << extractDir);
    proc.waitForFinished(10000);
    
    if (proc.exitCode() != 0) {
        qWarning() << "Failed to extract skin archive:" << archivePath << proc.readAllStandardError();
        return {};
    }
    
    return extractDir;
}

// ============================================================
// Simple FFT for spectrum analyzer (radix-2 DIT, 512-point)
// ============================================================
static void fft512(const float *input, float *magnitudes) {
    const int N = 512;
    // Bit-reversal permutation
    float re[N], im[N];
    for (int i = 0; i < N; i++) {
        int j = 0;
        for (int b = 0; b < 9; b++)
            j |= ((i >> b) & 1) << (8 - b);
        re[j] = input[i];
        im[j] = 0.0f;
    }
    // Cooley-Tukey FFT
    for (int s = 1; s <= 9; s++) {
        int m = 1 << s;
        int m2 = m >> 1;
        float wRe = cosf(-2.0f * M_PI / m);
        float wIm = sinf(-2.0f * M_PI / m);
        for (int k = 0; k < N; k += m) {
            float tRe = 1.0f, tIm = 0.0f;
            for (int j = 0; j < m2; j++) {
                float uRe = re[k + j], uIm = im[k + j];
                float vRe = tRe * re[k + j + m2] - tIm * im[k + j + m2];
                float vIm = tRe * im[k + j + m2] + tIm * re[k + j + m2];
                re[k + j] = uRe + vRe;
                im[k + j] = uIm + vIm;
                re[k + j + m2] = uRe - vRe;
                im[k + j + m2] = uIm - vIm;
                float newTRe = tRe * wRe - tIm * wIm;
                tIm = tRe * wIm + tIm * wRe;
                tRe = newTRe;
            }
        }
    }
    // Output magnitudes for first 256 bins
    for (int i = 0; i < N / 2; i++) {
        magnitudes[i] = sqrtf(re[i] * re[i] + im[i] * im[i]);
    }
}

// Winamp default visualization colors (24 entries from draw.cpp ppal2[])
static const QColor visColors[24] = {
    QColor(0, 0, 0),         // 0: background
    QColor(24, 33, 41),      // 1: grey dots
    QColor(239, 49, 16),     // 2: spectrum top (brightest)
    QColor(206, 41, 16),     // 3
    QColor(214, 90, 0),      // 4
    QColor(214, 102, 0),     // 5
    QColor(214, 115, 0),     // 6
    QColor(198, 123, 8),     // 7
    QColor(222, 165, 24),    // 8
    QColor(214, 181, 33),    // 9
    QColor(189, 222, 41),    // 10
    QColor(148, 222, 33),    // 11
    QColor(41, 206, 16),     // 12
    QColor(50, 190, 16),     // 13
    QColor(57, 181, 16),     // 14
    QColor(49, 156, 8),      // 15
    QColor(41, 148, 0),      // 16
    QColor(24, 132, 8),      // 17: spectrum bottom (dimmest)
    QColor(255, 255, 255),   // 18: oscilloscope brightest
    QColor(214, 214, 222),   // 19
    QColor(181, 189, 189),   // 20
    QColor(160, 170, 175),   // 21
    QColor(148, 156, 165),   // 22
    QColor(150, 150, 150),   // 23: analyzer peak dot
};

// Forward declaration
class WinampWindow;

// Shared text.bmp character lookup (5x6 per char)
static QPoint getTextCharPos(QChar ch) {
    if (ch >= 'A' && ch <= 'Z') return QPoint((ch.toLatin1() - 'A') * 5, 0);
    if (ch >= 'a' && ch <= 'z') return QPoint((ch.toLatin1() - 'a') * 5, 0);
    if (ch >= '0' && ch <= '9') return QPoint((ch.toLatin1() - '0') * 5, 6);
    switch (ch.toLatin1()) {
        case ' ': return QPoint(142, 0);
        case ':': return QPoint(60, 6);
        case '.': return QPoint(55, 6);
        case '\'': case '`': return QPoint(80, 6);
        case '(': return QPoint(65, 6);
        case ')': return QPoint(70, 6);
        case '-': return QPoint(75, 6);
        case '!': return QPoint(85, 6);
        case '_': return QPoint(90, 6);
        case '+': return QPoint(95, 6);
        case '\\': return QPoint(100, 6);
        case '/': return QPoint(105, 6);
        case '[': case '{': case '<': return QPoint(110, 6);
        case ']': case '}': case '>': return QPoint(115, 6);
        case '~': case '^': return QPoint(120, 6);
        case '&': return QPoint(125, 6);
        case '%': return QPoint(130, 6);
        case ',': return QPoint(135, 6);
        case '=': return QPoint(140, 6);
        case '$': return QPoint(145, 6);
        case '#': return QPoint(150, 6);
        case '"': return QPoint(130, 0);
        case '@': return QPoint(135, 0);
        case '?': return QPoint(15, 12);
        case '*': return QPoint(20, 12);
    }
    return QPoint(-1, -1);
}

// About Dialog — Demoscene-style animated credits (faithful to Windows original)
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

// Play Location Dialog
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

// Preferences Dialog
class PreferencesDialog : public QDialog {
    Q_OBJECT
public:
    PreferencesDialog(QWidget *parent = nullptr) : QDialog(parent) {
        setWindowTitle("Winamp Preferences");
        setMinimumSize(400, 300);
        setStyleSheet("background-color: #2b2b3d; color: #00ff00;");

        QVBoxLayout *mainLayout = new QVBoxLayout(this);
        QTabWidget *tabWidget = new QTabWidget(this);
        tabWidget->setStyleSheet("QTabWidget::pane { border: 1px solid #555; }");

        // General Preferences Tab (Placeholder)
        QWidget *generalTab = new QWidget();
        QVBoxLayout *generalLayout = new QVBoxLayout(generalTab);
        generalLayout->addWidget(new QLabel("General settings will go here."));
        generalLayout->addStretch();
        tabWidget->addTab(generalTab, "General");

        // Skins Tab
        QWidget *skinsTab = new QWidget();
        QVBoxLayout *skinsLayout = new QVBoxLayout(skinsTab);
        skinListWidget = new QListWidget(this);
        skinListWidget->setStyleSheet("QListWidget { background-color: #000; color: #00FF00; }");
        populateSkins();
        connect(skinListWidget, &QListWidget::itemDoubleClicked, this, &PreferencesDialog::onSkinSelected);
        skinsLayout->addWidget(skinListWidget);
        skinsTab->setLayout(skinsLayout);
        tabWidget->addTab(skinsTab, "Skins");

        mainLayout->addWidget(tabWidget);
        setLayout(mainLayout);
    }

signals:
    void skinChanged(const QString &skinPath);

private:
    QString defaultSkinPath;
    
    void populateSkins() {
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

    QListWidget *skinListWidget;
};


// Bitmap Resource Manager
class WinampBitmaps {
public:
    static WinampBitmaps& instance() {
        static WinampBitmaps inst;
        return inst;
    }
    
    bool loadAll(const QString &resourcePath) {
        basePath = resourcePath;
        
        // Helper: try loading a pixmap with case-insensitive fallback
        auto loadBmp = [&](const QString &name) -> QPixmap {
            QPixmap pm(basePath + "/" + name);
            if (!pm.isNull()) return pm;
            // Try uppercase/lowercase variants
            pm = QPixmap(basePath + "/" + name.toUpper());
            if (!pm.isNull()) return pm;
            pm = QPixmap(basePath + "/" + name.toLower());
            return pm;
        };
        
        // Load all classic Winamp bitmaps
        main = loadBmp("MAIN.BMP");
        cbuttons = loadBmp("CBUTTONS.BMP");
        titlebar = loadBmp("titlebar.bmp");
        numbers = loadBmp("numbers.bmp");
        text = loadBmp("text.bmp");
        playpaus = loadBmp("PLAYPAUS.BMP");
        monoster = loadBmp("MONOSTER.BMP");
        posbar = loadBmp("POSBAR.BMP");
        volume = loadBmp("volume.bmp");
        shufrep = loadBmp("SHUFREP.BMP");
        eqmain = loadBmp("Eqmain.bmp");
        pledit = loadBmp("Pledit.bmp");
        
        return !main.isNull();
    }
    
    // Load any missing bitmaps from an additional fallback path
    void loadMissing(const QString &fallbackPath) {
        auto tryLoad = [&](QPixmap &pm, const QString &name) {
            if (pm.isNull()) {
                pm = QPixmap(fallbackPath + "/" + name);
                if (pm.isNull()) pm = QPixmap(fallbackPath + "/" + name.toUpper());
                if (pm.isNull()) pm = QPixmap(fallbackPath + "/" + name.toLower());
            }
        };
        tryLoad(main, "MAIN.BMP");
        tryLoad(cbuttons, "CBUTTONS.BMP");
        tryLoad(titlebar, "titlebar.bmp");
        tryLoad(numbers, "numbers.bmp");
        tryLoad(text, "text.bmp");
        tryLoad(playpaus, "PLAYPAUS.BMP");
        tryLoad(monoster, "MONOSTER.BMP");
        tryLoad(posbar, "POSBAR.BMP");
        tryLoad(volume, "volume.bmp");
        tryLoad(shufrep, "SHUFREP.BMP");
        tryLoad(eqmain, "Eqmain.bmp");
        tryLoad(pledit, "Pledit.bmp");
    }
    
    QPixmap main, cbuttons, titlebar, numbers, text;
    QPixmap playpaus, monoster, posbar, volume, shufrep;
    QPixmap eqmain, pledit;
    QString basePath;
    
private:
    WinampBitmaps() {}
};

// Config file path helper
static QString configPath() {
    QString dir = QDir::homePath() + "/.config/winamp";
    QDir().mkpath(dir);
    return dir + "/winamp.conf";
}

// Built-in EQ Presets (slider values 0-63, center=32)
struct EqPreset {
    const char *name;
    int values[10];
};

static const EqPreset builtinPresets[] = {
    {"Flat",                {32, 32, 32, 32, 32, 32, 32, 32, 32, 32}},
    {"Classical",           {32, 32, 32, 32, 32, 32, 44, 44, 44, 48}},
    {"Club",                {32, 32, 25, 21, 21, 21, 25, 32, 32, 32}},
    {"Dance",               {14, 18, 27, 32, 32, 40, 43, 43, 32, 32}},
    {"Full Bass",           {14, 14, 14, 21, 29, 37, 44, 48, 50, 50}},
    {"Full Bass & Treble",  {18, 21, 32, 43, 37, 28, 16, 12,  8,  6}},
    {"Full Treble",         {48, 48, 48, 37, 27, 12,  4,  4,  4,  2}},
    {"Laptop Speakers",     {25, 12, 23, 36, 33, 29, 25, 15,  8,  4}},
    {"Large Hall",          {14, 14, 21, 21, 32, 37, 37, 37, 32, 32}},
    {"Live",                {37, 32, 25, 21, 21, 21, 25, 28, 28, 29}},
    {"Party",               {18, 18, 32, 32, 32, 32, 32, 32, 18, 18}},
    {"Pop",                 {33, 25, 18, 16, 21, 32, 33, 33, 33, 33}},
    {"Reggae",              {32, 32, 32, 40, 32, 22, 22, 32, 32, 32}},
    {"Rock",                {18, 25, 39, 43, 35, 26, 16, 12, 12, 12}},
    {"Ska",                 {34, 37, 37, 32, 26, 23, 16, 15, 12, 15}},
    {"Soft",                {25, 29, 32, 34, 32, 25, 16, 14, 12, 10}},
    {"Soft Rock",           {25, 25, 28, 32, 37, 35, 29, 25, 22, 16}},
    {"Techno",              {18, 21, 32, 39, 37, 32, 18, 15, 15, 16}},
};
static const int numPresets = sizeof(builtinPresets) / sizeof(builtinPresets[0]);

// Playlist Window
class PlaylistWindow : public QWidget {
    Q_OBJECT
public:
    PlaylistWindow(WinampWindow *parent = nullptr);
    void addTrack(const QString &filePath);
    void clearPlaylist();
    bool isSnapped() const { return isSnappedToMain; }
    void followMain();
    void checkSnap();
    void saveSettings(QSettings &s);
    void loadSettings(QSettings &s);

    // Track navigation accessors
    int trackCount() const { return tracks.size(); }
    QString trackAt(int index) const { return (index >= 0 && index < tracks.size()) ? tracks[index] : QString(); }
    int currentTrackIndex() const { return listWidget->currentRow(); }
    void setCurrentTrackIndex(int index) { if (index >= 0 && index < tracks.size()) listWidget->setCurrentRow(index); }

signals:
    void trackDoubleClicked(const QString &filePath);

protected:
    void paintEvent(QPaintEvent *) override;
    void mousePressEvent(QMouseEvent *) override;
    void mouseMoveEvent(QMouseEvent *) override;
    void mouseReleaseEvent(QMouseEvent *) override;
    void dragEnterEvent(QDragEnterEvent *) override;
    void dropEvent(QDropEvent *) override;

private:
    void updateTotalTimeDisplay();
    void drawText(QPainter &painter, const QString &text, int x, int y);
    QPoint getTextCharPos(QChar ch);
    void showAddMenu(QPoint globalPos);
    void showRemMenu(QPoint globalPos);
    void showSelMenu(QPoint globalPos);
    void showMiscMenu(QPoint globalPos);
    void showListMenu(QPoint globalPos);

    QListWidget *listWidget;
    QList<QString> tracks;
    QList<qint64> trackDurations; // Store durations in milliseconds
    QString totalTimeStr; // Formatted string for display
    QPoint dragPosition;
    bool isDragging = false;
    bool isSnappedToMain = false;
    WinampWindow *mainWindow = nullptr;
    int snapMode = 0;  // 0=none, 1=right of main, 2=below EQ, 3=below main
};

// Equalizer Window
class EqualizerWindow : public QWidget {
public:
    EqualizerWindow(WinampWindow *parent = nullptr);
    
    void setMainWindow(WinampWindow *main) { mainWindow = main; }
    
    void followMain();
    void checkSnap();
    
    void showPresetsMenu(QPoint globalPos) {
        QMenu menu;
        menu.setStyleSheet(
            "QMenu { background-color: #2b2d3d; color: #00ff00; border: 1px solid #555; font-size: 9pt; }"
            "QMenu::item:selected { background-color: #0000c6; }"
        );
        for (int i = 0; i < numPresets; i++) {
            QAction *action = menu.addAction(builtinPresets[i].name);
            action->setData(i);
        }
        QAction *selected = menu.exec(globalPos);
        if (selected) {
            int idx = selected->data().toInt();
            for (int i = 0; i < 10; i++) {
                eqValues[i] = builtinPresets[idx].values[i];
            }
            update();
        }
    }
    
    void saveSettings(QSettings &s) {
        s.beginGroup("Equalizer");
        s.setValue("x", x());
        s.setValue("y", y());
        s.setValue("visible", isVisible());
        s.setValue("enabled", eqEnabled);
        s.setValue("auto", autoEnabled);
        s.setValue("preamp", preampValue);
        for (int i = 0; i < 10; i++) {
            s.setValue(QString("band%1").arg(i), eqValues[i]);
        }
        s.setValue("snapped", isSnappedToMain);
        s.endGroup();
    }
    
    void loadSettings(QSettings &s) {
        s.beginGroup("Equalizer");
        if (s.contains("x")) {
            move(s.value("x").toInt(), s.value("y").toInt());
        }
        eqEnabled = s.value("enabled", true).toBool();
        autoEnabled = s.value("auto", false).toBool();
        preampValue = s.value("preamp", 32).toInt();
        for (int i = 0; i < 10; i++) {
            eqValues[i] = s.value(QString("band%1").arg(i), 32).toInt();
        }
        isSnappedToMain = s.value("snapped", false).toBool();
        s.endGroup();
        update();
    }
    
protected:
    void paintEvent(QPaintEvent *) override {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing, false);
        
        auto &bmp = WinampBitmaps::instance();
        if (bmp.eqmain.isNull()) {
            p.fillRect(rect(), QColor(66, 66, 99));
            p.setPen(QColor(0, 255, 0));
            p.setFont(QFont("Tahoma", 7, QFont::Bold));
            p.drawText(6, 10, "Winamp Equalizer");
            return;
        }
        
        // Draw base EQ background from rows 0-115 of Eqmain.bmp
        // This contains the full EQ skin graphic (gradients, labels, borders)
        p.drawPixmap(0, 0, bmp.eqmain, 0, 0, 275, 116);
        
        // Overlay titlebar: active at (0,134), inactive at (0,149), 275x14
        int tbY = isActiveWindow() ? 134 : 149;
        p.drawPixmap(0, 0, bmp.eqmain, 0, tbY, 275, 14);
        
        // ON button: dest(14,18), 25x12
        // States: OFF=(10,119), ON=(69,119), OFF pressed=(128,119), ON pressed=(187,119)
        int onSrcX = eqEnabled ? 69 : 10;
        p.drawPixmap(14, 18, bmp.eqmain, onSrcX, 119, 25, 12);
        
        // AUTO button: dest(39,18), 33x12
        int autoSrcX = autoEnabled ? 94 : 35;
        p.drawPixmap(39, 18, bmp.eqmain, autoSrcX, 119, 33, 12);
        
        // Presets button: dest(217,18), 44x12
        p.drawPixmap(217, 18, bmp.eqmain, 224, 164, 44, 12);
        
        // EQ graph background: dest(86,17), src(0,294), 113x19
        p.drawPixmap(86, 17, bmp.eqmain, 0, 294, 113, 19);
        
        // Draw slider grooves and thumbs
        // Preamp at x=21, bands at x=78+n*18
        drawEqSlider(p, 0, 21);  // Preamp
        for (int i = 0; i < 10; i++) {
            drawEqSlider(p, i + 1, 78 + i * 18);
        }
    }
    
    void drawEqSlider(QPainter &p, int which, int destX) {
        auto &bmp = WinampBitmaps::instance();
        int pos = (which == 0) ? preampValue : eqValues[which - 1];
        
        // Groove background: 28 images (14 per row)
        // n = (pos * 28) / 64, clamped to 0-27
        int n = (pos * 27) / 63;
        if (n > 27) n = 27;
        if (n < 0) n = 0;
        
        int grooveSrcX, grooveSrcY;
        if (n < 14) {
            grooveSrcX = 13 + n * 15;
            grooveSrcY = 164;
        } else {
            grooveSrcX = 13 + (n - 14) * 15;
            grooveSrcY = 229;
        }
        p.drawPixmap(destX, 38, bmp.eqmain, grooveSrcX, grooveSrcY, 14, 63);
        
        // Slider thumb (knob): 11x11 at src(0,164) unpressed
        int thumbY = 38 + 63 - 12 - ((63 - pos) * 52) / 64;
        p.drawPixmap(destX + 1, thumbY, bmp.eqmain, 0, 164, 11, 11);
    }
    
    void mousePressEvent(QMouseEvent *event) override {
        int x = event->pos().x();
        int y = event->pos().y();
        
        // Title bar
        if (y < 14) {
            if (x >= 264) { hide(); return; }
            isDragging = true;
            dragPosition = event->globalPosition().toPoint() - frameGeometry().topLeft();
            return;
        }
        
        // ON button: (14,18)-(39,30)
        if (x >= 14 && x < 39 && y >= 18 && y < 30) {
            eqEnabled = !eqEnabled;
            update();
            return;
        }
        
        // AUTO button: (39,18)-(72,30)
        if (x >= 39 && x < 72 && y >= 18 && y < 30) {
            autoEnabled = !autoEnabled;
            update();
            return;
        }
        
        // Presets button: (217,18)-(261,30)
        if (x >= 217 && x < 261 && y >= 18 && y < 30) {
            showPresetsMenu(mapToGlobal(QPoint(217, 30)));
            return;
        }
        
        // Slider dragging
        // Preamp: x=21..34, bands: x=78+n*18..78+n*18+14
        if (y >= 38 && y <= 101) {
            if (x >= 21 && x <= 34) {
                draggingSlider = 0;
                updateSliderFromY(y);
                return;
            }
            for (int i = 0; i < 10; i++) {
                int sx = 78 + i * 18;
                if (x >= sx && x <= sx + 14) {
                    draggingSlider = i + 1;
                    updateSliderFromY(y);
                    return;
                }
            }
        }
    }
    
    void updateSliderFromY(int y) {
        int pos = 63 - ((y - 38) * 63) / 52;
        if (pos < 0) pos = 0;
        if (pos > 63) pos = 63;
        if (draggingSlider == 0) preampValue = pos;
        else eqValues[draggingSlider - 1] = pos;
        update();
    }
    
    void mouseMoveEvent(QMouseEvent *event) override {
        if (draggingSlider >= 0) {
            updateSliderFromY(event->pos().y());
            return;
        }
        if (isDragging) {
            QPoint newPos = event->globalPosition().toPoint() - dragPosition;
            move(newPos);
            checkSnap();
        }
    }
    
    void mouseReleaseEvent(QMouseEvent *event) override {
        isDragging = false;
        draggingSlider = -1;
    }
    
    bool isSnapped() const { return isSnappedToMain; }
    
private:
    int eqValues[10];
    int preampValue;
    bool eqEnabled = true;
    bool autoEnabled = false;
    int draggingSlider = -1;
    QPoint dragPosition;
    bool isDragging = false;
    WinampWindow *mainWindow = nullptr;
    bool isSnappedToMain = false;
};

// Playlist Window Constructor
PlaylistWindow::PlaylistWindow(WinampWindow *parent) : QWidget(nullptr), mainWindow(parent) {
    setFixedSize(275, 232);
    setWindowTitle("Winamp Playlist Editor");
    setWindowFlags(Qt::Tool | Qt::FramelessWindowHint);
    
    // Position list widget within the skin frame
    // Titlebar=20px, left border=12px, right border=20px (incl scrollbar), bottom=38px
    listWidget = new QListWidget(this);
    listWidget->setGeometry(12, 20, 275 - 12 - 20, 232 - 20 - 38);
    listWidget->setStyleSheet(
        "QListWidget {"
        "  background-color: #000000;"
        "  color: #00FF00;"
        "  border: none;"
        "  font-family: 'Courier New', 'Courier';"
        "  font-size: 8pt;"
        "  selection-background-color: #0000C6;"
        "  selection-color: #00FF00;"
        "}"
        "QListWidget::item {"
        "  padding: 0px;"
        "}"
    );
    listWidget->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    listWidget->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    
    // Enable drag and drop for files
    listWidget->setAcceptDrops(true);
    listWidget->setDragEnabled(true);
    listWidget->setDropIndicatorShown(true);
    listWidget->setDragDropMode(QAbstractItemView::InternalMove);
    
    // Connect signals for drag and drop
    connect(listWidget, &QListWidget::itemDoubleClicked, this, [this](QListWidgetItem *item) {
        if (item) {
            int index = listWidget->row(item);
            emit trackDoubleClicked(tracks[index]);
        }
    });

    connect(listWidget->model(), &QAbstractItemModel::rowsMoved, this, [this](const QModelIndex &parent, int start, int end, const QModelIndex &destination, int row) {
        // Rearrange tracks and durations lists to match the new order in the list widget
        for (int i = 0; i <= end - start; ++i) {
            tracks.move(start, row + i);
            trackDurations.move(start, row + i);
        }
    });
}

void PlaylistWindow::updateTotalTimeDisplay() {
    qint64 totalMilliseconds = 0;
    for (qint64 duration : trackDurations) {
        totalMilliseconds += duration;
    }

    qint64 totalSeconds = totalMilliseconds / 1000;
    int hours = totalSeconds / 3600;
    int minutes = (totalSeconds % 3600) / 60;
    int seconds = totalSeconds % 60;

    if (hours > 0) {
        totalTimeStr = QString("%1 tracks, %2h %3m %4s").arg(tracks.size()).arg(hours).arg(minutes, 2, 10, QChar('0')).arg(seconds, 2, 10, QChar('0'));
    } else {
        totalTimeStr = QString("%1 tracks, %2m %3s").arg(tracks.size()).arg(minutes).arg(seconds, 2, 10, QChar('0'));
    }
    
    // Request a repaint to show the new time
    update();
}

void PlaylistWindow::addTrack(const QString &filePath) {
    QFileInfo fileInfo(filePath);
    if (fileInfo.exists()) {
        listWidget->addItem(fileInfo.fileName());
        tracks.append(filePath);

        // Use a temporary media player to get the duration
        QMediaPlayer tempPlayer;
        tempPlayer.setSource(QUrl::fromLocalFile(filePath));
        
        // We need to wait for it to load the media to get duration
        QEventLoop loop;
        QObject::connect(&tempPlayer, &QMediaPlayer::mediaStatusChanged, [&](QMediaPlayer::MediaStatus status){
            if(status == QMediaPlayer::LoadedMedia) {
                trackDurations.append(tempPlayer.duration());
                updateTotalTimeDisplay();
                loop.quit();
            } else if (status == QMediaPlayer::InvalidMedia) {
                trackDurations.append(0); // Add 0 if media is invalid
                updateTotalTimeDisplay();
                loop.quit();
            }
        });
        loop.exec();
    }
}

void PlaylistWindow::clearPlaylist() {
    listWidget->clear();
    tracks.clear();
    trackDurations.clear();
    updateTotalTimeDisplay();
}

void PlaylistWindow::paintEvent(QPaintEvent *event) {
    Q_UNUSED(event);
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, false);
    
    auto &bmp = WinampBitmaps::instance();
    int w = width();
    int h = height();
    
    if (bmp.pledit.isNull()) {
        painter.fillRect(rect(), QColor(0, 0, 0));
        painter.setPen(QColor(0, 255, 0));
        painter.setFont(QFont("Tahoma", 7, QFont::Bold));
        painter.drawText(6, 10, "Winamp Playlist Editor");
        return;
    }
    
    // === Compose playlist skin from Pledit.bmp sprite pieces ===
    
    // --- Titlebar (20px tall) ---
    bool active = isActiveWindow();
    int tbRow = active ? 0 : 21;  // active at y=0, inactive at y=21
    // Left corner 25x20
    painter.drawPixmap(0, 0, bmp.pledit, 0, tbRow, 25, 20);
    // Title text area 100x20 (centered)
    int titleMid = (w - 100) / 2;
    painter.drawPixmap(titleMid, 0, bmp.pledit, 26, tbRow, 100, 20);
    // Fill between left corner and title with filler tile
    for (int fx = 25; fx < titleMid; fx += 25)
        painter.drawPixmap(fx, 0, bmp.pledit, 127, tbRow, qMin(25, titleMid - fx), 20);
    // Fill between title and right corner with filler tile  
    for (int fx = titleMid + 100; fx < w - 25; fx += 25)
        painter.drawPixmap(fx, 0, bmp.pledit, 127, tbRow, qMin(25, w - 25 - fx), 20);
    // Right corner 25x20
    painter.drawPixmap(w - 25, 0, bmp.pledit, 153, tbRow, 25, 20);
    
    // --- Side borders (between titlebar and bottom bar) ---
    int bodyTop = 20;
    int bodyBottom = h - 38;  // bottom bar is 38px
    // Left border: 12px wide, tile 29px tall from (0,42)
    for (int fy = bodyTop; fy < bodyBottom; fy += 29)
        painter.drawPixmap(0, fy, bmp.pledit, 0, 42, 12, qMin(29, bodyBottom - fy));
    // Right border: 5+7=12px wide, from (31,42) and (44,42)
    for (int fy = bodyTop; fy < bodyBottom; fy += 29) {
        int tileH = qMin(29, bodyBottom - fy);
        painter.drawPixmap(w - 20, fy, bmp.pledit, 31, 42, 5, tileH);   // left part of right border
        painter.drawPixmap(w - 15, fy, bmp.pledit, 36, 42, 8, tileH);   // scrollbar track area
        painter.drawPixmap(w - 7, fy, bmp.pledit, 44, 42, 7, tileH);    // right part
    }
    
    // --- Body fill (black background for track list area) ---
    painter.fillRect(12, bodyTop, w - 12 - 20, bodyBottom - bodyTop, QColor(0, 0, 0));
    
    // --- Bottom bar (38px tall) ---
    // Bottom left 125x38 from (0,72)
    painter.drawPixmap(0, bodyBottom, bmp.pledit, 0, 72, 125, 38);
    // Bottom right 150x38 from (126,72)
    painter.drawPixmap(w - 150, bodyBottom, bmp.pledit, 126, 72, 150, 38);
    // Fill middle with filler tile 25x38 from (179,0)
    for (int fx = 125; fx < w - 150; fx += 25)
        painter.drawPixmap(fx, bodyBottom, bmp.pledit, 179, 0, qMin(25, w - 150 - fx), 38);
    
    // --- Total time text at bottom ---
    drawText(painter, totalTimeStr.toUpper(), w - 143, h - 28);
}

void PlaylistWindow::drawText(QPainter &painter, const QString &text, int x, int y) {
    int currentX = x;
    for (QChar ch : text) {
        QPoint pos = getTextCharPos(ch);
        if (pos.x() != -1) {
            painter.drawPixmap(currentX, y, WinampBitmaps::instance().text, pos.x(), pos.y(), 5, 6);
        }
        currentX += 5;
    }
}

QPoint PlaylistWindow::getTextCharPos(QChar ch) {
    return ::getTextCharPos(ch);
}

void PlaylistWindow::mousePressEvent(QMouseEvent *event) {
    int x = event->pos().x();
    int y = event->pos().y();
    int h = height();

    // Playlist bottom buttons — respond to both left and right click
    if (y >= h - 30 && y < h - 12) {
        if (x >= 14 && x < 35) {
            showAddMenu(event->globalPosition().toPoint());
            event->accept();
            return;
        } else if (x >= 43 && x < 64) {
            showRemMenu(event->globalPosition().toPoint());
            event->accept();
            return;
        } else if (x >= 82 && x < 103) {
            showSelMenu(event->globalPosition().toPoint());
            event->accept();
            return;
        } else if (x >= 121 && x < 142) {
            showMiscMenu(event->globalPosition().toPoint());
            event->accept();
            return;
        } else if (x >= width() - 44 && x < width() - 44 + 22) {
            showListMenu(event->globalPosition().toPoint());
            event->accept();
            return;
        }
    }

    if (event->button() == Qt::LeftButton) {
        // Close button: (w-11, 3) 9x9
        if (x >= width() - 11 && x < width() - 2 && y >= 3 && y < 12) {
            hide();
            event->accept();
            return;
        }
        isDragging = true;
        dragPosition = event->globalPosition().toPoint() - frameGeometry().topLeft();
        event->accept();
    }
}

void PlaylistWindow::dropEvent(QDropEvent *event) {
    const QMimeData *mimeData = event->mimeData();
    if (mimeData->hasUrls()) {
        for (const QUrl &url : mimeData->urls()) {
            QString filePath = url.toLocalFile();
            if (!filePath.isEmpty()) {
                addTrack(filePath);
            }
        }
        event->acceptProposedAction();
    }
}

void PlaylistWindow::mouseMoveEvent(QMouseEvent *event) {
    if (isDragging) {
        move(event->globalPosition().toPoint() - dragPosition);
        checkSnap();
    }
}

void PlaylistWindow::mouseReleaseEvent(QMouseEvent *event) {
    Q_UNUSED(event);
    isDragging = false;
}

void PlaylistWindow::dragEnterEvent(QDragEnterEvent *event) {
    if (event->mimeData()->hasUrls())
        event->acceptProposedAction();
}

void PlaylistWindow::saveSettings(QSettings &s) {
    s.beginGroup("Playlist");
    s.setValue("x", x());
    s.setValue("y", y());
    s.setValue("visible", isVisible());
    s.setValue("snapMode", snapMode);
    s.setValue("width", width());
    s.setValue("height", height());
    // Save track list as a proper string list
    s.setValue("trackList", QVariant(tracks));
    s.endGroup();
}

void PlaylistWindow::loadSettings(QSettings &s) {
    s.beginGroup("Playlist");
    if (s.contains("x")) {
        move(s.value("x").toInt(), s.value("y").toInt());
    }
    snapMode = s.value("snapMode", 0).toInt();
    isSnappedToMain = (snapMode != 0);

    // Restore tracks — try new format first, then legacy comma-separated
    QStringList savedTracks;
    if (s.contains("trackList")) {
        savedTracks = s.value("trackList").toStringList();
    } else if (s.contains("tracks")) {
        // Legacy format: comma-separated
        QString raw = s.value("tracks").toString();
        savedTracks = raw.split(", ", Qt::SkipEmptyParts);
    }
    s.endGroup();

    // Add each saved track back into the playlist
    for (const QString &track : savedTracks) {
        QString trimmed = track.trimmed();
        if (!trimmed.isEmpty() && QFile::exists(trimmed)) {
            QFileInfo fi(trimmed);
            listWidget->addItem(fi.fileName());
            tracks.append(trimmed);
            trackDurations.append(0); // Duration will be 0 until played
        }
    }
    updateTotalTimeDisplay();
}

void PlaylistWindow::showAddMenu(QPoint globalPos) {
    QMenu menu;
    menu.setStyleSheet(
        "QMenu { background-color: #2b2d3d; color: #00ff00; border: 1px solid #555; font-size: 9pt; }"
        "QMenu::item:selected { background-color: #0000c6; }"
    );
    
    QAction *addFiles = menu.addAction("Add file(s)");
    QAction *addDir = menu.addAction("Add directory");
    QAction *addUrl = menu.addAction("Add location");
    
    QAction *selected = menu.exec(globalPos);
    if (selected == addFiles) {
        QStringList files = QFileDialog::getOpenFileNames(this, "Add Files", QString(), 
            "Audio Files (*.mp3 *.wav *.flac *.ogg *.m4a *.aac);;All Files (*)");
        for (const QString &file : files) {
            if (!file.isEmpty()) {
                addTrack(file);
            }
        }
    } else if (selected == addDir) {
        QString dir = QFileDialog::getExistingDirectory(this, "Add Directory");
        if (!dir.isEmpty()) {
            QDir directory(dir);
            QStringList filters = {"*.mp3", "*.wav", "*.flac", "*.ogg", "*.m4a", "*.aac"};
            QStringList files = directory.entryList(filters, QDir::Files);
            for (const QString &file : files) {
                addTrack(directory.absoluteFilePath(file));
            }
        }
    } else if (selected == addUrl) {
        // For now, just show a placeholder
        // In real Winamp this would open a URL dialog
    }
}

void PlaylistWindow::showRemMenu(QPoint globalPos) {
    QMenu menu;
    menu.setStyleSheet(
        "QMenu { background-color: #2b2d3d; color: #00ff00; border: 1px solid #555; font-size: 9pt; }"
        "QMenu::item:selected { background-color: #0000c6; }"
    );
    
    QAction *removeSel = menu.addAction("Remove selected");
    QAction *crop = menu.addAction("Crop");
    QAction *clear = menu.addAction("Clear playlist");
    QAction *removeMisc = menu.addAction("Remove misc");
    
    QAction *selected = menu.exec(globalPos);
    if (selected == removeSel) {
        // Remove selected items
        QList<QListWidgetItem*> selectedItems = listWidget->selectedItems();
        // Iterate backwards to avoid index shifting issues
        for (int i = selectedItems.size() - 1; i >= 0; --i) {
            QListWidgetItem *item = selectedItems[i];
            int row = listWidget->row(item);
            if (row >= 0 && row < tracks.size()) {
                tracks.removeAt(row);
                trackDurations.removeAt(row);
            }
            delete item;
        }
        updateTotalTimeDisplay();
    } else if (selected == crop) {
        // Remove all except selected
        QList<QListWidgetItem*> selectedItems = listWidget->selectedItems();
        QStringList newTracks;
        QList<qint64> newDurations;
        
        for (QListWidgetItem *item : selectedItems) {
            int row = listWidget->row(item);
            if (row >= 0 && row < tracks.size()) {
                newTracks.append(tracks[row]);
                newDurations.append(trackDurations[row]);
            }
        }
        
        listWidget->clear();
        tracks = newTracks;
        trackDurations = newDurations;

        for (const QString &track : tracks) {
            listWidget->addItem(QFileInfo(track).fileName());
        }
        updateTotalTimeDisplay();

    } else if (selected == clear) {
        clearPlaylist();
    } else if (selected == removeMisc) {
        // "Remove dead files" and "Remove duplicates" would go here
    }
}

void PlaylistWindow::showSelMenu(QPoint globalPos) {
    QMenu menu;
    menu.setStyleSheet(
        "QMenu { background-color: #2b2d3d; color: #00ff00; border: 1px solid #555; font-size: 9pt; }"
        "QMenu::item:selected { background-color: #0000c6; }"
    );
    
    QAction *selectAll = menu.addAction("Select all");
    QAction *selectNone = menu.addAction("Select none");
    QAction *invertSel = menu.addAction("Invert selection");
    
    QAction *selected = menu.exec(globalPos);
    if (selected == selectAll) {
        listWidget->selectAll();
    } else if (selected == selectNone) {
        listWidget->clearSelection();
    } else if (selected == invertSel) {
        for (int i = 0; i < listWidget->count(); i++) {
            QListWidgetItem *item = listWidget->item(i);
            item->setSelected(!item->isSelected());
        }
    }
}

void PlaylistWindow::showMiscMenu(QPoint globalPos) {
    QMenu menu;
    menu.setStyleSheet(
        "QMenu { background-color: #2b2d3d; color: #00ff00; border: 1px solid #555; font-size: 9pt; }"
        "QMenu::item:selected { background-color: #0000c6; }"
    );
    
    QAction *sortByTitle = menu.addAction("Sort by title");
    QAction *sortByFilename = menu.addAction("Sort by filename");
    QAction *sortByPath = menu.addAction("Sort by path");
    QAction *reverseList = menu.addAction("Reverse list");
    QAction *randomizeList = menu.addAction("Randomize list");
    
    QAction *selected = menu.exec(globalPos);
    if (selected == sortByTitle) {
        listWidget->sortItems(Qt::AscendingOrder);
    } else if (selected == sortByFilename) {
        // Since we only store filenames, this is the same as sort by title for now
        listWidget->sortItems(Qt::AscendingOrder);
    } else if (selected == sortByPath) {
        // To sort by path, we need to rebuild the list widget based on the sorted `tracks` list
        std::sort(tracks.begin(), tracks.end());
        listWidget->clear();
        trackDurations.clear(); // This will be incorrect until we re-fetch them
        for (const QString &track : tracks) {
            addTrack(track); // Re-adding will fetch duration but is slow
        }
    } else if (selected == reverseList) {
        // Reverse the list
        std::reverse(tracks.begin(), tracks.end());
        std::reverse(trackDurations.begin(), trackDurations.end());
        // Rebuild the list widget
        listWidget->clear();
        for (const QString &track : tracks) {
            QFileInfo fi(track);
            listWidget->addItem(fi.fileName());
        }
        updateTotalTimeDisplay();
    } else if (selected == randomizeList) {
        // We need to shuffle tracks and durations together
        QList<QPair<QString, qint64>> combined;
        for(int i = 0; i < tracks.size(); ++i) {
            combined.append(qMakePair(tracks[i], trackDurations[i]));
        }

        std::random_device rd;
        std::mt19937 g(rd());
        std::shuffle(combined.begin(), combined.end(), g);

        tracks.clear();
        trackDurations.clear();
        listWidget->clear();

        for(const auto& pair : combined) {
            tracks.append(pair.first);
            trackDurations.append(pair.second);
            listWidget->addItem(QFileInfo(pair.first).fileName());
        }
        updateTotalTimeDisplay();
    }
}

void PlaylistWindow::showListMenu(QPoint globalPos) {
    QMenu menu;
    menu.setStyleSheet(
        "QMenu { background-color: #2b2d3d; color: #00ff00; border: 1px solid #555; font-size: 9pt; }"
        "QMenu::item:selected { background-color: #0000c6; }"
    );

    QAction *newPl = menu.addAction("New playlist");
    QAction *openPl = menu.addAction("Open playlist...");
    QAction *savePl = menu.addAction("Save playlist...");

    QAction *selected = menu.exec(globalPos);
    if (selected == newPl) {
        clearPlaylist();
    } else if (selected == openPl) {
        QString fileName = QFileDialog::getOpenFileName(this, "Open Playlist", "",
            "Playlist Files (*.m3u *.m3u8 *.pls);;All Files (*)");
        if (!fileName.isEmpty()) {
            clearPlaylist();
            QFile file(fileName);
            if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
                QTextStream in(&file);
                QString basePath = QFileInfo(fileName).absolutePath();
                while (!in.atEnd()) {
                    QString line = in.readLine().trimmed();
                    if (line.isEmpty() || line.startsWith('#'))
                        continue;
                    // Handle relative paths
                    if (!QFileInfo(line).isAbsolute())
                        line = basePath + "/" + line;
                    if (QFile::exists(line))
                        addTrack(line);
                }
                file.close();
            }
        }
    } else if (selected == savePl) {
        QString fileName = QFileDialog::getSaveFileName(this, "Save Playlist", "",
            "M3U Playlist (*.m3u);;M3U8 Playlist (*.m3u8);;All Files (*)");
        if (!fileName.isEmpty()) {
            QFile file(fileName);
            if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
                QTextStream out(&file);
                out << "#EXTM3U\n";
                for (int i = 0; i < tracks.size(); i++) {
                    qint64 durSec = (i < trackDurations.size()) ? trackDurations[i] / 1000 : -1;
                    QString title = QFileInfo(tracks[i]).baseName();
                    out << "#EXTINF:" << durSec << "," << title << "\n";
                    out << tracks[i] << "\n";
                }
                file.close();
            }
        }
    }
}

// Equalizer Window Constructor
EqualizerWindow::EqualizerWindow(WinampWindow *parent) : QWidget(nullptr), mainWindow(parent) {
    setFixedSize(275, 116);
    setWindowTitle("Winamp Equalizer");
    setWindowFlags(Qt::Tool | Qt::FramelessWindowHint);
    
    // Initialize EQ bands to center position (32 out of 63)
    for (int i = 0; i < 10; i++) {
        eqValues[i] = 32;
    }
    preampValue = 32;
}

// Main Winamp Window
class WinampWindow : public QWidget {
    Q_OBJECT
public:
    WinampWindow(QWidget *parent = nullptr) : QWidget(parent), dragPosition(0,0), isDragging(false), 
                 volume(200), hoveredButton(-1), pressedButton(-1),
                 shuffleOn(false), repeatOn(false), eqBtnOn(false), plBtnOn(false),
                 isDraggingVolume(false), isDraggingPos(false), scrollOffset(0),
                 visMode(1) {
        setFixedSize(275, 116);
        setWindowTitle("Winamp 5.666 for Linux");
        setWindowFlags(Qt::FramelessWindowHint);
        setAttribute(Qt::WA_TranslucentBackground, false);
        setMouseTracking(true);
        
        // Initialize visualization state
        memset(saBarHeight, 0, sizeof(saBarHeight));
        memset(saPeakHeight, 0, sizeof(saPeakHeight));
        memset(saPeakVel, 0, sizeof(saPeakVel));
        memset(spectrumData, 0, sizeof(spectrumData));
        memset(oscData, 0, sizeof(oscData));
        
        // Setup audio
        player = new QMediaPlayer(this);
        audioOutput = new QAudioOutput(this);
        player->setAudioOutput(audioOutput);
        audioOutput->setVolume(volume / 255.0f);
        
        // Setup audio buffer output for visualization
        audioBufferOutput = new QAudioBufferOutput(this);
        player->setAudioBufferOutput(audioBufferOutput);
        connect(audioBufferOutput, &QAudioBufferOutput::audioBufferReceived,
                this, &WinampWindow::processAudioBuffer);
        
        // Update timer (50ms = 20fps like original)
        timer = new QTimer(this);
        connect(timer, &QTimer::timeout, this, &WinampWindow::updateDisplay);
        timer->start(50);
        
        // Scroll timer for song title
        scrollTimer = new QTimer(this);
        connect(scrollTimer, &QTimer::timeout, this, [this]() {
            scrollOffset++;
            update();
        });
        scrollTimer->start(150);
        
        connect(player, &QMediaPlayer::positionChanged, this, [this](qint64) { update(); });
        
        // Auto-advance to next track when current one ends
        connect(player, &QMediaPlayer::mediaStatusChanged, this, [this](QMediaPlayer::MediaStatus status) {
            if (status == QMediaPlayer::EndOfMedia) {
                int curIdx = playlistWindow->currentTrackIndex();
                int count = playlistWindow->trackCount();
                if (count > 0) {
                    int nextIdx;
                    if (shuffleOn) {
                        nextIdx = QRandomGenerator::global()->bounded(count);
                    } else {
                        nextIdx = curIdx + 1;
                    }
                    if (nextIdx < count) {
                        playlistWindow->setCurrentTrackIndex(nextIdx);
                        playTrack(playlistWindow->trackAt(nextIdx));
                    } else if (repeatOn) {
                        playlistWindow->setCurrentTrackIndex(0);
                        playTrack(playlistWindow->trackAt(0));
                    }
                }
            }
        });
        
        // Extract bitrate from metadata when available
        connect(player, &QMediaPlayer::metaDataChanged, this, [this]() {
            QVariant br = player->metaData().value(QMediaMetaData::AudioBitRate);
            if (br.isValid()) {
                mediaBitrate = br.toInt() / 1000;  // bps -> kbps
            }
        });
        
        // Create playlist and EQ windows
        playlistWindow = new PlaylistWindow(this);
        connect(playlistWindow, &PlaylistWindow::trackDoubleClicked, this, &WinampWindow::playTrack);
        eqWindow = new EqualizerWindow(this);
        
        // Position windows: EQ below main, playlist to the right of main
        playlistWindow->move(x() + width(), y());  // right of main
        eqWindow->move(x(), y() + height());
        
        // Load saved settings (overrides defaults above)
        loadAllSettings();
    }
    
    ~WinampWindow() {
        delete playlistWindow;
        delete eqWindow;
    }

    void playFile(const QString &file) {
        if (!file.isEmpty() && QFile::exists(file)) {
            currentFile = file;
            player->setSource(QUrl::fromLocalFile(file));
            player->play();
        }
    }

    void playUrl(const QString &url) {
        if (!url.isEmpty()) {
            currentFile = url;
            player->setSource(QUrl(url));
            player->play();
        }
    }

public slots:
    void onPlayFile() {
        QString file = QFileDialog::getOpenFileName(this, "Open File", QString(), 
            "Audio Files (*.mp3 *.wav *.flac *.ogg *.m4a *.aac);;All Files (*)");
        if (!file.isEmpty()) {
            playFile(file);
        }
    }

    void onPlayLocation() {
        PlayLocationDialog dialog(this);
        if (dialog.exec() == QDialog::Accepted) {
            QString url = dialog.getUrl();
            if (!url.isEmpty()) {
                playUrl(url);
            }
        }
    }

    void onToggleAlwaysOnTop(bool checked) {
        setWindowFlag(Qt::WindowStaysOnTopHint, checked);
        show(); // Re-show to apply the flag change
    }

    void onShowAbout() {
        AboutDialog aboutDialog(WinampBitmaps::instance().basePath, this);
        aboutDialog.exec();
    }

    void onSkinChanged(const QString &skinPath) {
        WinampBitmaps::instance().loadAll(skinPath);
        
        // Load any missing bitmaps from fallback paths
        QString appDir = QCoreApplication::applicationDirPath();
        QStringList fallbacks = {
            appDir + "/../skins/default",
            appDir + "/../../skins/default",
            QDir::homePath() + "/.winamp/skins/default",
            appDir + "/../Src/Winamp/resource",
            appDir + "/../../Src/Winamp/resource"
        };
        for (const QString &fb : fallbacks) {
            QDir d(fb);
            if (d.exists())
                WinampBitmaps::instance().loadMissing(d.absolutePath());
        }
        
        // Force all windows to repaint with the new skin
        update();
        playlistWindow->update();
        eqWindow->update();

        // Save the new skin setting
        QSettings s(configPath(), QSettings::IniFormat);
        s.setValue("skin", skinPath);
    }

    void processAudioBuffer(const QAudioBuffer &buffer) {
        const QAudioFormat fmt = buffer.format();
        int sampleCount = buffer.frameCount();
        int channels = fmt.channelCount();
        
        // Update media info from audio format
        mediaChannels = channels;
        int sr = fmt.sampleRate();
        if (sr > 0) mediaSampleRate = sr / 1000; // e.g. 44100 -> 44

        auto extractData = [&](auto *data) {
            float scale = 1.0f;
            if constexpr (std::is_same_v<std::remove_const_t<decltype(*data)>, qint16>)
                scale = 1.0f / 32768.0f;

            for (int i = 0; i < 75 && i < sampleCount; i++)
                oscData[i] = data[i * channels] * scale;

            float fftInput[512];
            memset(fftInput, 0, sizeof(fftInput));
            int n = qMin(sampleCount, 512);
            for (int i = 0; i < n; i++)
                fftInput[i] = data[i * channels] * scale;

            float magnitudes[256];
            fft512(fftInput, magnitudes);

            for (int i = 0; i < 19; i++) {
                int startBin = i * 8 + 1;
                int endBin = qMin(startBin + 8, 256);
                float maxVal = 0;
                for (int j = startBin; j < endBin; j++)
                    if (magnitudes[j] > maxVal) maxVal = magnitudes[j];
                // Logarithmic scaling — matches the way real Winamp feels
                // Raw magnitudes can be 0..~100+ for loud audio
                // Convert to dB-like scale: log10(1 + val * boost) / log10(1 + boost)
                float db = 0;
                if (maxVal > 0.001f) {
                    db = log10f(1.0f + maxVal * 5.0f) / log10f(1.0f + 5.0f * 50.0f);
                }
                spectrumData[i] = qBound(0.0f, db, 1.0f);
            }
        };

        if (fmt.sampleFormat() == QAudioFormat::Int16)
            extractData(buffer.constData<qint16>());
        else if (fmt.sampleFormat() == QAudioFormat::Float)
            extractData(buffer.constData<float>());
    }

protected:
    void paintEvent(QPaintEvent *) override {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing, false);

        auto &bmp = WinampBitmaps::instance();

        // Main background
        if (!bmp.main.isNull())
            p.drawPixmap(0, 0, bmp.main);
        else {
            p.fillRect(rect(), QColor(66, 66, 99));
            p.setPen(QColor(0, 255, 0));
            p.setFont(QFont("Tahoma", 7, QFont::Bold));
            p.drawText(10, 14, "Winamp 5.666 for Linux");
            return;
        }

        // Titlebar: starts at x=27 in titlebar.bmp, active at y=0, inactive at y=15, 275x14
        if (!bmp.titlebar.isNull()) {
            int tbY = isActiveWindow() ? 0 : 15;
            p.drawPixmap(0, 0, bmp.titlebar, 27, tbY, 275, 14);
        }

        // Play/pause status indicator at (26,28), each 9x9
        if (!bmp.playpaus.isNull()) {
            int srcX = 27; // stopped/not playing
            if (player->playbackState() == QMediaPlayer::PlayingState) srcX = 0;
            else if (player->playbackState() == QMediaPlayer::PausedState) srcX = 9;
            p.drawPixmap(26, 28, bmp.playpaus, srcX, 0, 9, 9);
        }

        // Time display — digits are 9x13 in numbers.bmp
        // Positions: mins_tens(36,26), mins_ones(48,26), secs_tens(78,26), secs_ones(90,26)
        // The colon is baked into MAIN.BMP background — no colon glyph in numbers.bmp
        if (!bmp.numbers.isNull()) {
            qint64 pos = player->position();
            int sec = pos / 1000;
            int mins = sec / 60;
            sec %= 60;
            auto drawDigit = [&](int dx, int d) {
                int srcX = (d >= 0 && d <= 9) ? d * 9 : 90; // 90 = blank
                p.drawPixmap(dx, 26, bmp.numbers, srcX, 0, 9, 13);
            };
            drawDigit(36, (mins / 10) % 10);
            drawDigit(48, mins % 10);
            drawDigit(78, sec / 10);
            drawDigit(90, sec % 10);
        }

        // Scrolling song title in text area (111,27) ~154x6
        if (!bmp.text.isNull() && !currentFile.isEmpty()) {
            QFileInfo fi(currentFile);
            QString title = fi.baseName().toUpper();
            QString scrollText = title + "  ***  " + title;
            int charW = 5;
            int totalW = (title.length() + 7) * charW;
            if (totalW < 1) totalW = 1;

            p.save();
            p.setClipRect(111, 27, 154, 6);
            int textX = 111 - (scrollOffset % totalW);
            for (QChar ch : scrollText) {
                QPoint cp = ::getTextCharPos(ch);
                if (cp.x() >= 0)
                    p.drawPixmap(textX, 27, bmp.text, cp.x(), cp.y(), 5, 6);
                textX += charW;
            }
            p.restore();
        }

        // kbps display at (111, 43) — 3 chars using text.bmp 5x6 font
        // khz display at (156, 43) — 2 chars using text.bmp 5x6 font
        if (!bmp.text.isNull() && player->playbackState() != QMediaPlayer::StoppedState) {
            auto drawSmallChar = [&](int dx, int dy, QChar ch) {
                QPoint cp = ::getTextCharPos(ch);
                if (cp.x() >= 0)
                    p.drawPixmap(dx, dy, bmp.text, cp.x(), cp.y(), 5, 6);
            };
            // kbps (3 digits, right-aligned)
            if (mediaBitrate > 0) {
                QString kbStr = QString::number(mediaBitrate).rightJustified(3, ' ');
                for (int i = 0; i < 3; i++)
                    drawSmallChar(111 + i * 5, 43, kbStr[i]);
            }
            // khz (2 digits)
            if (mediaSampleRate > 0) {
                QString khStr = QString::number(mediaSampleRate).rightJustified(2, ' ');
                for (int i = 0; i < 2; i++)
                    drawSmallChar(156 + i * 5, 43, khStr[i]);
            }
        }

        // Mono/Stereo indicator at (212,41) — each state 29x12
        if (!bmp.monoster.isNull()) {
            // stereo on: (0,0), stereo off: (0,12), mono on: (29,0), mono off: (29,12)
            bool isStereo = (mediaChannels >= 2);
            bool isMono = (mediaChannels == 1);
            bool playing = (player->playbackState() != QMediaPlayer::StoppedState);
            p.drawPixmap(212, 41, bmp.monoster, 0, (playing && isStereo) ? 0 : 12, 29, 12);
            p.drawPixmap(239, 41, bmp.monoster, 29, (playing && isMono) ? 0 : 12, 27, 12);
        }

        // Transport buttons from CBUTTONS.BMP — each 23x18, pressed row at y+18
        if (!bmp.cbuttons.isNull()) {
            auto drawBtn = [&](int id, int dx, int sx, int w, int h) {
                int sy = (pressedButton == id) ? h : 0;
                p.drawPixmap(dx, 88, bmp.cbuttons, sx, sy, w, h);
            };
            drawBtn(0, 16,  0,  23, 18);  // Previous
            drawBtn(1, 39,  23, 23, 18);  // Play
            drawBtn(2, 62,  46, 23, 18);  // Pause
            drawBtn(3, 85,  69, 23, 18);  // Stop
            drawBtn(4, 108, 92, 22, 18);  // Next
            // Eject: 22x16
            int ejY = (pressedButton == 5) ? 16 : 0;
            p.drawPixmap(136, 89, bmp.cbuttons, 114, ejY, 22, 16);
        }

        // Volume slider — 28 frames, each 68x13, spaced 15px apart in volume.bmp
        if (!bmp.volume.isNull()) {
            int frame = qBound(0, (volume * 27) / 255, 27);
            p.drawPixmap(107, 57, bmp.volume, 0, frame * 15, 68, 13);
            // Volume thumb: 14x11 at (0,422) normal, (15,422) pressed
            int thumbX = 107 + (volume * 51) / 255; // 68-14=54 range, but visually ~51
            int thumbSrcX = isDraggingVolume ? 15 : 0;
            p.drawPixmap(thumbX, 58, bmp.volume, thumbSrcX, 422, 14, 11);
        }

        // Position bar
        if (!bmp.posbar.isNull()) {
            p.drawPixmap(16, 72, bmp.posbar, 0, 0, 248, 10);
            if (player->duration() > 0) {
                int thumbX = 16 + (int)((player->position() * 219LL) / player->duration());
                int thumbSrcX = isDraggingPos ? 278 : 248;
                p.drawPixmap(thumbX, 72, bmp.posbar, thumbSrcX, 0, 29, 10);
            }
        }

        // Shuffle/Repeat/EQ/PL from SHUFREP.BMP
        if (!bmp.shufrep.isNull()) {
            p.drawPixmap(164, 89, bmp.shufrep, 28, shuffleOn ? 15 : 0, 47, 15);
            p.drawPixmap(210, 89, bmp.shufrep, 0, repeatOn ? 15 : 0, 28, 15);
            p.drawPixmap(219, 58, bmp.shufrep, 0, eqBtnOn ? 73 : 61, 23, 12);
            p.drawPixmap(242, 58, bmp.shufrep, 23, plBtnOn ? 73 : 61, 23, 12);
        }

        // Visualization area: (24,43) to (99,59) — 75x16
        if (visMode == 1) drawSpectrumAnalyzer(p);
        else if (visMode == 2) drawOscilloscope(p);
    }

    void drawSpectrumAnalyzer(QPainter &p) {
        const int visX = 24, visY = 43, visH = 16;
        // Fill background
        p.fillRect(visX, visY, 75, visH, visColors[0]);
        // 19 bars, each 3px wide, 1px gap
        for (int i = 0; i < 19; i++) {
            float val = spectrumData[i]; // 0.0 - 1.0 (log-scaled)
            int target = (int)(val * 16.0f);
            if (target > 15) target = 15;
            // Smooth falloff
            if (target > saBarHeight[i]) saBarHeight[i] = target;
            else if (saBarHeight[i] > 0) saBarHeight[i]--;
            int h = saBarHeight[i];
            // Draw bar with color gradient
            for (int j = 0; j < h; j++) {
                int colorIdx = 17 - (j * 15 / 15);
                if (colorIdx < 2) colorIdx = 2;
                if (colorIdx > 17) colorIdx = 17;
                int py = visY + visH - 1 - j;
                p.fillRect(visX + i * 4, py, 3, 1, visColors[colorIdx]);
            }
            // Peak dot
            if (h > saPeakHeight[i]) {
                saPeakHeight[i] = h;
                saPeakVel[i] = 0;
            } else {
                saPeakVel[i] += 0.1f;
                saPeakHeight[i] -= (int)saPeakVel[i];
                if (saPeakHeight[i] < 0) saPeakHeight[i] = 0;
            }
            if (saPeakHeight[i] > 0) {
                int peakY = visY + visH - 1 - saPeakHeight[i];
                p.fillRect(visX + i * 4, peakY, 3, 1, visColors[23]);
            }
        }
    }

    void drawOscilloscope(QPainter &p) {
        const int visX = 24, visY = 43, visH = 16;
        p.fillRect(visX, visY, 75, visH, visColors[0]);
        p.setPen(visColors[18]);
        int prevY = visY + visH / 2;
        for (int i = 0; i < 75; i++) {
            int cy = visY + visH / 2 - (int)(oscData[i] * (visH / 2));
            if (cy < visY) cy = visY;
            if (cy >= visY + visH) cy = visY + visH - 1;
            p.drawLine(visX + i, prevY, visX + i, cy);
            prevY = cy;
        }
    }

    void showContextMenu(QPoint globalPos) {
        QMenu menu;
        menu.setStyleSheet(
            "QMenu { background-color: #2b2d3d; color: #00ff00; border: 1px solid #555; font-size: 9pt; }"
            "QMenu::item:selected { background-color: #0000c6; }"
        );
        QAction *playFileAct = menu.addAction("Play file...");
        QAction *playLocAct = menu.addAction("Play location...");
        menu.addSeparator();
        QAction *prefsAct = menu.addAction("Preferences...");
        QAction *aboutAct = menu.addAction("About Winamp...");
        menu.addSeparator();
        QAction *quitAct = menu.addAction("Exit");

        QAction *sel = menu.exec(globalPos);
        if (sel == playFileAct) onPlayFile();
        else if (sel == playLocAct) onPlayLocation();
        else if (sel == prefsAct) {
            PreferencesDialog *prefs = new PreferencesDialog(this);
            connect(prefs, &PreferencesDialog::skinChanged, this, &WinampWindow::onSkinChanged);
            prefs->setAttribute(Qt::WA_DeleteOnClose);
            prefs->exec();
        }
        else if (sel == aboutAct) onShowAbout();
        else if (sel == quitAct) close();
    }

    void mousePressEvent(QMouseEvent *event) override {
        if (event->button() == Qt::RightButton) {
            showContextMenu(event->globalPosition().toPoint());
            return;
        }
        int x = event->pos().x();
        int y = event->pos().y();
        
        // Visualization area click: (27,40)-(99,61) — cycle modes 0->1->2->0
        // Matches Windows Ui.cpp inreg(27,40,99,61)
        if (x >= 27 && x < 99 && y >= 40 && y < 61) {
            visMode++;
            if (visMode > 2) visMode = 0;
            // Reset viz state when switching modes
            memset(saBarHeight, 0, sizeof(saBarHeight));
            memset(saPeakHeight, 0, sizeof(saPeakHeight));
            memset(saPeakVel, 0, sizeof(saPeakVel));
            update();
            return;
        }
        
        // Title bar
        if (y < 14) {
            if (x >= 264 && x < 273) { close(); return; }           // Close
            if (x >= 244 && x < 253) { showMinimized(); return; }   // Minimize
            isDragging = true;
            dragPosition = event->globalPosition().toPoint() - frameGeometry().topLeft();
            return;
        }
        
        // Transport buttons
        int btnId = getButtonAt(x, y);
        if (btnId >= 0) {
            pressedButton = btnId;
            update();
            return;
        }
        
        // Shuffle button: (164,89) to (211,104)
        if (x >= 164 && x < 211 && y >= 89 && y < 104) {
            shuffleOn = !shuffleOn;
            update();
            return;
        }
        
        // Repeat button: (210,89) to (238,104)
        if (x >= 210 && x < 238 && y >= 89 && y < 104) {
            repeatOn = !repeatOn;
            update();
            return;
        }
        
        // EQ button: (219,58) to (242,70)
        if (x >= 219 && x < 242 && y >= 58 && y < 70) {
            eqBtnOn = !eqBtnOn;
            if (eqBtnOn) eqWindow->show(); else eqWindow->hide();
            update();
            return;
        }
        
        // PL button: (242,58) to (265,70)
        if (x >= 242 && x < 265 && y >= 58 && y < 70) {
            plBtnOn = !plBtnOn;
            if (plBtnOn) playlistWindow->show(); else playlistWindow->hide();
            update();
            return;
        }
        
        // Volume slider: (107,57) to (175,70)
        if (x >= 107 && x <= 175 && y >= 57 && y <= 70) {
            isDraggingVolume = true;
            volume = ((x - 107) * 255) / 68;
            if (volume > 255) volume = 255;
            if (volume < 0) volume = 0;
            audioOutput->setVolume(volume / 255.0f);
            update();
            return;
        }
        
        // Position bar: (16,72) to (264,82)
        if (x >= 16 && x <= 264 && y >= 72 && y <= 82 && player->duration() > 0) {
            isDraggingPos = true;
            qint64 newPos = ((qint64)(x - 16) * player->duration()) / 248;
            player->setPosition(newPos);
            update();
            return;
        }
        
        update();
    }
    
    void mouseMoveEvent(QMouseEvent *event) override {
        int x = event->position().x();
        int y = event->position().y();
        
        // Update hovered button
        int oldHover = hoveredButton;
        hoveredButton = getButtonAt(x, y);
        if (oldHover != hoveredButton) update();
        
        // Volume drag
        if (isDraggingVolume) {
            volume = ((x - 107) * 255) / 68;
            if (volume > 255) volume = 255;
            if (volume < 0) volume = 0;
            audioOutput->setVolume(volume / 255.0f);
            update();
        }
        
        // Position drag
        if (isDraggingPos && player->duration() > 0) {
            int clampX = qBound(16, (int)x, 264);
            qint64 newPos = ((qint64)(clampX - 16) * player->duration()) / 248;
            player->setPosition(newPos);
            update();
        }
        
        if (isDragging) {
            move(event->globalPosition().toPoint() - dragPosition);
            playlistWindow->followMain();
            eqWindow->followMain();
        }
    }
    
    void mouseReleaseEvent(QMouseEvent *event) override {
        if (pressedButton >= 0) {
            int x = event->pos().x();
            int y = event->pos().y();
            int btnId = getButtonAt(x, y);
            
            if (btnId == pressedButton) {
                switch (btnId) {
                    case 0: {                                       // Previous
                        int curIdx = playlistWindow->currentTrackIndex();
                        if (curIdx > 0) {
                            playlistWindow->setCurrentTrackIndex(curIdx - 1);
                            playTrack(playlistWindow->trackAt(curIdx - 1));
                        } else {
                            player->setPosition(0);
                        }
                        break;
                    }
                    case 1:                                          // Play
                        if (!currentFile.isEmpty()) player->play();
                        else openFile();
                        break;
                    case 2: player->pause(); break;                  // Pause
                    case 3: player->stop(); break;                   // Stop
                    case 4: {                                        // Next
                        int curIdx = playlistWindow->currentTrackIndex();
                        int count = playlistWindow->trackCount();
                        if (curIdx + 1 < count) {
                            playlistWindow->setCurrentTrackIndex(curIdx + 1);
                            playTrack(playlistWindow->trackAt(curIdx + 1));
                        }
                        break;
                    }
                    case 5: openFile(); break;                       // Eject
                }
            }
            pressedButton = -1;
            update();
        }
        
        isDraggingVolume = false;
        isDraggingPos = false;
        isDragging = false;
    }
    
    void openFile() {
        QString fileName = QFileDialog::getOpenFileName(this, "Open Audio File", "",
            "Audio Files (*.mp3 *.wav *.flac *.ogg *.m4a);;All Files (*)");
        if (!fileName.isEmpty()) {
            currentFile = fileName;
            player->setSource(QUrl::fromLocalFile(fileName));
            player->play();
            playlistWindow->addTrack(fileName);
        }
    }
    
    void playTrack(const QString &fileName) {
        if (!fileName.isEmpty() && QFile::exists(fileName)) {
            currentFile = fileName;
            // Reset media info — will be refreshed by metaDataChanged and processAudioBuffer
            mediaBitrate = 0;
            mediaSampleRate = 0;
            mediaChannels = 0;
            player->setSource(QUrl::fromLocalFile(fileName));
            player->play();
        }
    }
    
    void updateDisplay() {
        update();
    }

    int getButtonAt(int x, int y) {
        if (y >= 88 && y <= 106) {
            if (x >= 16 && x < 39)  return 0;  // Previous
            if (x >= 39 && x < 62)  return 1;  // Play
            if (x >= 62 && x < 85)  return 2;  // Pause
            if (x >= 85 && x < 108) return 3;  // Stop
            if (x >= 108 && x < 130) return 4; // Next
        }
        if (y >= 89 && y <= 105 && x >= 136 && x < 158) return 5; // Eject
        return -1;
    }
    
    void closeEvent(QCloseEvent *event) override {
        saveAllSettings();
        event->accept();
        QApplication::quit();
    }
    
    void saveAllSettings() {
        QSettings s(configPath(), QSettings::IniFormat);
        
        s.beginGroup("MainWindow");
        s.setValue("x", x());
        s.setValue("y", y());
        s.endGroup();
        
        s.beginGroup("Playback");
        s.setValue("volume", volume);
        s.setValue("shuffle", shuffleOn);
        s.setValue("repeat", repeatOn);
        s.setValue("eqVisible", eqBtnOn);
        s.setValue("plVisible", plBtnOn);
        if (!currentFile.isEmpty()) {
            s.setValue("lastFile", currentFile);
        }
        s.endGroup();
        
        eqWindow->saveSettings(s);
        playlistWindow->saveSettings(s);
        
        s.sync();
    }
    
    void loadAllSettings() {
        QString path = configPath();
        if (!QFile::exists(path)) return;
        
        QSettings s(path, QSettings::IniFormat);
        
        s.beginGroup("MainWindow");
        if (s.contains("x")) {
            move(s.value("x").toInt(), s.value("y").toInt());
        }
        s.endGroup();
        
        s.beginGroup("Playback");
        volume = s.value("volume", 200).toInt();
        audioOutput->setVolume(volume / 255.0f);
        shuffleOn = s.value("shuffle", false).toBool();
        repeatOn = s.value("repeat", false).toBool();
        eqBtnOn = s.value("eqVisible", false).toBool();
        plBtnOn = s.value("plVisible", true).toBool();  // Show playlist by default for testing
        QString lastFile = s.value("lastFile").toString();
        if (!lastFile.isEmpty() && QFile::exists(lastFile)) {
            currentFile = lastFile;
        }
        s.endGroup();
        
        eqWindow->loadSettings(s);
        playlistWindow->loadSettings(s);
        
        // Position child windows relative to loaded main position
        eqWindow->move(x(), y() + height());
        playlistWindow->move(x() + width(), y());  // right of main
        
        // Show/hide child windows based on saved state
        if (eqBtnOn) eqWindow->show();
        if (plBtnOn) playlistWindow->show();
        
        update();
    }

private:
    QMediaPlayer *player;
    QAudioOutput *audioOutput;
    QAudioBufferOutput *audioBufferOutput;
    QTimer *timer;
    QTimer *scrollTimer;
    QString currentFile;
    QPoint dragPosition;
    bool isDragging;
    int volume;  // 0-255 like original
    int hoveredButton;
    int pressedButton;
    bool shuffleOn, repeatOn, eqBtnOn, plBtnOn;
    bool isDraggingVolume, isDraggingPos;
    int scrollOffset;
    
    // Visualization state
    int visMode;  // 0=off, 1=spectrum analyzer, 2=oscilloscope (matches Windows config_sa)
    int saBarHeight[19];       // Current bar heights (0-15) for smooth fall-off
    int saPeakHeight[19];      // Peak dot positions
    float saPeakVel[19];       // Peak dot fall velocity
    float spectrumData[75];    // FFT spectrum bands (0.0-1.0)
    float oscData[75];         // Oscilloscope samples (-1.0 to 1.0)
    
    // Media info for kbps/khz/mono-stereo display
    int mediaBitrate = 0;    // in kbps
    int mediaSampleRate = 0; // in kHz (e.g. 44)
    int mediaChannels = 0;   // 1=mono, 2=stereo
    
    PlaylistWindow *playlistWindow;
    EqualizerWindow *eqWindow;
};

// PlaylistWindow snap methods
void PlaylistWindow::checkSnap() {
    if (!mainWindow) return;
    
    const int snapDist = 15;
    QPoint mainPos = mainWindow->pos();
    QSize mainSize = mainWindow->size();
    QPoint myPos = pos();
    
    // Snap to right of main window
    if (qAbs(myPos.x() - (mainPos.x() + mainSize.width())) < snapDist &&
        qAbs(myPos.y() - mainPos.y()) < snapDist) {
        move(mainPos.x() + mainSize.width(), mainPos.y());
        snapMode = 1;  // right of main
        return;
    }
    
    // Snap below EQ (if EQ is visible and snapped below main)
    // EQ is at main.y + main.height, so playlist goes at main.y + main.height + eq.height
    int eqBottom = mainPos.y() + mainSize.height() + 116;  // EQ is 116px tall
    if (qAbs(myPos.x() - mainPos.x()) < snapDist &&
        qAbs(myPos.y() - eqBottom) < snapDist) {
        move(mainPos.x(), eqBottom);
        snapMode = 2;  // below EQ
        return;
    }
    
    // Snap below main window directly
    if (qAbs(myPos.x() - mainPos.x()) < snapDist &&
        qAbs(myPos.y() - (mainPos.y() + mainSize.height())) < snapDist) {
        move(mainPos.x(), mainPos.y() + mainSize.height());
        snapMode = 3;  // below main
        return;
    }
    
    snapMode = 0;
}

void PlaylistWindow::followMain() {
    if (!mainWindow || !isVisible()) return;
    QPoint mainPos = mainWindow->pos();
    
    switch (snapMode) {
        case 1:  // right of main
            move(mainPos.x() + mainWindow->width(), mainPos.y());
            break;
        case 2:  // below EQ
            move(mainPos.x(), mainPos.y() + mainWindow->height() + 116);
            break;
        case 3:  // below main
            move(mainPos.x(), mainPos.y() + mainWindow->height());
            break;
    }
}

// EqualizerWindow snap methods
void EqualizerWindow::checkSnap() {
    if (!mainWindow) return;
    
    const int snapDist = 15;
    QPoint mainPos = mainWindow->pos();
    QSize mainSize = mainWindow->size();
    QPoint myPos = pos();
    
    if (qAbs(myPos.x() - mainPos.x()) < snapDist &&
        qAbs(myPos.y() - (mainPos.y() + mainSize.height())) < snapDist) {
        move(mainPos.x(), mainPos.y() + mainSize.height());
        isSnappedToMain = true;
    } else {
        isSnappedToMain = false;
    }
}

void EqualizerWindow::followMain() {
    if (isSnappedToMain && mainWindow && isVisible()) {
        move(mainWindow->pos().x(), mainWindow->pos().y() + mainWindow->height());
    }
}

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);

    // Load the Winamp icon from the source resource directory
    QString appDir = QCoreApplication::applicationDirPath();
    QStringList iconCandidates = {
        appDir + "/../../Src/Winamp/resource/WinampIcon.ico",
        appDir + "/../Src/Winamp/resource/WinampIcon.ico",
        appDir + "/WinampIcon.ico"
    };
    for (const QString &iconPath : iconCandidates) {
        if (QFile::exists(iconPath)) {
            app.setWindowIcon(QIcon(iconPath));
            break;
        }
    }

    // Load bitmaps — try saved skin, then project skins/default, then resource dir
    QSettings settings(configPath(), QSettings::IniFormat);
    QString skinPath = settings.value("skin").toString();

    // Build a list of candidate paths
    QStringList candidates;
    if (!skinPath.isEmpty()) candidates << skinPath;

    // Paths relative to the executable
    candidates << appDir + "/../skins/default"     // demo/build/ -> skins/default
               << appDir + "/../../skins/default"   // deeper build dirs
               << QDir::homePath() + "/.winamp/skins/default";

    // Also try the source resource directory (has MAIN.BMP etc.)
    candidates << appDir + "/../Src/Winamp/resource"
               << appDir + "/../../Src/Winamp/resource";

    bool loaded = false;
    for (const QString &path : candidates) {
        QDir d(path);
        if (d.exists() && WinampBitmaps::instance().loadAll(d.absolutePath())) {
            qDebug() << "Successfully loaded authentic Winamp bitmaps from:" << d.absolutePath();
            loaded = true;
            break;
        }
    }

    // Also try loading any missing bitmaps from all other candidate paths
    if (loaded) {
        for (const QString &path : candidates) {
            QDir d(path);
            if (d.exists()) {
                WinampBitmaps::instance().loadMissing(d.absolutePath());
            }
        }
    }

    if (!loaded) {
        qWarning() << "Could not load Winamp skin bitmaps from any candidate path.";
        // Continue anyway — fallback rendering will be used
    }

    WinampWindow w;
    w.show();
    
    return app.exec();
}

#include "winamp_authentic.moc"
