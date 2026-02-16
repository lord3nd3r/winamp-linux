#include <QApplication>
#include <QWidget>
#include <QPainter>
#include <QMediaPlayer>
#include <QMediaMetaData>
#include <QAudioOutput>
#include <QAudioBufferOutput>
#include <QAudioBuffer>
#include <QAudioSink>
#include <QAudioDevice>
#include <QMediaDevices>
#include <QVideoWidget>
#include <QVideoSink>
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
#include <QDesktopServices>
#include <QMessageBox>
#include <QSet>
#include <QOpenGLWidget>
#include <QOpenGLFunctions>
#include <QKeyEvent>
#include <QTreeView>
#include <QFileSystemModel>
#include <QStandardPaths>
#include <type_traits>
#include <QSystemTrayIcon>
#include <QToolTip>
#include <QSplashScreen>
#include <QStandardPaths>
#include <QScreen>
#include <QScrollBar>
#include <QComboBox>
#include <QCheckBox>
#include <QSpinBox>
#include <QGroupBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QTreeWidget>
#include <QStackedWidget>
#include <QHeaderView>
#include <QSlider>
#include <QTextEdit>
#include <QSizeGrip>
#include <QRegularExpression>

// D-Bus for MPRIS2 media player interface (Linux desktop integration)
#ifdef QT_DBUS_LIB
#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusInterface>
#include <QDBusAbstractAdaptor>
#include <QDBusObjectPath>
#include <QDBusMetaType>
#endif

// projectM — Milkdrop-compatible visualization engine
#include <libprojectM/projectM.hpp>

// ========================================================================
// EQ10 DSP Engine — ported from Winamp's eq10dsp.cpp / eq10dsp.h
// Original: Copyright (C) 2002 4Front Technologies, by George Yohng
// 10-band graphic equalizer with asymmetric Q (narrow boost, wide cut)
// and dynamic limiter. This is the REAL Winamp EQ algorithm.
// ========================================================================

#define EQ10_NOFBANDS 10
#define EQ10_Q        1.41   // global Q factor (matches original)
#define EQ10_TRIM_CODE    0.930  // limiter trim at -0.6dB
#define EQ10_TRIM_RELEASE 0.700  // limiter release time in seconds

struct eq10band_t {
    double gain;
    double ua0, ub1, ub2;  // "up" coefficients (boost, narrow Q)
    double da0, db1, db2;  // "down" coefficients (cut, wide Q)
    double x1, x2, y1, y2; // filter state
};

struct eq10_t {
    double rate;
    eq10band_t band[EQ10_NOFBANDS];
    double detect;       // limiter peak detector
    double detectdecay;  // limiter release coefficient
};

// Winamp-style frequency table (matches eq10dsp.cpp line 28)
static const double eq10_freq[EQ10_NOFBANDS] = {
    70, 180, 320, 600, 1000, 3000, 6000, 12000, 14000, 16000
};

// Preamp lookup table — 64 entries mapping slider 0-63 to linear gain
// (matches In.cpp eq_lookup1[64]: index 0 = +12dB = 4.0x, 31 = 0dB = 1.0x, 63 = -12dB = 0.25x)
static const float eq_preamp_table[64] = {
    4.000000f, 3.610166f, 3.320019f, 3.088821f, 2.896617f,
    2.732131f, 2.588368f, 2.460685f, 2.345845f, 2.241498f,
    2.145887f, 2.057660f, 1.975760f, 1.899338f, 1.827707f,
    1.760303f, 1.696653f, 1.636363f, 1.579094f, 1.524558f,
    1.472507f, 1.422724f, 1.375019f, 1.329225f, 1.285197f,
    1.242801f, 1.201923f, 1.162456f, 1.124306f, 1.087389f,
    1.051628f, 1.000000f, 0.983296f, 0.950604f, 0.918821f,
    0.887898f, 0.857789f, 0.828454f, 0.799853f, 0.771950f,
    0.744712f, 0.718108f, 0.692110f, 0.666689f, 0.641822f,
    0.617485f, 0.593655f, 0.570311f, 0.547435f, 0.525008f,
    0.503013f, 0.481433f, 0.460253f, 0.439458f, 0.419035f,
    0.398970f, 0.379252f, 0.359868f, 0.340807f, 0.322060f,
    0.303614f, 0.285462f, 0.267593f, 0.250000f
};

// Slider value (0-63) to dB (matches In.cpp VALTODB)
static inline double eq10_valtodb(int v) {
    v -= 31;
    if (v < -31) v = -31;
    if (v > 32) v = 32;
    if (v > 0) return -12.0 * (v / 32.0);
    else if (v < 0) return -12.0 * (v / 31.0);
    return 0.0;
}

// dB to internal gain value (matches eq10dsp.cpp eq10_db2gain)
static inline double eq10_db2gain(double gain_dB) {
    return pow(10.0, gain_dB / 20.0) - 1.0;
}

// Setup bandpass coefficients for one direction (boost or cut)
static void eq10_bsetup2(int u, double rate, eq10band_t *band, double freq, double Q) {
    if (rate < 4000.0) rate = 4000.0;
    if (rate > 384000.0) rate = 384000.0;
    if (freq < 20.0) freq = 20.0;
    if (freq >= (rate * 0.499)) { band->ua0 = band->da0 = 0; return; }

    double angle = 2.0 * M_PI * freq / rate;
    double alpha = sin(angle) / (2.0 * Q);

    double b0 = 1.0 / (1.0 + alpha);
    double a0 = b0 * alpha;
    double b1 = b0 * 2 * cos(angle);
    double b2 = b0 * (alpha - 1);

    if (u > 0) { band->ua0 = a0; band->ub1 = b1; band->ub2 = b2; }
    else       { band->da0 = a0; band->db1 = b1; band->db2 = b2; }
}

// Setup one band: wide Q for cut (Q*0.5), narrow Q for boost (Q*2.0)
static void eq10_bsetup(double rate, eq10band_t *band, double freq, double Q) {
    memset(band, 0, sizeof(*band));
    eq10_bsetup2(-1, rate, band, freq, Q * 0.5);
    eq10_bsetup2(+1, rate, band, freq, Q * 2.0);
}

// Initialize EQ for all channels
static void eq10_setup(eq10_t *eq, int eqs, double rate) {
    for (int k = 0; k < eqs; k++, eq++) {
        eq->rate = rate;
        for (int t = 0; t < EQ10_NOFBANDS; t++)
            eq10_bsetup(rate, &eq->band[t], eq10_freq[t], EQ10_Q);
        eq->detect = 0;
        eq->detectdecay = pow(0.001, 1.0 / (rate * EQ10_TRIM_RELEASE));
    }
}

// Set gain for a band across all channels
static void eq10_setgain(eq10_t *eq, int eqs, int bandnr, double gain_dB) {
    double realgain = eq10_db2gain(gain_dB);
    for (int k = 0; k < eqs; k++)
        eq[k].band[bandnr].gain = realgain;
}

// Process float samples through one channel's EQ (matches eq10dsp.cpp eq10_processf)
// buf = input (interleaved), outbuf = output, sz = frame count, idx = channel, step = channel count
static void eq10_processf(eq10_t *eq, float *buf, float *outbuf, int sz, int idx, int step) {
    if (!eq) return;
    buf += idx;
    outbuf += idx;
    float *in = buf;

    for (int k = 0; k < EQ10_NOFBANDS; k++) {
        double a0, b1, b2;
        double x1 = eq->band[k].x1, x2 = eq->band[k].x2;
        double y1 = eq->band[k].y1, y2 = eq->band[k].y2;
        double gain = eq->band[k].gain;
        float *out = outbuf;

        if (gain > 0.0) {
            a0 = eq->band[k].ua0 * gain;
            b1 = eq->band[k].ub1;
            b2 = eq->band[k].ub2;
        } else {
            a0 = eq->band[k].da0 * gain;
            b1 = eq->band[k].db1;
            b2 = eq->band[k].db2;
        }

        if (a0 == 0.0) continue;

        for (int t = 0; t < sz; t++, in += step, out += step) {
            double y0 = (in[0] - x2) * a0 + y1 * b1 + y2 * b2 + 1e-30; // denormal fix
            x2 = x1; x1 = in[0]; y2 = y1; y1 = y0;
            out[0] = (float)(y0 + in[0]); // parallel bandpass topology
        }
        in = outbuf; // chain bands serially
        eq->band[k].x1 = x1; eq->band[k].x2 = x2;
        eq->band[k].y1 = y1; eq->band[k].y2 = y2;
    }

    // Dynamic limiter (matches eq10dsp.cpp)
    {
        double detect = eq->detect;
        double detectdecay = eq->detectdecay;
        float *out = outbuf;
        for (int t = 0; t < sz; t++, in += step, out += step) {
            if (fabs(in[0]) > detect) detect = fabs(in[0]);
            if (detect > EQ10_TRIM_CODE)
                out[0] = in[0] * (float)(EQ10_TRIM_CODE / detect);
            else
                out[0] = in[0];
            detect *= detectdecay;
            detect += 1e-30; // denormal fix
        }
        eq->detect = detect;
    }
}

// ========================================================================
// End EQ10 DSP Engine
// ========================================================================

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
// Skin Playlist Colors — parse PLEDIT.TXT (from Windows skins)
// ============================================================
struct SkinPlaylistColors {
    QColor normal      = QColor(0, 255, 0);       // Normal text
    QColor current     = QColor(255, 255, 255);    // Currently playing
    QColor normBg      = QColor(0, 0, 0);          // Background
    QColor selectBg    = QColor(0, 0, 198);         // Selection background
    QColor mbFg        = QColor(0, 255, 0);         // Minibar foreground
    QColor mbBg        = QColor(0, 0, 0);           // Minibar background
};

static SkinPlaylistColors parsePleditTxt(const QString &skinPath) {
    SkinPlaylistColors colors;
    QStringList candidates = {
        skinPath + "/PLEDIT.TXT",
        skinPath + "/pledit.txt",
        skinPath + "/Pledit.txt"
    };
    for (const QString &path : candidates) {
        QFile file(path);
        if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
            QTextStream in(&file);
            while (!in.atEnd()) {
                QString line = in.readLine().trimmed();
                if (line.startsWith("Normal=", Qt::CaseInsensitive)) {
                    line = line.mid(line.indexOf('=') + 1).trimmed();
                    if (line.startsWith('#')) colors.normal = QColor(line);
                    else { QStringList p = line.split(','); if (p.size()==3) colors.normal = QColor(p[0].toInt(), p[1].toInt(), p[2].toInt()); }
                } else if (line.startsWith("Current=", Qt::CaseInsensitive)) {
                    line = line.mid(line.indexOf('=') + 1).trimmed();
                    if (line.startsWith('#')) colors.current = QColor(line);
                    else { QStringList p = line.split(','); if (p.size()==3) colors.current = QColor(p[0].toInt(), p[1].toInt(), p[2].toInt()); }
                } else if (line.startsWith("NormalBG=", Qt::CaseInsensitive)) {
                    line = line.mid(line.indexOf('=') + 1).trimmed();
                    if (line.startsWith('#')) colors.normBg = QColor(line);
                    else { QStringList p = line.split(','); if (p.size()==3) colors.normBg = QColor(p[0].toInt(), p[1].toInt(), p[2].toInt()); }
                } else if (line.startsWith("SelectedBG=", Qt::CaseInsensitive)) {
                    line = line.mid(line.indexOf('=') + 1).trimmed();
                    if (line.startsWith('#')) colors.selectBg = QColor(line);
                    else { QStringList p = line.split(','); if (p.size()==3) colors.selectBg = QColor(p[0].toInt(), p[1].toInt(), p[2].toInt()); }
                } else if (line.startsWith("MbFG=", Qt::CaseInsensitive)) {
                    line = line.mid(line.indexOf('=') + 1).trimmed();
                    if (line.startsWith('#')) colors.mbFg = QColor(line);
                    else { QStringList p = line.split(','); if (p.size()==3) colors.mbFg = QColor(p[0].toInt(), p[1].toInt(), p[2].toInt()); }
                } else if (line.startsWith("MbBG=", Qt::CaseInsensitive)) {
                    line = line.mid(line.indexOf('=') + 1).trimmed();
                    if (line.startsWith('#')) colors.mbBg = QColor(line);
                    else { QStringList p = line.split(','); if (p.size()==3) colors.mbBg = QColor(p[0].toInt(), p[1].toInt(), p[2].toInt()); }
                }
            }
            file.close();
            break;
        }
    }
    return colors;
}

// Global skin playlist colors (loaded when skin changes)
static SkinPlaylistColors g_plColors;

// ============================================================
// Bookmark Manager — store/retrieve bookmarked files and URLs
// ============================================================
class BookmarkManager {
public:
    struct Bookmark {
        QString title;
        QString path;  // file path or URL
    };

    static BookmarkManager& instance() {
        static BookmarkManager mgr;
        return mgr;
    }

    void load() {
        bookmarks.clear();
        QString dir = QDir::homePath() + "/.config/winamp";
        QDir().mkpath(dir);
        QFile file(dir + "/bookmarks.txt");
        if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
            QTextStream in(&file);
            while (!in.atEnd()) {
                QString line = in.readLine().trimmed();
                if (line.isEmpty() || line.startsWith('#')) continue;
                int sep = line.indexOf('\t');
                Bookmark bm;
                if (sep > 0) {
                    bm.title = line.left(sep);
                    bm.path = line.mid(sep + 1);
                } else {
                    bm.path = line;
                    bm.title = QFileInfo(line).baseName();
                }
                bookmarks.append(bm);
            }
            file.close();
        }
    }

    void save() {
        QString dir = QDir::homePath() + "/.config/winamp";
        QDir().mkpath(dir);
        QFile file(dir + "/bookmarks.txt");
        if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
            QTextStream out(&file);
            out << "# Winamp Bookmarks\n";
            for (const Bookmark &bm : bookmarks) {
                out << bm.title << "\t" << bm.path << "\n";
            }
            file.close();
        }
    }

    void addBookmark(const QString &title, const QString &path) {
        // Don't add duplicates
        for (const Bookmark &bm : bookmarks) {
            if (bm.path == path) return;
        }
        bookmarks.append({title, path});
        save();
    }

    void removeBookmark(int index) {
        if (index >= 0 && index < bookmarks.size()) {
            bookmarks.removeAt(index);
            save();
        }
    }

    QList<Bookmark> bookmarks;

private:
    BookmarkManager() { load(); }
};

// ============================================================
// Recent Files Manager — track recently played files
// ============================================================
class RecentFilesManager {
public:
    static RecentFilesManager& instance() {
        static RecentFilesManager mgr;
        return mgr;
    }

    void load() {
        recentFiles.clear();
        QSettings s(QDir::homePath() + "/.config/winamp/winamp.conf", QSettings::IniFormat);
        int count = s.beginReadArray("RecentFiles");
        for (int i = 0; i < count; i++) {
            s.setArrayIndex(i);
            recentFiles.append(s.value("path").toString());
        }
        s.endArray();
    }

    void save() {
        QSettings s(QDir::homePath() + "/.config/winamp/winamp.conf", QSettings::IniFormat);
        s.beginWriteArray("RecentFiles");
        for (int i = 0; i < recentFiles.size(); i++) {
            s.setArrayIndex(i);
            s.setValue("path", recentFiles[i]);
        }
        s.endArray();
    }

    void addFile(const QString &path) {
        recentFiles.removeAll(path);
        recentFiles.prepend(path);
        while (recentFiles.size() > maxRecent)
            recentFiles.removeLast();
        save();
    }

    QStringList recentFiles;
    int maxRecent = 15;

private:
    RecentFilesManager() { load(); }
};

// ============================================================
// Jump to File Dialog — search within playlist (Ctrl+J / J)
// ============================================================
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

// ====================================================================
// File Info Dialog — Display/edit ID3 tags (Alt+3, matches FileInfo.cpp)
// ====================================================================
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

// Winamp visualization colors (24 entries from draw.cpp ppal2[])
// Can be overridden by viscolor.txt in skin directory
static QColor visColors[24] = {
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

// Load viscolor.txt if present (Windows-compatible format: 24 lines of "r,g,b")
static void loadVisColors(const QString &skinPath) {
    QFile file(skinPath + "/viscolor.txt");
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        // Try case variations
        file.setFileName(skinPath + "/VISCOLOR.TXT");
        if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) return;
    }
    
    QTextStream in(&file);
    int idx = 0;
    while (!in.atEnd() && idx < 24) {
        QString line = in.readLine().trimmed();
        if (line.isEmpty() || line.startsWith("#") || line.startsWith(";")) continue;
        
        // Parse "r,g,b" format (matching draw.cpp line 402)
        QStringList parts = line.split(",");
        if (parts.size() >= 3) {
            int r = parts[0].trimmed().toInt();
            int g = parts[1].trimmed().toInt();
            int b = parts[2].trimmed().toInt();
            visColors[idx] = QColor(r, g, b);
            idx++;
        }
    }
}

// ============================================================
// Simple Translation System for Language Pack Support
// ============================================================
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
    
    void loadLanguage(const QString &langCode) {
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
    
    QString getCurrentLanguage() const { return currentLang; }
    
    QStringList getAvailableLanguages() {
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
    
private:
    Translator() { loadEnglishDefaults(); }
    
    void loadFromFile(const QString &filePath) {
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
    
    void loadEnglishDefaults() {
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
    
    QMap<QString, QString> strings;
    QString currentLang;
};

// Convenience macro
#define TR(key, def) Translator::instance().tr(key, def)

// Forward declarations
class WinampWindow;
class VideoWindow;
class MilkdropWindow;
class MediaLibraryWindow;

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

// Preferences Dialog — Tree-based layout matching Windows Winamp (Options.cpp)
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
        addPage(setupItem, "Titles", createTitlesPage());
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
        layout->addWidget(new QLabel("Select a skin category on the left.\nClassic skins (.wsz) are currently supported."));
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

    // ---- Plugins overview page ----
    QWidget *createPluginsPage() {
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

    // ---- Skin list helpers ----
    QListWidget *skinListWidget = nullptr;

    void populateSkins() {
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
        numbers_ex = loadBmp("nums_ex.bmp");  // Extended numbers with animated colon (optional)
        text = loadBmp("text.bmp");
        playpaus = loadBmp("PLAYPAUS.BMP");
        monoster = loadBmp("MONOSTER.BMP");
        posbar = loadBmp("POSBAR.BMP");
        volume = loadBmp("volume.bmp");
        balance = loadBmp("BALANCE.BMP");
        shufrep = loadBmp("SHUFREP.BMP");
        eqmain = loadBmp("Eqmain.bmp");
        pledit = loadBmp("Pledit.bmp");
        
        // Load custom visualization colors if present (Windows viscolor.txt format)
        loadVisColors(basePath);
        
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
        tryLoad(numbers_ex, "nums_ex.bmp");  // Optional extended numbers
        tryLoad(text, "text.bmp");
        tryLoad(playpaus, "PLAYPAUS.BMP");
        tryLoad(monoster, "MONOSTER.BMP");
        tryLoad(posbar, "POSBAR.BMP");
        tryLoad(volume, "volume.bmp");
        tryLoad(balance, "BALANCE.BMP");
        tryLoad(shufrep, "SHUFREP.BMP");
        tryLoad(eqmain, "Eqmain.bmp");
        tryLoad(pledit, "Pledit.bmp");
    }
    
    QPixmap main, cbuttons, titlebar, numbers, numbers_ex, text;
    QPixmap playpaus, monoster, posbar, volume, balance, shufrep;
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

// Custom list widget for dragging tracks to file manager
class PlaylistListWidget : public QListWidget {
    Q_OBJECT
public:
    PlaylistListWidget(QWidget *parent = nullptr) : QListWidget(parent) {}
    
protected:
    QMimeData* mimeData(const QList<QListWidgetItem*> &items) const override {
        QMimeData *data = new QMimeData();
        QList<QUrl> urls;
        
        for (const QListWidgetItem *item : items) {
            QString filePath = item->data(Qt::UserRole).toString();
            if (!filePath.isEmpty()) {
                urls.append(QUrl::fromLocalFile(filePath));
            }
        }
        
        if (!urls.isEmpty()) {
            data->setUrls(urls);
        }
        
        return data;
    }
};

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
    QStringList allTracks() const { return tracks; }
    
    // Track navigation convenience methods (called by MPRIS2, tray menu, etc.)
    void nextTrack() {
        int idx = currentTrackIndex();
        if (idx + 1 < trackCount()) {
            setCurrentTrackIndex(idx + 1);
            emit trackDoubleClicked(trackAt(idx + 1));
        }
    }
    void prevTrack() {
        int idx = currentTrackIndex();
        if (idx > 0) {
            setCurrentTrackIndex(idx - 1);
            emit trackDoubleClicked(trackAt(idx - 1));
        }
    }
    void playCurrentTrack() {
        int idx = currentTrackIndex();
        if (idx >= 0 && idx < trackCount())
            emit trackDoubleClicked(trackAt(idx));
    }
    
    // Appearance
    void applyPlaylistColors();
    void setPlaylistFont(const QString &family, int size) {
        playlistFontFamily = family;
        playlistFontSize = size;
        applyPlaylistColors();
    }
    
    // Window shade mode (compact single-line view)
    void toggleShadeMode() {
        shadeMode = !shadeMode;
        if (shadeMode) {
            savedHeight = height();
            setMinimumSize(275, 14);
            resize(width(), 14);
        } else {
            setMinimumSize(275, 116);
            resize(width(), savedHeight > 116 ? savedHeight : 232);
            updateListGeometry();
        }
        listWidget->setVisible(!shadeMode);
        update();
    }
    bool isShadeMode() const { return shadeMode; }

signals:
    void trackDoubleClicked(const QString &filePath);

protected:
    void paintEvent(QPaintEvent *) override;
    void mousePressEvent(QMouseEvent *) override;
    void mouseMoveEvent(QMouseEvent *) override;
    void mouseReleaseEvent(QMouseEvent *) override;
    void mouseDoubleClickEvent(QMouseEvent *) override;
    void dragEnterEvent(QDragEnterEvent *) override;
    void dropEvent(QDropEvent *) override;
    void resizeEvent(QResizeEvent *) override;
    bool eventFilter(QObject *obj, QEvent *event) override;

private:
    void updateTotalTimeDisplay();
    void updateListGeometry();
    void drawText(QPainter &painter, const QString &text, int x, int y);
    QPoint getTextCharPos(QChar ch);
    void showAddMenu(QPoint globalPos);
    void showRemMenu(QPoint globalPos);
    void showSelMenu(QPoint globalPos);
    void showMiscMenu(QPoint globalPos);
    void showListMenu(QPoint globalPos);
    void showContextMenu(QPoint globalPos);
    
    // Resize edge detection
    enum ResizeEdge { NoEdge = 0, RightEdge = 1, BottomEdge = 2, BottomRight = 3 };
    ResizeEdge hitTestResize(const QPoint &pos) {
        const int margin = 6;
        bool atRight = pos.x() >= width() - margin;
        bool atBottom = pos.y() >= height() - margin;
        if (atRight && atBottom) return BottomRight;
        if (atRight) return RightEdge;
        if (atBottom) return BottomEdge;
        return NoEdge;
    }
    
    // Playlist operations
    void removeSelected();
    void cropSelected();
    void removeDeadFiles();
    void moveSelectedUp();
    void moveSelectedDown();
    void sortByTitle();
    void sortByFilename();
    void sortByPath();
    void reverseList();
    void randomizeList();
    void exploreFolderOfSelected();
    void generateHtmlPlaylist();
    QString trackDisplayName(int index, const QString &filePath) {
        return QString("%1. %2").arg(index + 1).arg(QFileInfo(filePath).fileName());
    }
    void rebuildListDisplay() {
        listWidget->clear();
        for (int i = 0; i < tracks.size(); i++) {
            QListWidgetItem *item = new QListWidgetItem(trackDisplayName(i, tracks[i]));
            item->setData(Qt::UserRole, tracks[i]); // Store full file path for drag-out
            listWidget->addItem(item);
        }
    }

    PlaylistListWidget *listWidget;
    QList<QString> tracks;
    QList<qint64> trackDurations; // Store durations in milliseconds
    QString totalTimeStr; // Formatted string for display
    QPoint dragPosition;
    bool isDragging = false;
    bool isSnappedToMain = false;
    WinampWindow *mainWindow = nullptr;
    int snapMode = 0;  // 0=none, 1=right of main, 2=below EQ, 3=below main
    
    // Shade mode
    bool shadeMode = false;
    int savedHeight = 232;
    
    // Resize
    ResizeEdge resizeEdge = NoEdge;
    bool isResizing = false;
    QPoint resizeStartPos;
    QSize resizeStartSize;
    
    // Scrollbar dragging
    bool isDraggingScrollbar = false;
    
    // Font settings
    QString playlistFontFamily = "Courier New";
    int playlistFontSize = 8;
};

// Equalizer Window
class EqualizerWindow : public QWidget {
public:
    EqualizerWindow(WinampWindow *parent = nullptr);
    
    void setMainWindow(WinampWindow *main) { mainWindow = main; }
    
    void followMain();
    void checkSnap();
    
    // Get EQ band gain in dB (for audio processing)
    // Slider range 0-63, center=32 -> maps to +12dB...-12dB
    float getBandGainDb(int band) const {
        if (band < 0 || band >= 10) return 0.0f;
        if (!eqEnabled) return 0.0f;
        return (32 - eqValues[band]) * 12.0f / 32.0f;
    }
    float getPreampGainDb() const {
        if (!eqEnabled) return 0.0f;
        return (32 - preampValue) * 12.0f / 32.0f;
    }
    
    // Raw slider accessors for EQ DSP engine (0-63 range, matching Windows eq_tab/config_preamp)
    int getBandValue(int band) const {
        if (band < 0 || band >= 10) return 31;
        return eqValues[band];
    }
    int getPreampValue() const { return preampValue; }
    
    bool isEnabled() const { return eqEnabled; }
    
    // Auto-load EQ preset based on filename (matches Windows eq_autoload from Eq.cpp)
    void autoLoadPreset(const QString &filePath) {
        if (!autoEnabled) return;
        
        // Try loading per-file EQ preset (matches Windows EQDIR2 path)
        QString baseName = QFileInfo(filePath).completeBaseName();
        QString presetDir = QDir::homePath() + "/.config/winamp/eqpresets";
        QString perFilePreset = presetDir + "/" + baseName + ".eqf";
        
        if (QFile::exists(perFilePreset)) {
            loadPresetFile(perFilePreset);
            return;
        }
        
        // Fall back to default preset (matches Windows EQDIR1/"Default")
        QString defaultPreset = presetDir + "/Default.eqf";
        if (QFile::exists(defaultPreset)) {
            loadPresetFile(defaultPreset);
        }
    }
    
    bool isAutoEnabled() const { return autoEnabled; }
    
    // Window shade mode (compact single-line view)
    void toggleShadeMode() {
        shadeMode = !shadeMode;
        if (shadeMode) {
            setFixedSize(275, 14);
        } else {
            setFixedSize(275, 116);
        }
        update();
    }
    bool isShadeMode() const { return shadeMode; }
    
    void showPresetsMenu(QPoint globalPos) {
        QMenu menu;
        menu.setStyleSheet(
            "QMenu { background-color: #2b2d3d; color: #00ff00; border: 1px solid #555; font-size: 9pt; }"
            "QMenu::item:selected { background-color: #0000c6; }"
            "QMenu::separator { height: 1px; background: #555; margin: 2px 4px; }"
        );
        
        // Built-in presets submenu
        QMenu *builtinMenu = menu.addMenu("Presets");
        builtinMenu->setStyleSheet(menu.styleSheet());
        for (int i = 0; i < numPresets; i++) {
            QAction *action = builtinMenu->addAction(builtinPresets[i].name);
            action->setData(i);
        }
        
        menu.addSeparator();
        QAction *loadAct = menu.addAction("Load preset from file...");
        QAction *saveAct = menu.addAction("Save preset to file...");
        QAction *deleteAct = menu.addAction("Delete preset file...");
        menu.addSeparator();
        
        // Custom saved presets
        QMenu *customMenu = menu.addMenu("Saved Presets");
        customMenu->setStyleSheet(menu.styleSheet());
        QString presetDir = QDir::homePath() + "/.config/winamp/eqpresets";
        QDir().mkpath(presetDir);
        QDir dir(presetDir);
        QStringList presetFiles = dir.entryList(QStringList() << "*.eqf" << "*.EQF", QDir::Files);
        for (const QString &f : presetFiles) {
            QAction *a = customMenu->addAction(QFileInfo(f).baseName());
            a->setData("custom:" + dir.absoluteFilePath(f));
        }
        if (presetFiles.isEmpty()) {
            QAction *empty = customMenu->addAction("(no saved presets)");
            empty->setEnabled(false);
        }
        
        QAction *selected = menu.exec(globalPos);
        if (!selected) return;
        
        if (selected == loadAct) {
            loadPresetFromFile();
        } else if (selected == saveAct) {
            savePresetToFile();
        } else if (selected == deleteAct) {
            deletePresetFile();
        } else if (selected->data().toString().startsWith("custom:")) {
            loadPresetFile(selected->data().toString().mid(7));
        } else if (selected->data().isValid()) {
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
        
        // Draw frequency response curve (matches Windows draw_eq_graphthingy from draw_eq.cpp)
        drawEqFrequencyCurve(p);
        
        // Draw slider grooves and thumbs
        // Preamp at x=21, bands at x=78+n*18
        drawEqSlider(p, 0, 21);  // Preamp
        for (int i = 0; i < 10; i++) {
            drawEqSlider(p, i + 1, 78 + i * 18);
        }
    }
    
    // Draw EQ frequency response curve using spline interpolation (matches Windows draw_eq_graphthingy)
    void drawEqFrequencyCurve(QPainter &p) {
        auto &bmp = WinampBitmaps::instance();
        const int left = 86, top = 17;
        const int w = 113, h = 19;
        
        // Draw preamp level line across the graph (Windows: line 205)
        int preampY = top + h - 1 - (int)(preampValue * 19.0f / 64.0f);
        if (preampY >= top && preampY < top + h) {
            // Use line color from eqmain sprite at (0,314)
            p.setPen(QColor(0, 255, 0));  // Bright green line
            p.drawLine(left, preampY, left + w, preampY);
        }
        
        // Build spline keys for 10-band EQ (Windows: lines 207-213)
        float keys[12];
        for (int i = 0; i < 10; i++)
            keys[i + 1] = eqValues[i] * 19.0f / 64.0f;
        keys[0] = keys[1];    // Duplicate first for smooth edge
        keys[11] = keys[10];  // Duplicate last for smooth edge
        
        // Draw spline-interpolated curve (Windows: lines 215-234)
        // Catmull-Rom spline evaluation for smooth frequency response
        p.setPen(QColor(0, 198, 0));  // Slightly dimmer green for curve
        int lastY = -1;
        for (int x = 0; x < 109; x++) {
            // Map x position to spline parameter t in range [1.0, 11.0]
            float t = 1.0f + x / 12.0f;
            int idx = (int)t;
            float frac = t - idx;
            
            // Catmull-Rom spline: P(t) = 0.5 * [(2*P1) + (-P0+P2)*t + (2*P0-5*P1+4*P2-P3)*t^2 + (-P0+3*P1-3*P2+P3)*t^3]
            if (idx >= 1 && idx + 2 < 12) {
                float p0 = keys[idx - 1];
                float p1 = keys[idx];
                float p2 = keys[idx + 1];
                float p3 = keys[idx + 2];
                float t2 = frac * frac;
                float t3 = t2 * frac;
                float val = 0.5f * (
                    2.0f * p1 +
                    (-p0 + p2) * frac +
                    (2.0f * p0 - 5.0f * p1 + 4.0f * p2 - p3) * t2 +
                    (-p0 + 3.0f * p1 - 3.0f * p2 + p3) * t3
                );
                
                int curveY = (int)val;
                if (curveY < 0) curveY = 0;
                if (curveY > 18) curveY = 18;
                
                // Draw vertical line segment connecting previous and current points
                if (lastY != -1 && lastY != curveY) {
                    int y1 = qMin(lastY, curveY);
                    int y2 = qMax(lastY, curveY);
                    for (int dy = y1; dy <= y2; dy++)
                        p.drawPoint(left + 2 + x, top + dy);
                } else {
                    p.drawPoint(left + 2 + x, top + curveY);
                }
                lastY = curveY;
            }
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
    
    void mouseDoubleClickEvent(QMouseEvent *event) override {
        // Double-click on titlebar toggles shade mode
        if (event->pos().y() < 14) {
            toggleShadeMode();
            return;
        }
        QWidget::mouseDoubleClickEvent(event);
    }
    
    bool isSnapped() const { return isSnappedToMain; }
    
private:
    int eqValues[10];
    int preampValue;
    bool eqEnabled = true;
    bool autoEnabled = false;
    bool shadeMode = false;
    int draggingSlider = -1;
    QPoint dragPosition;
    bool isDragging = false;
    WinampWindow *mainWindow = nullptr;
    bool isSnappedToMain = false;
    
    // EQ preset file I/O (matches Windows writeEQfile/readEQfile)
    void loadPresetFromFile() {
        QString fileName = QFileDialog::getOpenFileName(this, "Load EQ Preset",
            QDir::homePath() + "/.config/winamp/eqpresets",
            "EQ Presets (*.eqf);;All Files (*)");
        if (!fileName.isEmpty()) loadPresetFile(fileName);
    }
    
    void loadPresetFile(const QString &path) {
        QFile file(path);
        if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
            QTextStream in(&file);
            in.readLine(); // header
            QString preampLine = in.readLine();
            if (preampLine.startsWith("Preamp="))
                preampValue = preampLine.mid(7).toInt();
            for (int i = 0; i < 10; i++) {
                QString line = in.readLine();
                if (line.startsWith(QString("Band%1=").arg(i)))
                    eqValues[i] = line.mid(line.indexOf('=') + 1).toInt();
            }
            file.close();
            update();
        }
    }
    
    void savePresetToFile() {
        QString presetDir = QDir::homePath() + "/.config/winamp/eqpresets";
        QDir().mkpath(presetDir);
        bool ok;
        QString name = QInputDialog::getText(this, "Save EQ Preset",
            "Preset name:", QLineEdit::Normal, "My Preset", &ok);
        if (!ok || name.isEmpty()) return;
        QString path = presetDir + "/" + name + ".eqf";
        QFile file(path);
        if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
            QTextStream out(&file);
            out << "[Winamp EQ Preset]\n";
            out << "Preamp=" << preampValue << "\n";
            for (int i = 0; i < 10; i++)
                out << "Band" << i << "=" << eqValues[i] << "\n";
            file.close();
        }
    }
    
    void deletePresetFile() {
        QString presetDir = QDir::homePath() + "/.config/winamp/eqpresets";
        QDir dir(presetDir);
        QStringList files = dir.entryList(QStringList() << "*.eqf", QDir::Files);
        if (files.isEmpty()) {
            QMessageBox::information(this, "Delete Preset", "No saved presets found.");
            return;
        }
        bool ok;
        QString selected = QInputDialog::getItem(this, "Delete EQ Preset",
            "Select preset to delete:", files, 0, false, &ok);
        if (ok && !selected.isEmpty()) {
            QFile::remove(presetDir + "/" + selected);
        }
    }
};

// Playlist Window Constructor
PlaylistWindow::PlaylistWindow(WinampWindow *parent) : QWidget(nullptr), mainWindow(parent) {
    setMinimumSize(275, 116);
    resize(275, 232);
    setWindowTitle(TR("win.playlist.title", "Winamp Playlist Editor"));
    setWindowFlags(Qt::Tool | Qt::FramelessWindowHint);
    
    // Position list widget within the skin frame
    // Titlebar=20px, left border=12px, right border=20px (incl scrollbar), bottom=38px
    listWidget = new PlaylistListWidget(this);
    updateListGeometry();
    applyPlaylistColors();
    listWidget->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    listWidget->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    
    // Enable drag and drop for files
    listWidget->setAcceptDrops(true);
    listWidget->setDragEnabled(true);
    listWidget->setDropIndicatorShown(true);
    listWidget->setDragDropMode(QAbstractItemView::DragDrop); // Allow both internal move and drag-out
    
    // Enable resizing from edges and corners
    setMouseTracking(true);
    
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

    // Install event filter for keyboard shortcuts and right-click context menu
    listWidget->installEventFilter(this);
}

void PlaylistWindow::applyPlaylistColors() {
    listWidget->setStyleSheet(
        QString("QListWidget {"
        "  background-color: %1;"
        "  color: %2;"
        "  border: none;"
        "  font-family: 'Courier New', 'Courier';"
        "  font-size: %3pt;"
        "  selection-background-color: %4;"
        "  selection-color: %5;"
        "}"
        "QListWidget::item {"
        "  padding: 0px;"
        "}")
        .arg(g_plColors.normBg.name())
        .arg(g_plColors.normal.name())
        .arg(playlistFontSize)
        .arg(g_plColors.selectBg.name())
        .arg(g_plColors.current.name())
    );
}

void PlaylistWindow::updateListGeometry() {
    listWidget->setGeometry(12, 20, width() - 12 - 20, height() - 20 - 38);
}

void PlaylistWindow::resizeEvent(QResizeEvent *event) {
    QWidget::resizeEvent(event);
    updateListGeometry();
}

bool PlaylistWindow::eventFilter(QObject *obj, QEvent *event) {
    if (obj == listWidget) {
        // Right-click on list widget → show context menu
        if (event->type() == QEvent::ContextMenu) {
            QContextMenuEvent *ce = static_cast<QContextMenuEvent*>(event);
            showContextMenu(ce->globalPos());
            return true;
        }
        if (event->type() == QEvent::KeyPress) {
        QKeyEvent *ke = static_cast<QKeyEvent*>(event);
        Qt::KeyboardModifiers mods = ke->modifiers();
        int key = ke->key();

        // Delete — remove selected
        if (key == Qt::Key_Delete && mods == Qt::NoModifier) {
            removeSelected(); return true;
        }
        // Ctrl+Delete — crop to selected
        if (key == Qt::Key_Delete && mods == Qt::ControlModifier) {
            cropSelected(); return true;
        }
        // Ctrl+Shift+Delete — clear playlist
        if (key == Qt::Key_Delete && mods == (Qt::ControlModifier | Qt::ShiftModifier)) {
            clearPlaylist(); return true;
        }
        // Alt+Delete — remove missing/dead files
        if (key == Qt::Key_Delete && mods == Qt::AltModifier) {
            removeDeadFiles(); return true;
        }
        // Enter — play selected
        if (key == Qt::Key_Return || key == Qt::Key_Enter) {
            int row = listWidget->currentRow();
            if (row >= 0 && row < tracks.size())
                emit trackDoubleClicked(tracks[row]);
            return true;
        }
        // Ctrl+A — select all
        if (key == Qt::Key_A && mods == Qt::ControlModifier) {
            listWidget->selectAll(); return true;
        }
        // Ctrl+I — invert selection
        if (key == Qt::Key_I && mods == Qt::ControlModifier) {
            for (int i = 0; i < listWidget->count(); i++)
                listWidget->item(i)->setSelected(!listWidget->item(i)->isSelected());
            return true;
        }
        // Ctrl+R — reverse list
        if (key == Qt::Key_R && mods == Qt::ControlModifier) {
            reverseList(); return true;
        }
        // Ctrl+Shift+R — randomize list
        if (key == Qt::Key_R && mods == (Qt::ControlModifier | Qt::ShiftModifier)) {
            randomizeList(); return true;
        }
        // Ctrl+Shift+1 — sort by title
        if (key == Qt::Key_1 && mods == (Qt::ControlModifier | Qt::ShiftModifier)) {
            sortByTitle(); return true;
        }
        // Ctrl+Shift+2 — sort by filename
        if (key == Qt::Key_2 && mods == (Qt::ControlModifier | Qt::ShiftModifier)) {
            sortByFilename(); return true;
        }
        // Ctrl+Shift+3 — sort by path
        if (key == Qt::Key_3 && mods == (Qt::ControlModifier | Qt::ShiftModifier)) {
            sortByPath(); return true;
        }
        // Alt+Up — move selection up
        if (key == Qt::Key_Up && mods == Qt::AltModifier) {
            moveSelectedUp(); return true;
        }
        // Alt+Down — move selection down
        if (key == Qt::Key_Down && mods == Qt::AltModifier) {
            moveSelectedDown(); return true;
        }
        // Ctrl+F — explore folder
        if (key == Qt::Key_F && mods == Qt::ControlModifier) {
            exploreFolderOfSelected(); return true;
        }
        // Ctrl+N — new playlist
        if (key == Qt::Key_N && mods == Qt::ControlModifier) {
            clearPlaylist(); return true;
        }
        // Ctrl+O — open playlist
        if (key == Qt::Key_O && mods == Qt::ControlModifier) {
            showListMenu(QCursor::pos()); return true;
        }
        // Ctrl+S — save playlist
        if (key == Qt::Key_S && mods == Qt::ControlModifier) {
            // Trigger save directly
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
            return true;
        }
        // Ctrl+Alt+G — generate HTML playlist
        if (key == Qt::Key_G && mods == (Qt::ControlModifier | Qt::AltModifier)) {
            generateHtmlPlaylist(); return true;
        }
        // L — add file(s)
        if (key == Qt::Key_L && mods == Qt::NoModifier) {
            showAddMenu(QCursor::pos()); return true;
        }
        // Home — scroll to top
        if (key == Qt::Key_Home && mods == Qt::NoModifier) {
            listWidget->scrollToTop();
            if (listWidget->count() > 0) listWidget->setCurrentRow(0);
            return true;
        }
        // End — scroll to bottom
        if (key == Qt::Key_End && mods == Qt::NoModifier) {
            listWidget->scrollToBottom();
            if (listWidget->count() > 0) listWidget->setCurrentRow(listWidget->count() - 1);
            return true;
        }
        } // end KeyPress
    } // end obj == listWidget
    return QWidget::eventFilter(obj, event);
}

// ---- Playlist operation helpers (used by menus and keyboard shortcuts) ----

void PlaylistWindow::removeSelected() {
    QList<int> rows;
    for (QListWidgetItem *item : listWidget->selectedItems())
        rows.append(listWidget->row(item));
    std::sort(rows.begin(), rows.end(), std::greater<int>());
    for (int row : rows) {
        if (row >= 0 && row < tracks.size()) {
            tracks.removeAt(row);
            if (row < trackDurations.size())
                trackDurations.removeAt(row);
        }
    }
    rebuildListDisplay();
    updateTotalTimeDisplay();
}

void PlaylistWindow::cropSelected() {
    QList<QListWidgetItem*> selectedItems = listWidget->selectedItems();
    QStringList newTracks;
    QList<qint64> newDurations;
    for (QListWidgetItem *item : selectedItems) {
        int row = listWidget->row(item);
        if (row >= 0 && row < tracks.size()) {
            newTracks.append(tracks[row]);
            newDurations.append(row < trackDurations.size() ? trackDurations[row] : 0);
        }
    }
    tracks = newTracks;
    trackDurations = newDurations;
    rebuildListDisplay();
    updateTotalTimeDisplay();
}

void PlaylistWindow::removeDeadFiles() {
    int removed = 0;
    for (int i = tracks.size() - 1; i >= 0; i--) {
        if (!QFile::exists(tracks[i])) {
            tracks.removeAt(i);
            if (i < trackDurations.size())
                trackDurations.removeAt(i);
            removed++;
        }
    }
    rebuildListDisplay();
    updateTotalTimeDisplay();
}

void PlaylistWindow::moveSelectedUp() {
    QList<int> rows;
    for (QListWidgetItem *item : listWidget->selectedItems())
        rows.append(listWidget->row(item));
    std::sort(rows.begin(), rows.end());
    if (rows.isEmpty() || rows.first() == 0) return;
    for (int row : rows) {
        if (row > 0 && row < tracks.size()) {
            tracks.swapItemsAt(row, row - 1);
            if (row < trackDurations.size() && row - 1 < trackDurations.size())
                trackDurations.swapItemsAt(row, row - 1);
        }
    }
    rebuildListDisplay();
    // Re-select moved items
    for (int row : rows) {
        if (row - 1 >= 0 && row - 1 < listWidget->count())
            listWidget->item(row - 1)->setSelected(true);
    }
    listWidget->setCurrentRow(rows.first() - 1);
}

void PlaylistWindow::moveSelectedDown() {
    QList<int> rows;
    for (QListWidgetItem *item : listWidget->selectedItems())
        rows.append(listWidget->row(item));
    std::sort(rows.begin(), rows.end(), std::greater<int>());
    if (rows.isEmpty() || rows.first() >= tracks.size() - 1) return;
    for (int row : rows) {
        if (row >= 0 && row + 1 < tracks.size()) {
            tracks.swapItemsAt(row, row + 1);
            if (row < trackDurations.size() && row + 1 < trackDurations.size())
                trackDurations.swapItemsAt(row, row + 1);
        }
    }
    rebuildListDisplay();
    for (int row : rows) {
        if (row + 1 < listWidget->count())
            listWidget->item(row + 1)->setSelected(true);
    }
    std::sort(rows.begin(), rows.end());
    listWidget->setCurrentRow(rows.first() + 1);
}

void PlaylistWindow::sortByTitle() {
    // Sort by display filename (baseName)
    QList<QPair<QString, qint64>> combined;
    for (int i = 0; i < tracks.size(); i++)
        combined.append({tracks[i], i < trackDurations.size() ? trackDurations[i] : 0});
    std::sort(combined.begin(), combined.end(), [](const auto &a, const auto &b) {
        return QFileInfo(a.first).baseName().toLower() < QFileInfo(b.first).baseName().toLower();
    });
    tracks.clear(); trackDurations.clear();
    for (const auto &p : combined) { tracks.append(p.first); trackDurations.append(p.second); }
    rebuildListDisplay();
    updateTotalTimeDisplay();
}

void PlaylistWindow::sortByFilename() {
    QList<QPair<QString, qint64>> combined;
    for (int i = 0; i < tracks.size(); i++)
        combined.append({tracks[i], i < trackDurations.size() ? trackDurations[i] : 0});
    std::sort(combined.begin(), combined.end(), [](const auto &a, const auto &b) {
        return QFileInfo(a.first).fileName().toLower() < QFileInfo(b.first).fileName().toLower();
    });
    tracks.clear(); trackDurations.clear();
    for (const auto &p : combined) { tracks.append(p.first); trackDurations.append(p.second); }
    rebuildListDisplay();
    updateTotalTimeDisplay();
}

void PlaylistWindow::sortByPath() {
    QList<QPair<QString, qint64>> combined;
    for (int i = 0; i < tracks.size(); i++)
        combined.append({tracks[i], i < trackDurations.size() ? trackDurations[i] : 0});
    std::sort(combined.begin(), combined.end(), [](const auto &a, const auto &b) {
        return a.first.toLower() < b.first.toLower();
    });
    tracks.clear(); trackDurations.clear();
    for (const auto &p : combined) { tracks.append(p.first); trackDurations.append(p.second); }
    rebuildListDisplay();
    updateTotalTimeDisplay();
}

void PlaylistWindow::reverseList() {
    std::reverse(tracks.begin(), tracks.end());
    std::reverse(trackDurations.begin(), trackDurations.end());
    rebuildListDisplay();
    updateTotalTimeDisplay();
}

void PlaylistWindow::randomizeList() {
    QList<QPair<QString, qint64>> combined;
    for (int i = 0; i < tracks.size(); i++)
        combined.append({tracks[i], i < trackDurations.size() ? trackDurations[i] : 0});
    std::random_device rd;
    std::mt19937 g(rd());
    std::shuffle(combined.begin(), combined.end(), g);
    tracks.clear(); trackDurations.clear();
    for (const auto &p : combined) { tracks.append(p.first); trackDurations.append(p.second); }
    rebuildListDisplay();
    updateTotalTimeDisplay();
}

void PlaylistWindow::exploreFolderOfSelected() {
    int row = listWidget->currentRow();
    if (row >= 0 && row < tracks.size()) {
        QString folder = QFileInfo(tracks[row]).absolutePath();
        QDesktopServices::openUrl(QUrl::fromLocalFile(folder));
    }
}

void PlaylistWindow::generateHtmlPlaylist() {
    QString fileName = QFileDialog::getSaveFileName(this, "Generate HTML Playlist", "",
        "HTML Files (*.html);;All Files (*)");
    if (fileName.isEmpty()) return;
    QFile file(fileName);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) return;
    QTextStream out(&file);
    out << "<html><head><title>Winamp Generated Playlist</title>\n";
    out << "<style>body{background:#000;color:#0f0;font-family:monospace;}";
    out << "table{border-collapse:collapse;width:100%;}";
    out << "td{padding:2px 8px;border-bottom:1px solid #333;}";
    out << "tr:hover{background:#003;}h1{color:#0f0;}</style></head>\n";
    out << "<body><h1>Winamp Playlist</h1>\n";
    out << "<p>" << tracks.size() << " tracks</p>\n";
    out << "<table><tr><th>#</th><th>Title</th><th>File</th></tr>\n";
    for (int i = 0; i < tracks.size(); i++) {
        out << "<tr><td>" << (i+1) << "</td><td>" 
            << QFileInfo(tracks[i]).baseName().toHtmlEscaped() 
            << "</td><td>" << tracks[i].toHtmlEscaped() << "</td></tr>\n";
    }
    out << "</table></body></html>\n";
    file.close();
    // Open it in default browser
    QDesktopServices::openUrl(QUrl::fromLocalFile(fileName));
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
        QListWidgetItem *item = new QListWidgetItem(trackDisplayName(tracks.size(), filePath));
        item->setData(Qt::UserRole, filePath); // Store full file path for drag-out
        listWidget->addItem(item);
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
    
    // --- Scrollbar track and thumb (right side) ---
    // Matching Windows draw_pe_vslide() at draw_pe.cpp line 213
    int scrollX = w - 15;
    int scrollTop = bodyTop;
    int scrollBottom = bodyBottom;
    int trackH = scrollBottom - scrollTop;
    int thumbH = 18;
    
    // Draw scrollbar track background (already tiled in border loop above)
    // Calculate thumb position based on QListWidget scroll position
    int thumbY = scrollTop;
    if (listWidget && listWidget->count() > 0) {
        QScrollBar *vsb = listWidget->verticalScrollBar();
        if (vsb && vsb->maximum() > 0) {
            // Calculate thumb position: map scrollbar value to track position
            int scrollRange = vsb->maximum() - vsb->minimum();
            int thumbRange = trackH - thumbH;
            if (scrollRange > 0 && thumbRange > 0) {
                thumbY = scrollTop + (vsb->value() * thumbRange) / scrollRange;
            }
        }
    }
    
    // Draw thumb sprite (8x18 at Pledit.bmp (52,53) normal, (61,53) pressed)
    painter.drawPixmap(scrollX, thumbY, bmp.pledit, 52, 53, 8, thumbH);
    
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
    
    // --- Bottom buttons (ADD/REM/SEL/MISC/LIST) from Pledit.bmp ---
    // Matches Windows draw_pe.cpp button drawing - show normal state only
    // (Popup menu states are rendered dynamically when clicked, not in paintEvent)
    int btnY = h - 30;
    // ADD button at x=14 - show "file" normal state
    painter.drawPixmap(14, btnY, bmp.pledit, 0, 149, 22, 18);
    
    // REM button at x=43 - show "remove sel" normal state
    painter.drawPixmap(43, btnY, bmp.pledit, 54, 149, 22, 18);
    
    // SEL button at x=72 - show "all" normal state
    painter.drawPixmap(72, btnY, bmp.pledit, 104, 149, 22, 18);
    
    // MISC button at x=101 - show "info" normal state
    painter.drawPixmap(101, btnY, bmp.pledit, 154, 149, 22, 18);
    
    // LIST/FILE button at x=width-44 - show "load" normal state
    painter.drawPixmap(w - 44, btnY, bmp.pledit, 204, 149, 22, 18);
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

    // Right-click on playlist items shows context menu
    if (event->button() == Qt::RightButton) {
        // Bottom buttons still respond to right click (matching drawn button positions)
        if (y >= h - 30 && y < h - 12) {
            if (x >= 14 && x < 36) { showAddMenu(event->globalPosition().toPoint()); return; }
            if (x >= 43 && x < 65) { showRemMenu(event->globalPosition().toPoint()); return; }
            if (x >= 72 && x < 94) { showSelMenu(event->globalPosition().toPoint()); return; }
            if (x >= 101 && x < 123) { showMiscMenu(event->globalPosition().toPoint()); return; }
            if (x >= width() - 44 && x < width() - 22) { showListMenu(event->globalPosition().toPoint()); return; }
        }
        // Right-click on list area shows the full context menu
        if (y >= 20 && y < h - 38) {
            showContextMenu(event->globalPosition().toPoint());
            event->accept();
            return;
        }
        return;
    }

    // Left-click bottom buttons (matching drawn positions from draw_pe.cpp)
    if (y >= h - 30 && y < h - 12) {
        if (x >= 14 && x < 36) {
            showAddMenu(event->globalPosition().toPoint());
            event->accept();
            return;
        } else if (x >= 43 && x < 65) {
            showRemMenu(event->globalPosition().toPoint());
            event->accept();
            return;
        } else if (x >= 72 && x < 94) {
            showSelMenu(event->globalPosition().toPoint());
            event->accept();
            return;
        } else if (x >= 101 && x < 123) {
            showMiscMenu(event->globalPosition().toPoint());
            event->accept();
            return;
        } else if (x >= width() - 44 && x < width() - 22) {
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
        
        // Scrollbar drag: x=[width-15, width-7], y=[20, height-38]
        int scrollX = width() - 15;
        int bodyTop = 20;
        int bodyBottom = height() - 38;
        if (x >= scrollX && x < scrollX + 8 && y >= bodyTop && y < bodyBottom) {
            isDraggingScrollbar = true;
            // Calculate scroll position from mouse Y
            if (listWidget && listWidget->count() > 0) {
                QScrollBar *vsb = listWidget->verticalScrollBar();
                if (vsb && vsb->maximum() > 0) {
                    int trackH = bodyBottom - bodyTop;
                    int thumbH = 18;
                    int thumbRange = trackH - thumbH;
                    int clickY = y - bodyTop;
                    int scrollValue = (clickY * vsb->maximum()) / thumbRange;
                    vsb->setValue(qBound(0, scrollValue, vsb->maximum()));
                    update();
                }
            }
            event->accept();
            return;
        }
        
        // Check for resize edges
        if (!shadeMode) {
            ResizeEdge edge = hitTestResize(event->pos());
            if (edge != NoEdge) {
                isResizing = true;
                resizeEdge = edge;
                resizeStartPos = event->globalPosition().toPoint();
                resizeStartSize = size();
                event->accept();
                return;
            }
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
    // Handle resizing
    if (isResizing) {
        QPoint delta = event->globalPosition().toPoint() - resizeStartPos;
        QSize newSize = resizeStartSize;
        if (resizeEdge & RightEdge)
            newSize.setWidth(qMax(275, resizeStartSize.width() + delta.x()));
        if (resizeEdge & BottomEdge)
            newSize.setHeight(qMax(116, resizeStartSize.height() + delta.y()));
        resize(newSize);
        return;
    }
    
    // Handle scrollbar dragging
    if (isDraggingScrollbar) {
        int y = event->position().y();
        int bodyTop = 20;
        int bodyBottom = height() - 38;
        if (listWidget && listWidget->count() > 0) {
            QScrollBar *vsb = listWidget->verticalScrollBar();
            if (vsb && vsb->maximum() > 0) {
                int trackH = bodyBottom - bodyTop;
                int thumbH = 18;
                int thumbRange = trackH - thumbH;
                int dragY = y - bodyTop;
                int scrollValue = (dragY * vsb->maximum()) / thumbRange;
                vsb->setValue(qBound(0, scrollValue, vsb->maximum()));
                update();
            }
        }
        return;
    }
    
    // Update cursor for resize edges
    if (!isDragging && !shadeMode) {
        ResizeEdge edge = hitTestResize(event->pos());
        if (edge == BottomRight)
            setCursor(Qt::SizeFDiagCursor);
        else if (edge == RightEdge)
            setCursor(Qt::SizeHorCursor);
        else if (edge == BottomEdge)
            setCursor(Qt::SizeVerCursor);
        else
            setCursor(Qt::ArrowCursor);
    }
    
    if (isDragging) {
        move(event->globalPosition().toPoint() - dragPosition);
        checkSnap();
    }
}

void PlaylistWindow::mouseReleaseEvent(QMouseEvent *event) {
    Q_UNUSED(event);
    isDragging = false;
    isResizing = false;
    isDraggingScrollbar = false;
    resizeEdge = NoEdge;
}

void PlaylistWindow::mouseDoubleClickEvent(QMouseEvent *event) {
    // Double-click on titlebar toggles shade mode
    if (event->pos().y() < 20) {
        toggleShadeMode();
        return;
    }
    QWidget::mouseDoubleClickEvent(event);
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
            listWidget->addItem(trackDisplayName(tracks.size(), trimmed));
            tracks.append(trimmed);
            trackDurations.append(0); // Duration will be 0 until played
        }
    }
    updateTotalTimeDisplay();
}

// Right-click context menu on playlist items (matches Windows Winamp)
void PlaylistWindow::showContextMenu(QPoint globalPos) {
    static const char *menuStyle =
        "QMenu { background-color: #2b2d3d; color: #00ff00; border: 1px solid #555; font-size: 9pt; }"
        "QMenu::item:selected { background-color: #0000c6; }"
        "QMenu::item:disabled { color: #666; }"
        "QMenu::separator { height: 1px; background: #555; margin: 2px 4px; }";

    QMenu menu;
    menu.setStyleSheet(menuStyle);

    bool hasSelection = !listWidget->selectedItems().isEmpty();

    QAction *playAct = menu.addAction("Play item(s)");
    playAct->setShortcut(QKeySequence(Qt::Key_Return));
    playAct->setEnabled(hasSelection);
    menu.addSeparator();

    QAction *removeAct = menu.addAction("Remove item(s)");
    removeAct->setShortcut(QKeySequence(Qt::Key_Delete));
    removeAct->setEnabled(hasSelection);
    QAction *cropAct = menu.addAction("Crop files");
    cropAct->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_Delete));
    cropAct->setEnabled(hasSelection);
    menu.addSeparator();

    QAction *fileInfoAct = menu.addAction("View file info...");
    fileInfoAct->setShortcut(QKeySequence(Qt::ALT | Qt::Key_3));
    fileInfoAct->setEnabled(hasSelection);
    QAction *editEntryAct = menu.addAction("Playlist entry...");
    editEntryAct->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_E));
    editEntryAct->setEnabled(hasSelection);
    menu.addSeparator();

    // Sort submenu
    QMenu *sortMenu = menu.addMenu("Sort");
    sortMenu->setStyleSheet(menuStyle);
    QAction *sortTitleAct = sortMenu->addAction("Sort list by title");
    sortTitleAct->setShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_1));
    QAction *sortFilenameAct = sortMenu->addAction("Sort list by filename");
    sortFilenameAct->setShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_2));
    QAction *sortPathAct = sortMenu->addAction("Sort list by path + filename");
    sortPathAct->setShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_3));
    sortMenu->addSeparator();
    QAction *reverseAct = sortMenu->addAction("Reverse list");
    reverseAct->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_R));
    QAction *randomizeAct = sortMenu->addAction("Randomize list");
    randomizeAct->setShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_R));

    menu.addSeparator();

    QAction *exploreAct = menu.addAction("Explore item folder");
    exploreAct->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_F));
    exploreAct->setEnabled(hasSelection);

    QAction *moveUpAct = menu.addAction("Move up");
    moveUpAct->setShortcut(QKeySequence(Qt::ALT | Qt::Key_Up));
    moveUpAct->setEnabled(hasSelection);
    QAction *moveDownAct = menu.addAction("Move down");
    moveDownAct->setShortcut(QKeySequence(Qt::ALT | Qt::Key_Down));
    moveDownAct->setEnabled(hasSelection);

    menu.addSeparator();

    QAction *selAllAct = menu.addAction("Select all");
    selAllAct->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_A));
    QAction *selNoneAct = menu.addAction("Select none");
    QAction *selInvAct = menu.addAction("Invert selection");
    selInvAct->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_I));

    QAction *sel = menu.exec(globalPos);
    if (!sel) return;

    if (sel == playAct) {
        int row = listWidget->currentRow();
        if (row >= 0 && row < tracks.size())
            emit trackDoubleClicked(tracks[row]);
    }
    else if (sel == removeAct) removeSelected();
    else if (sel == cropAct) cropSelected();
    else if (sel == fileInfoAct) {
        // Show basic file info dialog
        int row = listWidget->currentRow();
        if (row >= 0 && row < tracks.size()) {
            QFileInfo fi(tracks[row]);
            QString info = QString("File: %1\nPath: %2\nSize: %3 KB\nModified: %4")
                .arg(fi.fileName())
                .arg(fi.absolutePath())
                .arg(fi.size() / 1024)
                .arg(fi.lastModified().toString("yyyy-MM-dd hh:mm:ss"));
            QMessageBox::information(this, "File Info", info);
        }
    }
    else if (sel == editEntryAct) {
        int row = listWidget->currentRow();
        if (row >= 0 && row < tracks.size()) {
            bool ok;
            QString newPath = QInputDialog::getText(this, "Edit Playlist Entry",
                "File path:", QLineEdit::Normal, tracks[row], &ok);
            if (ok && !newPath.isEmpty()) {
                tracks[row] = newPath;
                rebuildListDisplay();
            }
        }
    }
    else if (sel == sortTitleAct) sortByTitle();
    else if (sel == sortFilenameAct) sortByFilename();
    else if (sel == sortPathAct) sortByPath();
    else if (sel == reverseAct) reverseList();
    else if (sel == randomizeAct) randomizeList();
    else if (sel == exploreAct) exploreFolderOfSelected();
    else if (sel == moveUpAct) moveSelectedUp();
    else if (sel == moveDownAct) moveSelectedDown();
    else if (sel == selAllAct) listWidget->selectAll();
    else if (sel == selNoneAct) listWidget->clearSelection();
    else if (sel == selInvAct) {
        for (int i = 0; i < listWidget->count(); i++)
            listWidget->item(i)->setSelected(!listWidget->item(i)->isSelected());
    }
}

void PlaylistWindow::showAddMenu(QPoint globalPos) {
    QMenu menu;
    menu.setStyleSheet(
        "QMenu { background-color: #2b2d3d; color: #00ff00; border: 1px solid #555; font-size: 9pt; }"
        "QMenu::item:selected { background-color: #0000c6; }"
    );
    
    QAction *addFiles = menu.addAction("Add file(s)\tL");
    QAction *addDir = menu.addAction("Add folder\tShift+L");
    QAction *addUrl = menu.addAction("Add URL\tCtrl+L");
    
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
            QStringList files = directory.entryList(filters, QDir::Files, QDir::Name);
            for (const QString &file : files) {
                addTrack(directory.absoluteFilePath(file));
            }
        }
    } else if (selected == addUrl) {
        bool ok;
        QString url = QInputDialog::getText(this, "Add URL",
            "Enter URL:", QLineEdit::Normal, "http://", &ok);
        if (ok && !url.isEmpty()) {
            // Add URL as a track (QMediaPlayer can handle remote URLs)
            listWidget->addItem(trackDisplayName(tracks.size(), url));
            tracks.append(url);
            trackDurations.append(0);
            updateTotalTimeDisplay();
        }
    }
}

void PlaylistWindow::showRemMenu(QPoint globalPos) {
    QMenu menu;
    menu.setStyleSheet(
        "QMenu { background-color: #2b2d3d; color: #00ff00; border: 1px solid #555; font-size: 9pt; }"
        "QMenu::item:selected { background-color: #0000c6; }"
        "QMenu::separator { height: 1px; background: #555; margin: 2px 4px; }"
    );
    
    QAction *removeSel = menu.addAction("Remove selected\tDel");
    QAction *crop = menu.addAction("Crop selected\tCtrl+Del");
    QAction *clear = menu.addAction("Clear playlist\tCtrl+Shift+Del");
    menu.addSeparator();
    QAction *removeDead = menu.addAction("Remove missing files\tAlt+Del");
    QAction *removeDupes = menu.addAction("Remove duplicates");
    
    QAction *selected = menu.exec(globalPos);
    if (selected == removeSel) removeSelected();
    else if (selected == crop) cropSelected();
    else if (selected == clear) clearPlaylist();
    else if (selected == removeDead) removeDeadFiles();
    else if (selected == removeDupes) {
        // Remove duplicate entries (keep first occurrence)
        QSet<QString> seen;
        for (int i = tracks.size() - 1; i >= 0; i--) {
            if (seen.contains(tracks[i])) {
                tracks.removeAt(i);
                if (i < trackDurations.size()) trackDurations.removeAt(i);
            } else {
                seen.insert(tracks[i]);
            }
        }
        rebuildListDisplay();
        updateTotalTimeDisplay();
    }
}

void PlaylistWindow::showSelMenu(QPoint globalPos) {
    QMenu menu;
    menu.setStyleSheet(
        "QMenu { background-color: #2b2d3d; color: #00ff00; border: 1px solid #555; font-size: 9pt; }"
        "QMenu::item:selected { background-color: #0000c6; }"
    );
    
    QAction *selectAll = menu.addAction("Select all\tCtrl+A");
    QAction *selectNone = menu.addAction("Select none");
    QAction *invertSel = menu.addAction("Invert selection\tCtrl+I");
    
    QAction *selected = menu.exec(globalPos);
    if (selected == selectAll) {
        listWidget->selectAll();
    } else if (selected == selectNone) {
        listWidget->clearSelection();
    } else if (selected == invertSel) {
        for (int i = 0; i < listWidget->count(); i++)
            listWidget->item(i)->setSelected(!listWidget->item(i)->isSelected());
    }
}

void PlaylistWindow::showMiscMenu(QPoint globalPos) {
    static const char *menuStyle =
        "QMenu { background-color: #2b2d3d; color: #00ff00; border: 1px solid #555; font-size: 9pt; }"
        "QMenu::item:selected { background-color: #0000c6; }"
        "QMenu::separator { height: 1px; background: #555; margin: 2px 4px; }";

    QMenu menu;
    menu.setStyleSheet(menuStyle);

    // File Info submenu (matches Windows MISC → File info)
    QMenu *fileInfoMenu = menu.addMenu("File info");
    fileInfoMenu->setStyleSheet(menuStyle);
    QAction *fileInfoAct = fileInfoMenu->addAction("View file info...\tAlt+3");
    QAction *editEntryAct = fileInfoMenu->addAction("Playlist entry...\tCtrl+E");

    // Sort submenu (matches Windows MISC → Sort)
    QMenu *sortMenu = menu.addMenu("Sort");
    sortMenu->setStyleSheet(menuStyle);
    QAction *sortTitleAct = sortMenu->addAction("Sort list by title\tCtrl+Shift+1");
    QAction *sortFilenameAct = sortMenu->addAction("Sort list by filename\tCtrl+Shift+2");
    QAction *sortPathAct = sortMenu->addAction("Sort list by path + filename\tCtrl+Shift+3");
    sortMenu->addSeparator();
    QAction *reverseAct = sortMenu->addAction("Reverse list\tCtrl+R");
    QAction *randomizeAct = sortMenu->addAction("Randomize list\tCtrl+Shift+R");

    // Misc submenu (matches Windows MISC → Misc)
    QMenu *miscMenu = menu.addMenu("Misc");
    miscMenu->setStyleSheet(menuStyle);
    QAction *htmlAct = miscMenu->addAction("Generate HTML playlist...\tCtrl+Alt+G");
    miscMenu->addSeparator();
    QAction *moveUpAct = miscMenu->addAction("Move selected up\tAlt+Up");
    QAction *moveDownAct = miscMenu->addAction("Move selected down\tAlt+Down");
    miscMenu->addSeparator();
    QAction *exploreAct = miscMenu->addAction("Explore item folder\tCtrl+F");

    QAction *selected = menu.exec(globalPos);
    if (!selected) return;

    if (selected == fileInfoAct) {
        int row = listWidget->currentRow();
        if (row >= 0 && row < tracks.size()) {
            QFileInfo fi(tracks[row]);
            QString info = QString("File: %1\nPath: %2\nSize: %3 KB\nModified: %4")
                .arg(fi.fileName()).arg(fi.absolutePath())
                .arg(fi.size() / 1024).arg(fi.lastModified().toString("yyyy-MM-dd hh:mm:ss"));
            QMessageBox::information(this, "File Info", info);
        }
    }
    else if (selected == editEntryAct) {
        int row = listWidget->currentRow();
        if (row >= 0 && row < tracks.size()) {
            bool ok;
            QString newPath = QInputDialog::getText(this, "Edit Playlist Entry",
                "File path:", QLineEdit::Normal, tracks[row], &ok);
            if (ok && !newPath.isEmpty()) {
                tracks[row] = newPath;
                rebuildListDisplay();
            }
        }
    }
    else if (selected == sortTitleAct) sortByTitle();
    else if (selected == sortFilenameAct) sortByFilename();
    else if (selected == sortPathAct) sortByPath();
    else if (selected == reverseAct) this->reverseList();
    else if (selected == randomizeAct) this->randomizeList();
    else if (selected == htmlAct) generateHtmlPlaylist();
    else if (selected == moveUpAct) moveSelectedUp();
    else if (selected == moveDownAct) moveSelectedDown();
    else if (selected == exploreAct) exploreFolderOfSelected();
}

void PlaylistWindow::showListMenu(QPoint globalPos) {
    QMenu menu;
    menu.setStyleSheet(
        "QMenu { background-color: #2b2d3d; color: #00ff00; border: 1px solid #555; font-size: 9pt; }"
        "QMenu::item:selected { background-color: #0000c6; }"
    );

    QAction *newPl = menu.addAction("New playlist\tCtrl+N");
    QAction *openPl = menu.addAction("Open playlist...\tCtrl+O");
    QAction *savePl = menu.addAction("Save playlist...\tCtrl+S");
    menu.addSeparator();
    QAction *genPl = menu.addAction("Generate playlist...");

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
    } else if (selected == genPl) {
        // Show playlist generator dialog
        QDialog dialog(this);
        dialog.setWindowTitle(TR("win.plgen.title", "Playlist Generator"));
        dialog.setModal(true);
        dialog.resize(350, 200);
        dialog.setStyleSheet(
            "QDialog { background-color: #2b2d3d; color: #00ff00; }"
            "QLabel { color: #00ff00; }"
            "QPushButton { background-color: #1a1c2a; color: #00ff00; border: 1px solid #555; padding: 5px 15px; }"
            "QPushButton:hover { background-color: #0000c6; }"
            "QSpinBox { background-color: #1a1c2a; color: #00ff00; border: 1px solid #555; }"
            "QCheckBox { color: #00ff00; }"
        );
        
        QVBoxLayout *layout = new QVBoxLayout(&dialog);
        
        // Number of tracks
        QHBoxLayout *countLayout = new QHBoxLayout();
        QLabel *countLabel = new QLabel(TR("plgen.numtracks", "Number of tracks:"));
        QSpinBox *countSpin = new QSpinBox();
        countSpin->setMinimum(1);
        countSpin->setMaximum(1000);
        countSpin->setValue(50);
        countLayout->addWidget(countLabel);
        countLayout->addWidget(countSpin);
        countLayout->addStretch();
        layout->addLayout(countLayout);
        
        // Replace or add option
        QCheckBox *replaceCheck = new QCheckBox(TR("plgen.replace", "Replace current playlist (otherwise add to current)"));
        replaceCheck->setChecked(false);
        layout->addWidget(replaceCheck);
        
        layout->addStretch();
        
        // Buttons
        QHBoxLayout *buttonLayout = new QHBoxLayout();
        buttonLayout->addStretch();
        QPushButton *okBtn = new QPushButton(TR("button.generate", "Generate"));
        QPushButton *cancelBtn = new QPushButton(TR("button.cancel", "Cancel"));
        buttonLayout->addWidget(okBtn);
        buttonLayout->addWidget(cancelBtn);
        layout->addLayout(buttonLayout);
        
        connect(okBtn, &QPushButton::clicked, &dialog, &QDialog::accept);
        connect(cancelBtn, &QPushButton::clicked, &dialog, &QDialog::reject);
        
        if (dialog.exec() == QDialog::Accepted) {
            int numTracks = countSpin->value();
            bool replace = replaceCheck->isChecked();
            
            if (replace) {
                clearPlaylist();
            }
            
            // Scan music directory for all audio files
            QStringList allFiles;
            QString musicPath = QStandardPaths::writableLocation(QStandardPaths::MusicLocation);
            if (musicPath.isEmpty() || !QDir(musicPath).exists()) {
                musicPath = QDir::homePath();
            }
            
            QStringList queue;
            queue << musicPath;
            QStringList filters;
            filters << "*.mp3" << "*.flac" << "*.ogg" << "*.wav" << "*.m4a" 
                   << "*.aac" << "*.wma" << "*.opus";
            
            while (!queue.isEmpty() && allFiles.size() < numTracks * 10) {
                QString currentDir = queue.takeFirst();
                QDir dir(currentDir);
                
                // Add audio files
                QFileInfoList files = dir.entryInfoList(filters, QDir::Files);
                for (const QFileInfo &file : files) {
                    allFiles << file.absoluteFilePath();
                }
                
                // Add subdirectories to queue
                QFileInfoList subdirs = dir.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot);
                for (const QFileInfo &subdir : subdirs) {
                    queue << subdir.absoluteFilePath();
                }
            }
            
            if (allFiles.isEmpty()) {
                QMessageBox::information(this, "Playlist Generator", 
                    "No audio files found in " + musicPath);
                return;
            }
            
            // Randomly select tracks
            std::random_device rd;
            std::mt19937 g(rd());
            std::shuffle(allFiles.begin(), allFiles.end(), g);
            
            int tracksToAdd = qMin(numTracks, allFiles.size());
            for (int i = 0; i < tracksToAdd; i++) {
                addTrack(allFiles[i]);
            }
        }
    }
}

// Equalizer Window Constructor
EqualizerWindow::EqualizerWindow(WinampWindow *parent) : QWidget(nullptr), mainWindow(parent) {
    setFixedSize(275, 116);
    setWindowTitle(TR("win.equalizer.title", "Winamp Equalizer"));
    setWindowFlags(Qt::Tool | Qt::FramelessWindowHint);
    
    // Initialize EQ bands to center position (32 out of 63)
    for (int i = 0; i < 10; i++) {
        eqValues[i] = 32;
    }
    preampValue = 32;
}

// ============================================================
// VideoWindow — Video playback window with skin support
// ============================================================
class VideoWindow : public QWidget {
    Q_OBJECT
    
public:
    VideoWindow(QWidget *parent = nullptr) : QWidget(parent) {
        setWindowTitle(TR("win.video.title", "Winamp Video"));
        setWindowFlags(Qt::Window | Qt::FramelessWindowHint);
        setAttribute(Qt::WA_DeleteOnClose, false);  // Don't delete on close, just hide
        resize(320, 240);
        setMinimumSize(160, 120);
        setFocusPolicy(Qt::StrongFocus);  // Accept keyboard focus for F/Escape keys
        setMouseTracking(true);  // Enable cursor updates for resize edges
        
        // Create video widget for actual video rendering — inset by resizeMargin
        // so the parent window's edges are exposed for resize mouse events
        videoWidget = new QVideoWidget(this);
        const int m = resizeMargin;
        videoWidget->setGeometry(m, m, 320 - 2 * m, 240 - 2 * m);
        videoWidget->setStyleSheet("background-color: black;");
        
        // Load logo bitmap
        loadLogo();
        
        isDragging = false;
        isResizing = false;
        wasFullscreen = false;
        resizeEdge = NoEdge;
    }
    
    QVideoWidget* getVideoWidget() { return videoWidget; }
    
    void setHasVideo(bool has) {
        hasActiveVideo = has;
        if (has) {
            videoWidget->show();
        } else {
            videoWidget->hide();
        }
        update();
    }
    
signals:
    void fullscreenChanged(bool isFullscreen);
    
protected:
    void paintEvent(QPaintEvent*) override {
        QPainter p(this);
        p.fillRect(rect(), Qt::black);  // Fill border area black
        
        // If no video is playing, show the logo
        if (!hasActiveVideo && !logoPixmap.isNull()) {
            QRect r = rect();
            int xp = (r.width() - logoPixmap.width()) / 2;
            int yp = (r.height() - logoPixmap.height()) / 2;
            p.drawPixmap(xp, yp, logoPixmap);
        }
    }
    
    void resizeEvent(QResizeEvent*) override {
        // Leave a resize margin around the video widget so edges receive mouse events
        // In fullscreen, fill the entire window
        if (isFullScreen()) {
            videoWidget->setGeometry(0, 0, width(), height());
        } else {
            const int m = resizeMargin;
            videoWidget->setGeometry(m, m, width() - 2 * m, height() - 2 * m);
        }
    }
    
    void showEvent(QShowEvent*) override {
        // Track fullscreen state when shown
        bool nowFullscreen = isFullScreen();
        if (nowFullscreen != wasFullscreen) {
            wasFullscreen = nowFullscreen;
            emit fullscreenChanged(nowFullscreen);
        }
    }
    
    void keyPressEvent(QKeyEvent *event) override {
        // F or F11 toggles fullscreen, Escape exits fullscreen or closes window
        if (event->key() == Qt::Key_F || event->key() == Qt::Key_F11) {
            toggleFullscreen();
        } else if (event->key() == Qt::Key_Escape) {
            if (isFullScreen()) exitFullscreen();
            else hide();
        } else {
            QWidget::keyPressEvent(event);
        }
    }
    
    void mousePressEvent(QMouseEvent *event) override {
        if (event->button() == Qt::LeftButton && !isFullScreen()) {
            // Check for resize edges first
            ResizeEdge edge = hitTestResize(event->pos());
            if (edge != NoEdge) {
                isResizing = true;
                resizeEdge = edge;
                resizeStartPos = event->globalPosition().toPoint();
                resizeStartGeometry = geometry();
                event->accept();
                return;
            }
            // Otherwise, start dragging
            isDragging = true;
            dragStartPos = event->globalPosition().toPoint() - frameGeometry().topLeft();
        } else if (event->button() == Qt::RightButton) {
            // Could show context menu here
        }
    }
    
    void mouseMoveEvent(QMouseEvent *event) override {
        if (isFullScreen()) return;
        
        // Handle resizing
        if (isResizing) {
            QPoint delta = event->globalPosition().toPoint() - resizeStartPos;
            QRect newGeom = resizeStartGeometry;
            
            // Adjust geometry based on resize edge
            if (resizeEdge & LeftEdge) {
                int newX = resizeStartGeometry.x() + delta.x();
                int newWidth = resizeStartGeometry.width() - delta.x();
                if (newWidth >= minimumWidth()) {
                    newGeom.setX(newX);
                    newGeom.setWidth(newWidth);
                }
            }
            if (resizeEdge & RightEdge) {
                newGeom.setWidth(qMax(minimumWidth(), resizeStartGeometry.width() + delta.x()));
            }
            if (resizeEdge & TopEdge) {
                int newY = resizeStartGeometry.y() + delta.y();
                int newHeight = resizeStartGeometry.height() - delta.y();
                if (newHeight >= minimumHeight()) {
                    newGeom.setY(newY);
                    newGeom.setHeight(newHeight);
                }
            }
            if (resizeEdge & BottomEdge) {
                newGeom.setHeight(qMax(minimumHeight(), resizeStartGeometry.height() + delta.y()));
            }
            
            setGeometry(newGeom);
            return;
        }
        
        // Handle dragging
        if (isDragging) {
            move(event->globalPosition().toPoint() - dragStartPos);
            return;
        }
        
        // Update cursor for resize edges
        ResizeEdge edge = hitTestResize(event->pos());
        updateCursorForEdge(edge);
    }
    
    void mouseReleaseEvent(QMouseEvent *event) override {
        if (event->button() == Qt::LeftButton) {
            isDragging = false;
            isResizing = false;
            resizeEdge = NoEdge;
        }
    }
    
    void mouseDoubleClickEvent(QMouseEvent*) override {
        toggleFullscreen();
    }
    
private:
    // Resize edge detection (all 8 resize points: 4 corners + 4 edges)
    enum ResizeEdge {
        NoEdge = 0,
        LeftEdge = 1,
        RightEdge = 2,
        TopEdge = 4,
        BottomEdge = 8,
        TopLeft = TopEdge | LeftEdge,
        TopRight = TopEdge | RightEdge,
        BottomLeft = BottomEdge | LeftEdge,
        BottomRight = BottomEdge | RightEdge
    };
    
    ResizeEdge hitTestResize(const QPoint &pos) {
        const int margin = resizeMargin;
        bool atLeft = pos.x() < margin;
        bool atRight = pos.x() >= width() - margin;
        bool atTop = pos.y() < margin;
        bool atBottom = pos.y() >= height() - margin;
        
        int edge = NoEdge;
        if (atLeft) edge |= LeftEdge;
        if (atRight) edge |= RightEdge;
        if (atTop) edge |= TopEdge;
        if (atBottom) edge |= BottomEdge;
        
        return static_cast<ResizeEdge>(edge);
    }
    
    void updateCursorForEdge(ResizeEdge edge) {
        QCursor cursor;
        switch (edge) {
            case TopLeft:
            case BottomRight:
                cursor = Qt::SizeFDiagCursor;
                break;
            case TopRight:
            case BottomLeft:
                cursor = Qt::SizeBDiagCursor;
                break;
            case LeftEdge:
            case RightEdge:
                cursor = Qt::SizeHorCursor;
                break;
            case TopEdge:
            case BottomEdge:
                cursor = Qt::SizeVerCursor;
                break;
            default:
                cursor = Qt::ArrowCursor;
                break;
        }
        setCursor(cursor);
        videoWidget->setCursor(cursor);
    }
    
    void toggleFullscreen() {
        if (isFullScreen()) {
            exitFullscreen();
        } else {
            enterFullscreen();
        }
    }
    
    void enterFullscreen() {
        showFullScreen();
        wasFullscreen = true;
        emit fullscreenChanged(true);
    }
    
    void exitFullscreen() {
        showNormal();
        wasFullscreen = false;
        emit fullscreenChanged(false);
    }
    
    void loadLogo() {
        // Try to load video_logo.bmp from skin
        QStringList paths = {
            "skins/default/video_logo.bmp",
            "Src/Winamp/resource/video_logo.bmp"
        };
        
        for (const QString &path : paths) {
            if (QFile::exists(path)) {
                logoPixmap = QPixmap(path);
                if (!logoPixmap.isNull()) {
                    break;
                }
            }
        }
    }
    
    static constexpr int resizeMargin = 6;  // Exposed edge width for resize grab
    QVideoWidget *videoWidget;
    QPixmap logoPixmap;
    bool hasActiveVideo = false;
    bool isDragging = false;
    bool isResizing = false;
    bool wasFullscreen = false;
    ResizeEdge resizeEdge = NoEdge;
    QPoint dragStartPos;
    QPoint resizeStartPos;
    QRect resizeStartGeometry;
};

// ============================================================
// MilkdropWindow — projectM-powered Milkdrop visualization
// ============================================================
class MilkdropWindow : public QOpenGLWidget, protected QOpenGLFunctions {
    Q_OBJECT
public:
    MilkdropWindow(QWidget *parent = nullptr) : QOpenGLWidget(parent) {
        setWindowTitle("Milkdrop Visualization");
        setWindowFlags(Qt::Window);
        setAttribute(Qt::WA_DeleteOnClose);
        resize(640, 480);
        setMinimumSize(320, 240);
        setFocusPolicy(Qt::StrongFocus);
        
        // Timer driving projectM render at ~33fps
        renderTimer = new QTimer(this);
        connect(renderTimer, &QTimer::timeout, this, QOverload<>::of(&MilkdropWindow::update));
    }
    
    ~MilkdropWindow() override {
        makeCurrent();
        if (pm) { delete pm; pm = nullptr; }
        doneCurrent();
    }
    
    void feedPCMInt16(const qint16 *data, int frames, int channels) {
        if (!pm) return;
        // Convert interleaved int16 stereo to the format projectM expects:
        // addPCM16Data wants interleaved L,R,L,R... shorts
        int n = qMin(frames, 512);
        pm->pcm()->addPCM16Data(data, n);
    }
    
    void feedPCMFloat(const float *data, int frames, int channels) {
        if (!pm) return;
        // Convert interleaved float to mono float for addPCMfloat
        float mono[512];
        int n = qMin(frames, 512);
        for (int i = 0; i < n; i++)
            mono[i] = data[i * channels];
        pm->pcm()->addPCMfloat(mono, n);
    }
    
    void nextPreset() { if (pm) pm->selectNext(true); }
    void prevPreset() { if (pm) pm->selectPrevious(true); }
    void randomPreset() { if (pm) pm->selectRandom(true); }
    void toggleLock() {
        if (!pm) return;
        pm->setPresetLock(!pm->isPresetLocked());
    }
    
protected:
    void initializeGL() override {
        initializeOpenGLFunctions();
        glClearColor(0, 0, 0, 1);
        
        // Create projectM instance from settings struct
        projectM::Settings s;
        s.meshX = 32;
        s.meshY = 24;
        s.fps = 33;
        s.textureSize = 1024;
        s.windowWidth = width();
        s.windowHeight = height();
        s.presetURL = "/usr/share/projectM/presets";
        s.smoothPresetDuration = 5;
        s.presetDuration = 30;
        s.beatSensitivity = 1.0f;
        s.aspectCorrection = true;
        s.easterEgg = 1.0f;
        s.shuffleEnabled = true;
        s.softCutRatingsEnabled = false;
        
        // Find fonts
        std::string fontPath;
        for (auto &candidate : {std::string("/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf"),
                                 std::string("/usr/share/fonts/truetype/ttf-dejavu/DejaVuSans.ttf"),
                                 std::string("/usr/share/fonts/truetype/freefont/FreeSans.ttf")}) {
            if (QFile::exists(QString::fromStdString(candidate))) { fontPath = candidate; break; }
        }
        s.titleFontURL = fontPath;
        s.menuFontURL = fontPath;
        
        try {
            pm = new projectM(s, projectM::FLAG_NONE);
            pm->projectM_resetGL(width(), height());
            pm->projectM_setTitle("");  // Clear the default "projectM" title overlay
            pm->selectRandom(true);
            qDebug() << "Milkdrop (projectM) initialized with" << pm->getPlaylistSize() << "presets";
        } catch (const std::exception &e) {
            qWarning() << "Failed to initialize projectM:" << e.what();
            pm = nullptr;
        } catch (...) {
            qWarning() << "Failed to initialize projectM (unknown error)";
            pm = nullptr;
        }
        
        renderTimer->start(1000 / 33);
    }
    
    void resizeGL(int w, int h) override {
        if (pm) pm->projectM_resetGL(w, h);
    }
    
    void paintGL() override {
        if (pm) {
            pm->renderFrame();
        } else {
            glClear(GL_COLOR_BUFFER_BIT);
        }
    }
    
    void keyPressEvent(QKeyEvent *e) override {
        if (!pm) { QOpenGLWidget::keyPressEvent(e); return; }
        switch (e->key()) {
            case Qt::Key_Space: nextPreset(); break;
            case Qt::Key_Backspace: prevPreset(); break;
            case Qt::Key_L: toggleLock(); break;
            case Qt::Key_R: pm->setShuffleEnabled(!pm->isShuffleEnabled()); break;
            case Qt::Key_F:
            case Qt::Key_F11:
                toggleFullScreen();
                break;
            case Qt::Key_Escape:
                if (isFullScreen()) toggleFullScreen();
                else close();
                break;
            case Qt::Key_Q: close(); break;
            default: QOpenGLWidget::keyPressEvent(e); return;
        }
        e->accept();
    }
    
    void mouseDoubleClickEvent(QMouseEvent *e) override {
        toggleFullScreen();
    }
    
    void showEvent(QShowEvent *e) override {
        QOpenGLWidget::showEvent(e);
        setFocus();
    }
    
    void closeEvent(QCloseEvent *e) override {
        if (isFullScreen()) {
            showNormal();
            emit fullscreenChanged(false);
        }
        renderTimer->stop();
        e->accept();
    }
    
signals:
    void fullscreenChanged(bool fs);
    
private:
    void toggleFullScreen() {
        if (isFullScreen()) {
            showNormal();
            emit fullscreenChanged(false);
        } else {
            showFullScreen();
            emit fullscreenChanged(true);
        }
        setFocus();
    }
    
    projectM *pm = nullptr;
    QTimer *renderTimer;
};

// ============================================================================
// MPRIS2 D-Bus Adaptor — Linux desktop media player integration
// Implements org.mpris.MediaPlayer2 and org.mpris.MediaPlayer2.Player
// This enables media keys (play/pause/next/prev), KDE Connect, panel widgets, etc.
// Matches what Windows Winamp does with its global hotkeys + remote control features
// ============================================================================
#ifdef QT_DBUS_LIB

class Mpris2RootAdaptor : public QDBusAbstractAdaptor {
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.mpris.MediaPlayer2")
    Q_PROPERTY(bool CanQuit READ canQuit)
    Q_PROPERTY(bool CanRaise READ canRaise)
    Q_PROPERTY(bool HasTrackList READ hasTrackList)
    Q_PROPERTY(QString Identity READ identity)
    Q_PROPERTY(QStringList SupportedUriSchemes READ supportedUriSchemes)
    Q_PROPERTY(QStringList SupportedMimeTypes READ supportedMimeTypes)

public:
    explicit Mpris2RootAdaptor(QObject *parent) : QDBusAbstractAdaptor(parent) {}

    bool canQuit() const { return true; }
    bool canRaise() const { return true; }
    bool hasTrackList() const { return false; }
    QString identity() const { return "Winamp"; }
    QStringList supportedUriSchemes() const { return {"file", "http", "https"}; }
    QStringList supportedMimeTypes() const {
        return {"audio/mpeg", "audio/x-wav", "audio/ogg", "audio/flac", "audio/x-m4a", 
                "audio/aac", "audio/opus", "audio/x-ms-wma"};
    }

public slots:
    void Raise() {
        QWidget *w = qobject_cast<QWidget*>(parent());
        if (w) { w->show(); w->raise(); w->activateWindow(); }
    }
    void Quit() { QApplication::quit(); }
};

class Mpris2PlayerAdaptor : public QDBusAbstractAdaptor {
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.mpris.MediaPlayer2.Player")
    Q_PROPERTY(QString PlaybackStatus READ playbackStatus)
    Q_PROPERTY(double Rate READ rate WRITE setRate)
    Q_PROPERTY(QVariantMap Metadata READ metadata)
    Q_PROPERTY(double Volume READ volume WRITE setVolume)
    Q_PROPERTY(qlonglong Position READ position)
    Q_PROPERTY(double MinimumRate READ minimumRate)
    Q_PROPERTY(double MaximumRate READ maximumRate)
    Q_PROPERTY(bool CanGoNext READ canGoNext)
    Q_PROPERTY(bool CanGoPrevious READ canGoPrevious)
    Q_PROPERTY(bool CanPlay READ canPlay)
    Q_PROPERTY(bool CanPause READ canPause)
    Q_PROPERTY(bool CanSeek READ canSeek)
    Q_PROPERTY(bool CanControl READ canControl)

public:
    Mpris2PlayerAdaptor(QMediaPlayer *player, QAudioOutput *audioOut, QObject *parent)
        : QDBusAbstractAdaptor(parent), m_player(player), m_audioOutput(audioOut) {
        
        connect(m_player, &QMediaPlayer::playbackStateChanged, this, [this]() {
            emitPropertyChanged("PlaybackStatus", playbackStatus());
        });
        connect(m_player, &QMediaPlayer::metaDataChanged, this, [this]() {
            emitPropertyChanged("Metadata", metadata());
        });
        connect(m_player, &QMediaPlayer::positionChanged, this, [this]() {
            // Don't spam position updates — only on significant changes
        });
    }

    QString playbackStatus() const {
        switch (m_player->playbackState()) {
            case QMediaPlayer::PlayingState: return "Playing";
            case QMediaPlayer::PausedState: return "Paused";
            default: return "Stopped";
        }
    }
    
    double rate() const { return 1.0; }
    void setRate(double) {} // Not supported
    double minimumRate() const { return 1.0; }
    double maximumRate() const { return 1.0; }

    QVariantMap metadata() const {
        QVariantMap map;
        map["mpris:trackid"] = QVariant::fromValue(QDBusObjectPath("/org/mpris/MediaPlayer2/CurrentTrack"));
        if (m_player->duration() > 0)
            map["mpris:length"] = m_player->duration() * 1000; // microseconds
        
        QMediaMetaData meta = m_player->metaData();
        QString title = meta.stringValue(QMediaMetaData::Title);
        if (!title.isEmpty()) map["xesam:title"] = title;
        
        QString artist = meta.stringValue(QMediaMetaData::AlbumArtist);
        if (artist.isEmpty()) artist = meta.stringValue(QMediaMetaData::ContributingArtist);
        if (!artist.isEmpty()) map["xesam:artist"] = QStringList{artist};
        
        QString album = meta.stringValue(QMediaMetaData::AlbumTitle);
        if (!album.isEmpty()) map["xesam:album"] = album;
        
        QUrl source = m_player->source();
        if (source.isValid()) map["xesam:url"] = source.toString();
        
        return map;
    }

    double volume() const { return m_audioOutput->volume(); }
    void setVolume(double v) { m_audioOutput->setVolume(qBound(0.0, v, 1.0)); }

    qlonglong position() const { return m_player->position() * 1000; } // microseconds

    bool canGoNext() const { return true; }
    bool canGoPrevious() const { return true; }
    bool canPlay() const { return true; }
    bool canPause() const { return true; }
    bool canSeek() const { return m_player->duration() > 0; }
    bool canControl() const { return true; }

public slots:
    void Next();
    void Previous();
    void Pause() { m_player->pause(); }
    void PlayPause() {
        if (m_player->playbackState() == QMediaPlayer::PlayingState)
            m_player->pause();
        else
            m_player->play();
    }
    void Stop() { m_player->stop(); }
    void Play() { m_player->play(); }
    void Seek(qlonglong offset) {
        qint64 newPos = m_player->position() + offset / 1000; // from microseconds
        m_player->setPosition(qBound(0LL, newPos, m_player->duration()));
    }
    void SetPosition(const QDBusObjectPath &, qlonglong pos) {
        m_player->setPosition(pos / 1000); // from microseconds
    }
    void OpenUri(const QString &uri);

private:
    void emitPropertyChanged(const QString &property, const QVariant &value) {
        QDBusMessage msg = QDBusMessage::createSignal(
            "/org/mpris/MediaPlayer2",
            "org.freedesktop.DBus.Properties",
            "PropertiesChanged");
        msg << "org.mpris.MediaPlayer2.Player";
        QVariantMap changedProps;
        changedProps[property] = value;
        msg << changedProps;
        msg << QStringList();
        QDBusConnection::sessionBus().send(msg);
    }

    QMediaPlayer *m_player;
    QAudioOutput *m_audioOutput;
};

#endif // QT_DBUS_LIB

// ============================================================
// MediaLibraryWindow — Media Library browser with gen.bmp skin
// ============================================================
class MediaLibraryWindow : public QWidget {
    Q_OBJECT
    
public:
    MediaLibraryWindow(QWidget *parent = nullptr) : QWidget(parent) {
        setWindowTitle(TR("win.library.title", "Winamp Library"));
        setWindowFlags(Qt::Window | Qt::FramelessWindowHint);
        setAttribute(Qt::WA_DeleteOnClose, false);  // Don't delete on close, just hide
        resize(275, 300);
        setMinimumSize(275, 200);
        
        // Load gen.bmp and genex.bmp for skinning
        loadSkin();
        
        // Create tree view for browsing music folders
        treeView = new QTreeView(this);
        fileModel = new QFileSystemModel(this);
        
        // Start at user's home music directory or home
        QString musicPath = QStandardPaths::writableLocation(QStandardPaths::MusicLocation);
        if (musicPath.isEmpty() || !QDir(musicPath).exists()) {
            musicPath = QDir::homePath();
        }
        
        fileModel->setRootPath(musicPath);
        
        // Filter for audio files
        QStringList filters;
        filters << "*.mp3" << "*.flac" << "*.ogg" << "*.wav" << "*.m4a" << "*.aac" 
                << "*.wma" << "*.opus" << "*.mp4" << "*.avi" << "*.mkv" << "*.mov" << "*.webm";
        fileModel->setNameFilters(filters);
        fileModel->setNameFilterDisables(false); // Hide non-matching files
        
        treeView->setModel(fileModel);
        treeView->setRootIndex(fileModel->index(musicPath));
        treeView->setColumnWidth(0, 200);
        treeView->setStyleSheet(QString("QTreeView { background-color: %1; color: %2; border: none; }")
                               .arg(bgColor.name()).arg(fgColor.name()));
        treeView->setSelectionMode(QTreeView::ExtendedSelection);
        
        // Double-click to add to playlist
        connect(treeView, &QTreeView::doubleClicked, this, &MediaLibraryWindow::onItemDoubleClicked);
        
        updateLayout();
        
        isDragging = false;
        isResizing = false;
        resizeEdge = NoEdge;
    }
    
    void setPlaylistWindow(QObject *pl) { playlistWindow = pl; }
    
signals:
    void addToPlaylist(const QString &filePath);
    void addToPlaylistRecursive(const QString &dirPath);
    
protected:
    void paintEvent(QPaintEvent*) override {
        QPainter p(this);
        
        if (genBitmap.isNull()) {
            // Fallback if skin not loaded
            p.fillRect(rect(), Qt::darkGray);
            return;
        }
        
        // Draw window frame from gen.bmp
        // The layout is similar to minibrowser: corners + edges that tile
        // gen.bmp is 194x109:
        // Top-left corner (0,0) to (24,24)
        // Top-right corner (170,0) to (194,24)
        // Bottom-left corner (0,85) to (24,109)
        // Bottom-right corner (170,85) to (194,109)
        // Left edge: (0,24) height 61
        // Right edge: (170,24) height 61
        // Top edge: (24,0) width 146
        // Bottom edge: (24,85) width 146
        
        int w = width();
        int h = height();
        
        // Corners
        p.drawPixmap(0, 0, genBitmap, 0, 0, 24, 24);                    // top-left
        p.drawPixmap(w - 24, 0, genBitmap, 170, 0, 24, 24);             // top-right
        p.drawPixmap(0, h - 24, genBitmap, 0, 85, 24, 24);              // bottom-left
        p.drawPixmap(w - 24, h - 24, genBitmap, 170, 85, 24, 24);       // bottom-right
        
        // Edges (tiled)
        for (int x = 24; x < w - 24; x += 146) {
            int tileW = qMin(146, w - 24 - x);
            p.drawPixmap(x, 0, genBitmap, 24, 0, tileW, 24);            // top edge
            p.drawPixmap(x, h - 24, genBitmap, 24, 85, tileW, 24);      // bottom edge
        }
        for (int y = 24; y < h - 24; y += 61) {
            int tileH = qMin(61, h - 24 - y);
            p.drawPixmap(0, y, genBitmap, 0, 24, 24, tileH);            // left edge
            p.drawPixmap(w - 24, y, genBitmap, 170, 24, 24, tileH);     // right edge
        }
        
        // Fill center with background color
        p.fillRect(24, 24, w - 48, h - 48, bgColor);
        
        // Draw titlebar text "Winamp Library" using gen.bmp font
        QString title = "Winamp Library";
        int textX = 30;
        for (const QChar &ch : title) {
            QPoint charPos = getTitleCharPos(ch);
            if (charPos.x() >= 0) {
                p.drawPixmap(textX, 6, genBitmap, charPos.x(), charPos.y(), getCharWidth(ch), 10);
                textX += getCharWidth(ch);
            }
        }
    }
    
    void resizeEvent(QResizeEvent*) override {
        updateLayout();
    }
    
    void mousePressEvent(QMouseEvent *event) override {
        if (event->button() == Qt::LeftButton) {
            // Check for resize edges first
            ResizeEdge edge = hitTestResize(event->pos());
            if (edge != NoEdge) {
                isResizing = true;
                resizeEdge = edge;
                resizeStartPos = event->globalPosition().toPoint();
                resizeStartGeometry = geometry();
                return;
            }
            
            // Check if clicking in titlebar area (draggable)
            if (event->pos().y() < 24) {
                isDragging = true;
                dragStartPos = event->pos();
            }
        }
    }
    
    void mouseMoveEvent(QMouseEvent *event) override {
        if (isResizing) {
            QPoint delta = event->globalPosition().toPoint() - resizeStartPos;
            QRect newGeom = resizeStartGeometry;
            
            if (resizeEdge & LeftEdge) {
                int newX = resizeStartGeometry.x() + delta.x();
                int newWidth = resizeStartGeometry.width() - delta.x();
                if (newWidth >= minimumWidth()) {
                    newGeom.setX(newX);
                    newGeom.setWidth(newWidth);
                }
            }
            if (resizeEdge & RightEdge) {
                newGeom.setWidth(qMax(minimumWidth(), resizeStartGeometry.width() + delta.x()));
            }
            if (resizeEdge & TopEdge) {
                int newY = resizeStartGeometry.y() + delta.y();
                int newHeight = resizeStartGeometry.height() - delta.y();
                if (newHeight >= minimumHeight()) {
                    newGeom.setY(newY);
                    newGeom.setHeight(newHeight);
                }
            }
            if (resizeEdge & BottomEdge) {
                newGeom.setHeight(qMax(minimumHeight(), resizeStartGeometry.height() + delta.y()));
            }
            
            setGeometry(newGeom);
            return;
        }
        
        if (isDragging) {
            move(event->globalPosition().toPoint() - dragStartPos);
            return;
        }
        
        // Update cursor for resize edges
        ResizeEdge edge = hitTestResize(event->pos());
        updateCursorForEdge(edge);
    }
    
    void mouseReleaseEvent(QMouseEvent *event) override {
        if (event->button() == Qt::LeftButton) {
            isDragging = false;
            isResizing = false;
            resizeEdge = NoEdge;
        }
    }
    
    void mouseDoubleClickEvent(QMouseEvent *event) override {
        // Double-click titlebar to close
        if (event->pos().y() < 24) {
            hide();
        }
    }
    
    void keyPressEvent(QKeyEvent *event) override {
        if (event->key() == Qt::Key_Escape) {
            hide();
        } else {
            QWidget::keyPressEvent(event);
        }
    }
    
private slots:
    void onItemDoubleClicked(const QModelIndex &index) {
        QString path = fileModel->filePath(index);
        QFileInfo info(path);
        
        if (info.isFile()) {
            // Single file - add to playlist
            emit addToPlaylist(path);
        } else if (info.isDir()) {
            // Directory - add all audio files recursively
            emit addToPlaylistRecursive(path);
        }
    }
    
private:
    enum ResizeEdge {
        NoEdge = 0,
        LeftEdge = 1,
        RightEdge = 2,
        TopEdge = 4,
        BottomEdge = 8
    };
    
    void loadSkin() {
        // Try to load gen.bmp and genex.bmp from skin paths
        QStringList skinPaths = {
            "skins/default",
            "Src/Winamp/resource"
        };
        
        for (const QString &basePath : skinPaths) {
            QString genPath = basePath + "/gen.bmp";
            QString genexPath = basePath + "/genex.bmp";
            
            if (QFile::exists(genPath) && genBitmap.isNull()) {
                genBitmap = QPixmap(genPath);
            }
            if (QFile::exists(genexPath) && genexBitmap.isNull()) {
                genexBitmap = QPixmap(genexPath);
            }
        }
        
        // Load colors from genex.bmp if available
        if (!genexBitmap.isNull()) {
            QImage img = genexBitmap.toImage();
            if (img.width() >= 95 && img.height() >= 1) {
                // Read color palette from genex.bmp (x=48 onwards, every other pixel)
                bgColor = img.pixelColor(52, 0);    // x=52: window background
                fgColor = img.pixelColor(56, 0);    // x=56: window text color
            }
        }
        
        // Fallback colors if not loaded
        if (!bgColor.isValid()) bgColor = QColor(36, 36, 60);
        if (!fgColor.isValid()) fgColor = QColor(255, 255, 255);
    }
    
    QPoint getTitleCharPos(QChar ch) {
        // gen.bmp titlebar font starts at y=99 (highlighted) and y=100-109 (normal)
        // We'll use the highlighted version for now
        // The font is variable width, first color before 'A' is delimiter
        // For simplicity, use a standard 5-pixel width assumption
        int yPos = 99;  // Highlighted titlebar font row
        
        if (ch >= 'A' && ch <='Z') {
            return QPoint(3 + (ch.toLatin1() - 'A') * 5, yPos);
        } else if (ch >= 'a' && ch <= 'z') {
            return QPoint(3 + (ch.toLatin1() - 'a') * 5, yPos);
        } else if (ch >= '0' && ch <= '9') {
            return QPoint(3 + 26 * 5 + (ch.toLatin1() - '0') * 5, yPos);
        } else if (ch == ' ') {
            return QPoint(3 + 36 * 5, yPos);
        }
        return QPoint(-1, -1); // Unknown character
    }
    
    int getCharWidth(QChar ch) {
        // For simplicity, assume 5px width for all chars
        // A real implementation would read the delimiter pixel
        if (ch == ' ') return 3;
        return 5;
    }
    
    ResizeEdge hitTestResize(QPoint pos) {
        const int edgeSize = 8;
        int w = width();
        int h = height();
        ResizeEdge edge = NoEdge;
        
        if (pos.x() < edgeSize) edge = (ResizeEdge)(edge | LeftEdge);
        else if (pos.x() > w - edgeSize) edge = (ResizeEdge)(edge | RightEdge);
        
        if (pos.y() < edgeSize) edge = (ResizeEdge)(edge | TopEdge);
        else if (pos.y() > h - edgeSize) edge = (ResizeEdge)(edge | BottomEdge);
        
        return edge;
    }
    
    void updateCursorForEdge(ResizeEdge edge) {
        if (edge == NoEdge) {
            setCursor(Qt::ArrowCursor);
        } else if (edge == (LeftEdge | TopEdge) || edge == (RightEdge | BottomEdge)) {
            setCursor(Qt::SizeFDiagCursor);
        } else if (edge == (RightEdge | TopEdge) || edge == (LeftEdge | BottomEdge)) {
            setCursor(Qt::SizeBDiagCursor);
        } else if (edge & (LeftEdge | RightEdge)) {
            setCursor(Qt::SizeHorCursor);
        } else if (edge & (TopEdge | BottomEdge)) {
            setCursor(Qt::SizeVerCursor);
        }
    }
    
    void updateLayout() {
        // Position tree view inside the window frame (24px border on all sides)
        if (treeView) {
            treeView->setGeometry(24, 24, width() - 48, height() - 48);
        }
    }
    
    QTreeView *treeView;
    QFileSystemModel *fileModel;
    QPixmap genBitmap;
    QPixmap genexBitmap;
    QColor bgColor;
    QColor fgColor;
    
    bool isDragging;
    bool isResizing;
    ResizeEdge resizeEdge;
    QPoint dragStartPos;
    QPoint resizeStartPos;
    QRect resizeStartGeometry;
    
    QObject *playlistWindow = nullptr;
};

// Main Winamp Window
class WinampWindow : public QWidget {
    Q_OBJECT
public:
    WinampWindow(QWidget *parent = nullptr) : QWidget(parent), dragPosition(0,0), isDragging(false),
                 volume(200), balance(0), hoveredButton(-1), pressedButton(-1),
                 shuffleOn(false), repeatOn(false), eqBtnOn(false), plBtnOn(false),
                 repeatTrack(false), stopAfterCurrent(false),
                 isDraggingVolume(false), isDraggingBalance(false), isDraggingPos(false), 
                 scrollOffset(0), visMode(1), doubleSize(false), shadeMode(false),
                 alwaysOnTop(false), clutterbarOpen(false) {
        setFixedSize(275, 116);
        setWindowTitle("Winamp 5.666 for Linux");
        setWindowFlags(Qt::FramelessWindowHint);
        setAttribute(Qt::WA_TranslucentBackground, false);
        setMouseTracking(true);
        setFocusPolicy(Qt::StrongFocus);
        setAcceptDrops(true);  // Accept drag-drop on main window too
        
        // Initialize visualization state
        memset(saBarHeight, 0, sizeof(saBarHeight));
        memset(saPeakHeight, 0, sizeof(saPeakHeight));
        memset(saPeakVel, 0, sizeof(saPeakVel));
        memset(spectrumData, 0, sizeof(spectrumData));
        memset(oscData, 0, sizeof(oscData));
        memset(vuData, 0, sizeof(vuData));
        
        // Initialize easter egg state
        memset(eggStr, 0, sizeof(eggStr));
        eggStat = 0;
        
        // Setup audio — dual path:
        // 1) QAudioOutput for direct playback (used as fallback / when EQ is off)
        // 2) QAudioBufferOutput → EQ10 DSP → QAudioSink (when EQ is on)
        player = new QMediaPlayer(this);
        audioOutput = new QAudioOutput(this);
        player->setAudioOutput(audioOutput);
        audioOutput->setVolume(volume / 255.0f);
        
        // Setup second player for gapless playback
        nextPlayer = new QMediaPlayer(this);
        nextAudioOutput = new QAudioOutput(this);
        nextPlayer->setAudioOutput(nextAudioOutput);
        nextAudioOutput->setVolume(volume / 255.0f);
        usingNextPlayer = false;
        
        // Initialize EQ DSP state
        memset(eqState, 0, sizeof(eqState));
        eqSampleRate = 0;
        eqChannels = 0;
        eqDspActive = false;
        
        // Setup audio buffer output for visualization + EQ DSP
        audioBufferOutput = new QAudioBufferOutput(this);
        player->setAudioBufferOutput(audioBufferOutput);
        connect(audioBufferOutput, &QAudioBufferOutput::audioBufferReceived,
                this, &WinampWindow::processAudioBuffer);
        
        // System tray icon (matches Windows SYSTRAY.cpp)
        setupSystemTray();
        
        // MPRIS2 D-Bus integration — Linux desktop media keys and remote control
        // (equivalent to Windows global hotkeys + WM_COMMAND remote control)
#ifdef QT_DBUS_LIB
        new Mpris2RootAdaptor(this);
        new Mpris2PlayerAdaptor(player, audioOutput, this);
        QDBusConnection dbus = QDBusConnection::sessionBus();
        dbus.registerObject("/org/mpris/MediaPlayer2", this);
        dbus.registerService("org.mpris.MediaPlayer2.winamp");
#endif
        
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
        
        // Auto-show video window when video content is detected
        connect(player, &QMediaPlayer::hasVideoChanged, this, [this](bool hasVideo) {
            if (hasVideo && videoWindow) {
                videoWindow->setHasVideo(true);
                videoWindow->show();
                videoWindow->raise();
            } else if (videoWindow) {
                videoWindow->setHasVideo(false);
                videoWindow->hide();
            }
        });
        
        // Auto-advance to next track when current one ends
        connect(player, &QMediaPlayer::mediaStatusChanged, this, [this](QMediaPlayer::MediaStatus status) {
            if (status == QMediaPlayer::EndOfMedia) {
                // Stop after current track (like Windows g_stopaftercur)
                if (stopAfterCurrent) {
                    stopAfterCurrent = false;
                    player->stop();
                    update();
                    return;
                }
                
                // Repeat track (repeat one): replay same track
                if (repeatOn && repeatTrack) {
                    player->setPosition(0);
                    player->play();
                    return;
                }
                
                // Gapless playback: if next track is preloaded, swap players
                if (nextPlayer->source().isValid() && !shuffleOn) {
                    // Swap players for seamless transition
                    std::swap(player, nextPlayer);
                    std::swap(audioOutput, nextAudioOutput);
                    
                    // Update visualization to use the now-active player
                    player->setAudioBufferOutput(audioBufferOutput);
                    nextPlayer->setAudioBufferOutput(nullptr);
                    
                    // Start the preloaded track
                    player->play();
                    
                    // Update currentFile
                    currentFile = player->source().toLocalFile();
                    
                    // Update playlist index
                    int curIdx = playlistWindow->currentTrackIndex();
                    int count = playlistWindow->trackCount();
                    int nextIdx = curIdx + 1;
                    if (nextIdx < count) {
                        playlistWindow->setCurrentTrackIndex(nextIdx);
                    } else if (repeatOn && count > 0) {
                        playlistWindow->setCurrentTrackIndex(0);
                    }
                    
                    // Update tray and show notification
                    QString fileName = currentFile;
                    RecentFilesManager::instance().addFile(fileName);
                    updateTrayTooltip();
                    if (showSongNotifications && trayIcon) {
                        QString title = metaTitle.isEmpty() ? QFileInfo(fileName).completeBaseName() : metaTitle;
                        trayIcon->showMessage("Winamp", title, QSystemTrayIcon::Information, 3000);
                    }
                    
                    // Preload the next track
                    preloadNextTrack();
                    return;
                }
                
                // Fallback to normal track advancing (for shuffle or when preload failed)
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
                        // Repeat all: wrap to beginning
                        playlistWindow->setCurrentTrackIndex(0);
                        playTrack(playlistWindow->trackAt(0));
                    }
                }
            }
        });
        
        // Extract bitrate and song metadata when available
        connect(player, &QMediaPlayer::metaDataChanged, this, [this]() {
            QMediaMetaData md = player->metaData();
            QVariant br = md.value(QMediaMetaData::AudioBitRate);
            if (br.isValid()) {
                mediaBitrate = br.toInt() / 1000;  // bps -> kbps
            }
            // Extract title and artist for scrolling display
            QString title = md.value(QMediaMetaData::Title).toString();
            QString artist;
            QVariant artistVar = md.value(QMediaMetaData::ContributingArtist);
            if (artistVar.canConvert<QStringList>())
                artist = artistVar.toStringList().join(", ");
            else
                artist = artistVar.toString();
            
            QString newMetaTitle;
            if (!title.isEmpty()) {
                if (!artist.isEmpty())
                    newMetaTitle = artist + " - " + title;
                else
                    newMetaTitle = title;
            }
            
            // If metadata changed (for streams), show notification
            if (!newMetaTitle.isEmpty() && newMetaTitle != metaTitle) {
                metaTitle = newMetaTitle;
                if (showSongNotifications && trayIcon) {
                    trayIcon->showMessage("Winamp", metaTitle, QSystemTrayIcon::Information, 3000);
                }
            } else if (!newMetaTitle.isEmpty()) {
                metaTitle = newMetaTitle;
            }
            
            // Update tray tooltip
            updateTrayTooltip();
        });
        
        // Create playlist and EQ windows
        playlistWindow = new PlaylistWindow(this);
        connect(playlistWindow, &PlaylistWindow::trackDoubleClicked, this, &WinampWindow::playTrack);
        eqWindow = new EqualizerWindow(this);
        
        // Create video window (hidden by default)
        videoWindow = new VideoWindow(this);
        videoWindow->hide();
        player->setVideoOutput(videoWindow->getVideoWidget());
        
        // Create media library window (hidden by default)
        mediaLibraryWindow = new MediaLibraryWindow(this);
        mediaLibraryWindow->hide();
        connect(mediaLibraryWindow, &MediaLibraryWindow::addToPlaylist, this, [this](const QString &path) {
            playlistWindow->addTrack(path);
        });
        connect(mediaLibraryWindow, &MediaLibraryWindow::addToPlaylistRecursive, this, [this](const QString &dirPath) {
            // Add all audio files in directory recursively
            QStringList queue;
            queue << dirPath;
            while (!queue.isEmpty()) {
                QString currentDir = queue.takeFirst();
                QDir dir(currentDir);
                
                // Add audio files
                QStringList filters;
                filters << "*.mp3" << "*.flac" << "*.ogg" << "*.wav" << "*.m4a" 
                       << "*.aac" << "*.wma" << "*.opus" << "*.mp4" << "*.avi" 
                       << "*.mkv" << "*.mov" << "*.webm";
                QFileInfoList files = dir.entryInfoList(filters, QDir::Files);
                for (const QFileInfo &file : files) {
                    playlistWindow->addTrack(file.absoluteFilePath());
                }
                
                // Add subdirectories to queue
                QFileInfoList subdirs = dir.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot);
                for (const QFileInfo &subdir : subdirs) {
                    queue << subdir.absoluteFilePath();
                }
            }
        });
        
        // Hide EQ and playlist when video goes fullscreen (like Milkdrop)
        connect(videoWindow, &VideoWindow::fullscreenChanged, this, [this](bool fs) {
            if (fs) {
                // Entering fullscreen - hide main, EQ, and playlist
                hide();
                eqWindow->hide();
                playlistWindow->hide();
            } else {
                // Exiting fullscreen - restore visibility based on previous state
                show();
                if (eqBtnOn) eqWindow->show();
                if (plBtnOn) playlistWindow->show();
            }
        });
        
        // Position windows: EQ below main, playlist to the right of main
        playlistWindow->move(x() + width(), y());  // right of main
        eqWindow->move(x(), y() + height());
        
        // Load bookmarks and recent files
        BookmarkManager::instance().load();
        RecentFilesManager::instance().load();
        
        // Load saved settings (overrides defaults above)
        loadAllSettings();
    }
    
    ~WinampWindow() {
        delete playlistWindow;
        delete eqWindow;
        delete videoWindow;
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
            updateTrayTooltip();
            
            // Show notification for stream
            if (showSongNotifications && trayIcon) {
                QString title = metaTitle.isEmpty() ? url : metaTitle;
                trayIcon->showMessage("Winamp", title, QSystemTrayIcon::Information, 3000);
            }
            
            // Don't preload next track for streams
        }
    }

public slots:
    void onPlayFile() {
        QString file = QFileDialog::getOpenFileName(this, "Open File", QString(), 
            "Audio Files (*.mp3 *.wav *.flac *.ogg *.m4a *.aac *.wma);;All Files (*)");
        if (!file.isEmpty()) {
            playFile(file);
            RecentFilesManager::instance().addFile(file);
        }
    }

    // Public accessors for main() CLI argument handling
    QMediaPlayer *getPlayer() { return player; }
    PlaylistWindow *getPlaylistWindow() { return playlistWindow; }

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
        alwaysOnTop = checked;
        setWindowFlag(Qt::WindowStaysOnTopHint, checked);
        show(); // Re-show to apply the flag change
    }
    
    void onToggleDoubleSize() {
        doubleSize = !doubleSize;
        if (doubleSize) {
            setFixedSize(550, 232);
        } else {
            setFixedSize(275, 116);
        }
        update();
    }
    
    void onToggleShadeMode() {
        shadeMode = !shadeMode;
        if (shadeMode) {
            setFixedSize(275, 14);
        } else {
            if (doubleSize)
                setFixedSize(550, 232);
            else
                setFixedSize(275, 116);
        }
        update();
    }

    void onShowAbout() {
        AboutDialog aboutDialog(WinampBitmaps::instance().basePath, this);
        aboutDialog.exec();
    }
    
    void onJumpToFile() {
        JumpToFileDialog dialog(playlistWindow->allTracks(), this);
        if (dialog.exec() == QDialog::Accepted) {
            int idx = dialog.getSelectedIndex();
            if (idx >= 0 && idx < playlistWindow->trackCount()) {
                playlistWindow->setCurrentTrackIndex(idx);
                playTrack(playlistWindow->trackAt(idx));
            }
        }
    }
    
    void onAddBookmark() {
        if (!currentFile.isEmpty()) {
            QString title = metaTitle.isEmpty() ? QFileInfo(currentFile).baseName() : metaTitle;
            BookmarkManager::instance().addBookmark(title, currentFile);
        }
    }

    void onSkinChanged(const QString &skinPath) {
        WinampBitmaps::instance().loadAll(skinPath);
        
        // Parse skin playlist colors (PLEDIT.TXT)
        g_plColors = parsePleditTxt(skinPath);
        
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
        playlistWindow->applyPlaylistColors();
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

        // ---- VISUALIZATION (always runs, regardless of EQ) ----
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
                float db = 0;
                if (maxVal > 0.001f) {
                    db = log10f(1.0f + maxVal * 5.0f) / log10f(1.0f + 5.0f * 50.0f);
                }
                spectrumData[i] = qBound(0.0f, db, 1.0f);
            }
        };

        // Extract visualization data + VU meter
        if (fmt.sampleFormat() == QAudioFormat::Int16) {
            extractData(buffer.constData<qint16>());
            const qint16 *data = buffer.constData<qint16>();
            float lSum = 0, rSum = 0;
            int n = qMin(sampleCount, 512);
            for (int i = 0; i < n; i++) {
                float l = data[i * channels] / 32768.0f;
                float r = (channels > 1) ? data[i * channels + 1] / 32768.0f : l;
                lSum += l * l;
                rSum += r * r;
            }
            vuData[0] = sqrtf(lSum / n) * 3.0f;
            vuData[1] = sqrtf(rSum / n) * 3.0f;
            if (milkdropWindow && milkdropWindow->isVisible())
                milkdropWindow->feedPCMInt16(buffer.constData<qint16>(), sampleCount, channels);
        } else if (fmt.sampleFormat() == QAudioFormat::Float) {
            extractData(buffer.constData<float>());
            const float *data = buffer.constData<float>();
            float lSum = 0, rSum = 0;
            int n = qMin(sampleCount, 512);
            for (int i = 0; i < n; i++) {
                float l = data[i * channels];
                float r = (channels > 1) ? data[i * channels + 1] : l;
                lSum += l * l;
                rSum += r * r;
            }
            vuData[0] = sqrtf(lSum / n) * 3.0f;
            vuData[1] = sqrtf(rSum / n) * 3.0f;
            if (milkdropWindow && milkdropWindow->isVisible())
                milkdropWindow->feedPCMFloat(buffer.constData<float>(), sampleCount, channels);
        }

        // ---- EQ DSP PROCESSING (matches Windows eq10dsp.cpp + In.cpp) ----
        // When EQ is enabled: mute QAudioOutput, process through EQ10, output via QAudioSink
        bool eqEnabled = eqWindow && eqWindow->isEnabled();
        
        if (eqEnabled) {
            int sampleRate = fmt.sampleRate();
            
            // Mute the direct QAudioOutput path — we'll output processed audio via QAudioSink
            if (!eqDspActive) {
                audioOutput->setVolume(0.0f);
                eqDspActive = true;
            }
            
            // Setup/reconfigure QAudioSink if format changed
            if (sampleRate != eqSampleRate || channels != eqChannels) {
                eqSampleRate = sampleRate;
                eqChannels = qMin(channels, 2); // stereo max for EQ
                
                // Re-initialize EQ filter state for new sample rate
                eq10_setup(eqState, eqChannels, (double)sampleRate);
                
                // Update EQ gains from current slider positions
                for (int b = 0; b < 10; b++) {
                    int sliderVal = eqWindow->getBandValue(b);
                    double dB = eq10_valtodb(sliderVal);
                    eq10_setgain(eqState, eqChannels, b, dB);
                }
                
                // Create QAudioSink with matching format
                if (audioSink) {
                    audioSink->stop();
                    delete audioSink;
                }
                QAudioFormat outFmt;
                outFmt.setSampleRate(sampleRate);
                outFmt.setChannelCount(eqChannels);
                outFmt.setSampleFormat(QAudioFormat::Float);
                
                audioSink = new QAudioSink(QMediaDevices::defaultAudioOutput(), outFmt, this);
                audioSink->setBufferSize(sampleRate * eqChannels * sizeof(float) / 5); // ~200ms buffer
                audioSinkDevice = audioSink->start();
            }
            
            if (!audioSinkDevice) return;
            
            // Update EQ gains every buffer (cheap, ensures sliders are responsive)
            for (int b = 0; b < 10; b++) {
                int sliderVal = eqWindow->getBandValue(b);
                double dB = eq10_valtodb(sliderVal);
                eq10_setgain(eqState, eqChannels, b, dB);
            }
            
            // Get preamp value — uses the original Winamp lookup table
            int preampSlider = eqWindow->getPreampValue();
            float preampGain = eq_preamp_table[qBound(0, preampSlider, 63)];
            
            // Volume and balance (applied post-EQ, matching Windows output chain)
            float vol = volume / 255.0f;
            float balL = 1.0f, balR = 1.0f;
            if (balance < 0) balR = (127.0f + balance) / 127.0f; // left-biased
            if (balance > 0) balL = (127.0f - balance) / 127.0f; // right-biased
            
            // Allocate float working buffer
            int totalSamples = sampleCount * eqChannels;
            QVector<float> floatBuf(totalSamples);
            QVector<float> outBuf(totalSamples);
            
            // Convert input to float with preamp applied (matches In.cpp FillFloat)
            if (fmt.sampleFormat() == QAudioFormat::Int16) {
                const qint16 *src = buffer.constData<qint16>();
                for (int i = 0; i < sampleCount; i++) {
                    for (int ch = 0; ch < eqChannels; ch++) {
                        floatBuf[i * eqChannels + ch] = (src[i * channels + ch] / 32768.0f) * preampGain;
                    }
                }
            } else if (fmt.sampleFormat() == QAudioFormat::Float) {
                const float *src = buffer.constData<float>();
                for (int i = 0; i < sampleCount; i++) {
                    for (int ch = 0; ch < eqChannels; ch++) {
                        floatBuf[i * eqChannels + ch] = src[i * channels + ch] * preampGain;
                    }
                }
            } else {
                return; // unsupported format
            }
            
            // Process through EQ10 for each channel (matches Windows inner loop)
            for (int ch = 0; ch < eqChannels; ch++) {
                eq10_processf(&eqState[ch], floatBuf.data(), outBuf.data(),
                              sampleCount, ch, eqChannels);
            }
            
            // Apply volume and balance post-EQ
            for (int i = 0; i < sampleCount; i++) {
                if (eqChannels >= 2) {
                    outBuf[i * eqChannels + 0] *= vol * balL;
                    outBuf[i * eqChannels + 1] *= vol * balR;
                } else {
                    outBuf[i * eqChannels + 0] *= vol;
                }
            }
            
            // Write to QAudioSink
            if (audioSinkDevice) {
                qint64 bytes = totalSamples * sizeof(float);
                audioSinkDevice->write(reinterpret_cast<const char*>(outBuf.data()), bytes);
            }
        } else {
            // EQ is off — restore direct QAudioOutput path
            if (eqDspActive) {
                audioOutput->setVolume(volume / 255.0f);
                eqDspActive = false;
                // Stop the DSP sink
                if (audioSink) {
                    audioSink->stop();
                    delete audioSink;
                    audioSink = nullptr;
                    audioSinkDevice = nullptr;
                }
                eqSampleRate = 0;
                eqChannels = 0;
            }
        }
    }

protected:
    void paintEvent(QPaintEvent *) override {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing, false);

        auto &bmp = WinampBitmaps::instance();
        
        // ---- Shade mode: compact 275x14 titlebar-only view ----
        if (shadeMode) {
            // Draw shade background from titlebar.bmp (shade bar)
            if (!bmp.titlebar.isNull()) {
                // Shade titlebar: active at line 29, inactive at line 42, each 275x14
                int shadeY = isActiveWindow() ? 29 : 42;
                p.drawPixmap(0, 0, bmp.titlebar, 27, shadeY, 275, 14);
            } else {
                p.fillRect(0, 0, 275, 14, QColor(66, 66, 99));
            }
            // Draw scrolling title text in shade mode
            QString title = metaTitle.isEmpty() ? QFileInfo(currentFile).completeBaseName() : metaTitle;
            if (!title.isEmpty()) {
                p.setPen(QColor(0, 255, 0));
                p.setFont(QFont("Small Fonts", 7));
                p.setClipRect(30, 2, 200, 11);
                p.drawText(30 - (scrollOffset % (title.length() * 6 + 200)), 10, title + "  ***  " + title);
                p.setClipping(false);
            }
            return;
        }
        
        // ---- Double-size: scale everything 2x ----
        if (doubleSize) {
            p.scale(2.0, 2.0);
        }

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

        // Clutterbar — left side options bar (O/A/I/D/V buttons) at (10,22)
        // Matching Windows draw.cpp draw_clutterbar() function
        drawClutterbar(p);

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
        // BUT if nums_ex.bmp is present, use it and draw animated colon (matches Windows draw.cpp)
        // Click the time area to toggle elapsed / remaining
        
        // Prefer nums_ex.bmp if available (extended numbers with animated colon)
        const QPixmap &numberBitmap = !bmp.numbers_ex.isNull() ? bmp.numbers_ex : bmp.numbers;
        bool hasExtended = !bmp.numbers_ex.isNull();
        
        if (!numberBitmap.isNull()) {
            qint64 displayMs;
            if (showRemainingTime && player->duration() > 0) {
                displayMs = player->duration() - player->position();
                // Draw minus indicator from numbers.bmp 12th glyph, or fallback dash
                if (numberBitmap.width() >= 108)
                    p.drawPixmap(27, 26, numberBitmap, 99, 0, 9, 13);
                else
                    p.fillRect(29, 32, 5, 1, QColor(0, 198, 0));
            } else {
                displayMs = player->position();
            }
            int sec = displayMs / 1000;
            int mins = sec / 60;
            sec %= 60;
            auto drawDigit = [&](int dx, int d) {
                int srcX = (d >= 0 && d <= 9) ? d * 9 : 90; // 90 = blank
                p.drawPixmap(dx, 26, numberBitmap, srcX, 0, 9, 13);
            };
            
            // Draw animated colon if nums_ex.bmp is present (matches Windows ex==1 path)
            if (hasExtended) {
                // nums_ex.bmp has colon at x=90 (Windows draw_main.cpp line 240)
                p.drawPixmap(38, 26, numberBitmap, 90, 0, 9, 13);
            }
            // else: colon is baked into MAIN.BMP at position ~68, so don't draw it
            
            drawDigit(36, (mins / 10) % 10);
            drawDigit(48, mins % 10);
            drawDigit(78, sec / 10);
            drawDigit(90, sec % 10);
        }

        // Scrolling song title in text area (111,27) ~154x6
        // Uses metadata (Artist - Title) when available, falls back to filename
        if (!bmp.text.isNull() && !currentFile.isEmpty()) {
            QString title;
            if (!metaTitle.isEmpty())
                title = metaTitle.toUpper();
            else
                title = QFileInfo(currentFile).baseName().toUpper();
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

        // Balance/Pan slider — uses BALANCE.BMP (same format as volume)
        // Falls back to volume.bmp if balance isn't available
        if (!bmp.balance.isNull()) {
            int balNorm = qBound(0, (balance + 127) * 27 / 254, 27); // -127..+127 -> 0..27
            // Source sprite starts at x=9 in balance.bmp (matching Windows draw_panbar)
            p.drawPixmap(177, 57, bmp.balance, 9, balNorm * 15, 38, 13);
            int balThumbX = 177 + ((balance + 127) * 24) / 254;
            int balThumbSrcX = isDraggingBalance ? 0 : 15; // pressed=0, normal=15 (reversed from volume!)
            p.drawPixmap(balThumbX, 58, bmp.balance, balThumbSrcX, 422, 14, 11);
        } else if (!bmp.volume.isNull()) {
            // Fallback: draw balance using volume.bmp (cropped narrower)
            int balNorm = qBound(0, (balance + 127) * 27 / 254, 27);
            p.drawPixmap(177, 57, bmp.volume, 15, balNorm * 15, 38, 13);
            int balThumbX = 177 + ((balance + 127) * 24) / 254;
            p.drawPixmap(balThumbX, 58, bmp.volume, 0, 422, 14, 11);
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
        else if (visMode == 3) drawVUMeter(p);
        
        // Double-size mode: scale 2x
        if (doubleSize && !shadeMode) {
            // Already handled by transform in actual paint
        }
    }

    // Clutterbar — Options bar on left side (O/A/I/D/V buttons)
    // Matches Windows draw.cpp draw_clutterbar() at line 550
    void drawClutterbar(QPainter &p) {
        auto &bmp = WinampBitmaps::instance();
        if (bmp.titlebar.isNull()) return;
        
        // Clutterbar region: x=10, y=22, width=8, height=43
        // Source sprite at titlebar.bmp x=304
        int enable = clutterbarOpen ? 1 : 0;
        int x, y;
        
        if (!enable) {
            x = 8;  // Closed state
            y = 0;
        } else {
            x = 0;  // Open state
            y = 0;
        }
        
        // Draw main clutterbar strip (8x43 pixels)
        p.drawPixmap(10, 22, bmp.titlebar, 304 + x, y, 8, 43);
        
        // Draw Always On Top button state (at button position y=22+11=33)
        if (enable) {
            if (alwaysOnTop) {
                // AOT enabled: draw pressed sprite
                p.drawPixmap(11, 22 + 11, bmp.titlebar, 312 + 1, 44 + 11, 7, 8);
            } else {
                // AOT disabled: draw normal sprite
                p.drawPixmap(11, 22 + 11, bmp.titlebar, 304 + 1, 11, 7, 8);
            }
            
            // Draw Double Size button state (at button position y=22+27=49)
            if (doubleSize) {
                // Double size enabled: draw pressed sprite
                p.drawPixmap(11, 22 + 27, bmp.titlebar, 328 + 1, 44 + 27, 7, 6);
            } else {
                // Double size disabled: draw normal sprite
                p.drawPixmap(11, 22 + 27, bmp.titlebar, 304 + 1, 27, 7, 6);
            }
        }
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

    // VU Meter — dual-channel level meter (matches Windows vu.cpp)
    void drawVUMeter(QPainter &p) {
        const int visX = 24, visY = 43, visH = 16;
        p.fillRect(visX, visY, 75, visH, visColors[0]);
        
        // Left channel
        int leftLevel = qBound(0, (int)(vuData[0] * 35), 35);
        for (int i = 0; i < leftLevel; i++) {
            int colorIdx = 17 - (i * 15 / 35);
            if (colorIdx < 2) colorIdx = 2;
            if (colorIdx > 17) colorIdx = 17;
            p.fillRect(visX + i * 2, visY, 1, 7, visColors[colorIdx]);
        }
        
        // Right channel
        int rightLevel = qBound(0, (int)(vuData[1] * 35), 35);
        for (int i = 0; i < rightLevel; i++) {
            int colorIdx = 17 - (i * 15 / 35);
            if (colorIdx < 2) colorIdx = 2;
            if (colorIdx > 17) colorIdx = 17;
            p.fillRect(visX + i * 2, visY + 9, 1, 7, visColors[colorIdx]);
        }
        
        // Labels
        p.setPen(visColors[1]);
        p.setFont(QFont("Courier", 5));
        p.drawText(visX + 72, visY + 6, "L");
        p.drawText(visX + 72, visY + 15, "R");
    }

    void keyPressEvent(QKeyEvent *event) override {
        // Easter egg detection (matches Windows eggstat/eggstr from main.cpp)
        if (event->text().length() == 1) {
            QChar ch = event->text().at(0).toUpper();
            const QString egg1 = "NULLSOFT";
            const QString egg2 = "WINAMP";
            // Shift characters left, append new char
            for (int i = 0; i < 7; i++) eggStr[i] = eggStr[i + 1];
            eggStr[7] = ch.toLatin1();
            eggStr[8] = 0;
            
            if (QString(eggStr).endsWith(egg1)) {
                eggStat = 1;
                setWindowTitle("Winamp - \"It really whips the llama's ass!\"");
                QTimer::singleShot(3000, this, [this]() { 
                    setWindowTitle("Winamp 5.666 for Linux"); 
                    eggStat = 0;
                });
            } else if (QString(eggStr).endsWith(egg2)) {
                eggStat = 2;
                setWindowTitle("Winamp - by Justin Frankel & the Nullsoft crew");
                QTimer::singleShot(3000, this, [this]() { 
                    setWindowTitle("Winamp 5.666 for Linux"); 
                    eggStat = 0;
                });
            }
        }
        
        switch (event->key()) {
            case Qt::Key_Space:
                if (player->playbackState() == QMediaPlayer::PlayingState)
                    player->pause();
                else if (!currentFile.isEmpty())
                    player->play();
                else
                    openFile();
                break;
            case Qt::Key_V:
                if (event->modifiers() == Qt::NoModifier) {
                    player->stop();
                }
                break;
            case Qt::Key_C:
                player->pause();
                break;
            case Qt::Key_Z: {
                int curIdx = playlistWindow->currentTrackIndex();
                if (curIdx > 0) {
                    playlistWindow->setCurrentTrackIndex(curIdx - 1);
                    playTrack(playlistWindow->trackAt(curIdx - 1));
                } else {
                    player->setPosition(0);
                }
                break;
            }
            case Qt::Key_B: {
                int curIdx = playlistWindow->currentTrackIndex();
                int count = playlistWindow->trackCount();
                if (curIdx + 1 < count) {
                    playlistWindow->setCurrentTrackIndex(curIdx + 1);
                    playTrack(playlistWindow->trackAt(curIdx + 1));
                }
                break;
            }
            case Qt::Key_X:
                // X = Play (like Windows Winamp)
                if (!currentFile.isEmpty()) player->play();
                else openFile();
                break;
            case Qt::Key_L:
                if (event->modifiers() & Qt::ControlModifier)
                    onPlayLocation();
                else
                    openFile();
                break;
            case Qt::Key_J: {
                if (event->modifiers() & Qt::ControlModifier) {
                    // Ctrl+J = Jump to file (search in playlist)
                    onJumpToFile();
                } else {
                    // J = Jump to time dialog
                    bool ok;
                    QString timeStr = QInputDialog::getText(this, "Jump to Time",
                        "Enter time (MM:SS or seconds):", QLineEdit::Normal, "", &ok);
                    if (ok && !timeStr.isEmpty()) {
                        qint64 jumpMs = 0;
                        if (timeStr.contains(':')) {
                            QStringList parts = timeStr.split(':');
                            if (parts.size() >= 2)
                                jumpMs = (parts[0].toInt() * 60 + parts[1].toInt()) * 1000;
                        } else {
                            jumpMs = timeStr.toInt() * 1000;
                        }
                        player->setPosition(qBound(0LL, jumpMs, player->duration()));
                    }
                }
                break;
            }
            case Qt::Key_D:
                if (event->modifiers() & Qt::ControlModifier) {
                    // Ctrl+D = Toggle double size
                    onToggleDoubleSize();
                }
                break;
            case Qt::Key_W:
                if (event->modifiers() & Qt::ControlModifier) {
                    // Ctrl+W = Toggle windowshade mode
                    onToggleShadeMode();
                }
                break;
            case Qt::Key_T:
                if (event->modifiers() & Qt::ControlModifier) {
                    // Ctrl+T = Toggle always on top
                    onToggleAlwaysOnTop(!alwaysOnTop);
                }
                break;
            case Qt::Key_P:
                if (event->modifiers() & Qt::ControlModifier) {
                    // Ctrl+P = Preferences
                    PreferencesDialog *prefs = new PreferencesDialog(this);
                    connect(prefs, &PreferencesDialog::skinChanged, this, &WinampWindow::onSkinChanged);
                    connect(prefs, &PreferencesDialog::settingChanged, this, [this](const QString &key, const QVariant &value) {
                        if (key == "showNotifications") {
                            showSongNotifications = value.toBool();
                        } else if (key == "aot") {
                            onToggleAlwaysOnTop(value.toBool());
                        } else if (key == "doubleSize") {
                            if (value.toBool() != doubleSize) {
                                doubleSize = value.toBool();
                                setFixedSize(doubleSize ? 550 : 275, doubleSize ? 232 : 116);
                                update();
                            }
                        } else if (key == "stopAfterCurrent") {
                            stopAfterCurrent = value.toBool();
                        }
                    });
                    prefs->setAttribute(Qt::WA_DeleteOnClose);
                    prefs->exec();
                }
                break;
            case Qt::Key_3:
                if (event->modifiers() & Qt::AltModifier) {
                    // Alt+3 = File info dialog (matches Windows WINAMP_EDIT_ID3 / in_infobox)
                    if (!currentFile.isEmpty()) {
                        FileInfoDialog *dlg = new FileInfoDialog(currentFile, player, this);
                        dlg->setAttribute(Qt::WA_DeleteOnClose);
                        dlg->exec();
                    }
                }
                break;
            case Qt::Key_Left:
                player->setPosition(qMax(0LL, player->position() - 5000));
                break;
            case Qt::Key_Right:
                player->setPosition(qMin(player->duration(), player->position() + 5000));
                break;
            case Qt::Key_Up:
                volume = qMin(255, volume + 10);
                applyVolume();
                update();
                break;
            case Qt::Key_Down:
                volume = qMax(0, volume - 10);
                applyVolume();
                update();
                break;
            case Qt::Key_Plus:
            case Qt::Key_Equal:
                volume = qMin(255, volume + 10);
                applyVolume();
                update();
                break;
            case Qt::Key_Minus:
                volume = qMax(0, volume - 10);
                applyVolume();
                update();
                break;
            case Qt::Key_R:
                if (event->modifiers() == Qt::NoModifier) {
                    // R = Toggle repeat
                    repeatOn = !repeatOn;
                    update();
                }
                break;
            case Qt::Key_S:
                if (event->modifiers() == Qt::NoModifier) {
                    // S = Toggle shuffle
                    shuffleOn = !shuffleOn;
                    update();
                }
                break;
            default:
                QWidget::keyPressEvent(event);
                return;
        }
        event->accept();
    }

    void showContextMenu(QPoint globalPos) {
        static const char *menuStyle =
            "QMenu { background-color: #2b2d3d; color: #00ff00; border: 1px solid #555; font-size: 9pt; }"
            "QMenu::item:selected { background-color: #0000c6; }"
            "QMenu::item:checked { font-weight: bold; }"
            "QMenu::item:disabled { color: #666; }"
            "QMenu::separator { height: 1px; background: #555; margin: 2px 4px; }";

        QMenu menu;
        menu.setStyleSheet(menuStyle);
        
        // === Winamp main menu (matching Windows main.cpp top_menu) ===
        
        // -- Play submenu --
        QMenu *playMenu = menu.addMenu("Play");
        playMenu->setStyleSheet(menuStyle);
        QAction *playFileAct = playMenu->addAction("Play file...\tL");
        QAction *playLocAct = playMenu->addAction("Play location...\tCtrl+L");
        playMenu->addSeparator();
        
        // -- Recent files submenu --
        QMenu *recentMenu = playMenu->addMenu("Recent files");
        recentMenu->setStyleSheet(menuStyle);
        auto &recent = RecentFilesManager::instance();
        if (recent.recentFiles.isEmpty()) {
            QAction *empty = recentMenu->addAction("(no recent files)");
            empty->setEnabled(false);
        } else {
            for (int i = 0; i < recent.recentFiles.size(); i++) {
                QAction *a = recentMenu->addAction(
                    QString("%1. %2").arg(i + 1).arg(QFileInfo(recent.recentFiles[i]).fileName()));
                a->setData(recent.recentFiles[i]);
            }
        }
        
        // -- Bookmarks submenu --
        QMenu *bmMenu = menu.addMenu("Bookmarks");
        bmMenu->setStyleSheet(menuStyle);
        QAction *addBmAct = bmMenu->addAction("Add current as bookmark");
        addBmAct->setEnabled(!currentFile.isEmpty());
        bmMenu->addSeparator();
        auto &bmMgr = BookmarkManager::instance();
        for (int i = 0; i < bmMgr.bookmarks.size(); i++) {
            QAction *a = bmMenu->addAction(bmMgr.bookmarks[i].title);
            a->setData("bm:" + QString::number(i));
        }
        if (bmMgr.bookmarks.isEmpty()) {
            QAction *empty = bmMenu->addAction("(no bookmarks)");
            empty->setEnabled(false);
        }
        
        menu.addSeparator();
        
        // -- Options submenu --
        QMenu *optMenu = menu.addMenu("Options");
        optMenu->setStyleSheet(menuStyle);
        QAction *aotAct = optMenu->addAction("Always on top\tCtrl+T");
        aotAct->setCheckable(true);
        aotAct->setChecked(alwaysOnTop);
        
        QAction *dsizeAct = optMenu->addAction("Double size\tCtrl+D");
        dsizeAct->setCheckable(true);
        dsizeAct->setChecked(doubleSize);
        
        QAction *shadeAct = optMenu->addAction("Windowshade mode\tCtrl+W");
        shadeAct->setCheckable(true);
        shadeAct->setChecked(shadeMode);
        
        optMenu->addSeparator();
        QAction *prefsAct = optMenu->addAction("Preferences...\tCtrl+P");
        
        optMenu->addSeparator();
        QAction *stopAfterAct = optMenu->addAction("Stop after current");
        stopAfterAct->setCheckable(true);
        stopAfterAct->setChecked(stopAfterCurrent);
        
        // -- Playback submenu --
        QMenu *pbMenu = menu.addMenu("Playback");
        pbMenu->setStyleSheet(menuStyle);
        QAction *jumpTimeAct = pbMenu->addAction("Jump to time...\tJ");
        QAction *jumpFileAct = pbMenu->addAction("Jump to file...\tCtrl+J");
        pbMenu->addSeparator();
        
        QAction *shuffAct = pbMenu->addAction("Shuffle");
        shuffAct->setCheckable(true);
        shuffAct->setChecked(shuffleOn);
        
        // Repeat submenu
        QMenu *repMenu = pbMenu->addMenu("Repeat");
        repMenu->setStyleSheet(menuStyle);
        QAction *repOffAct = repMenu->addAction("Off");
        repOffAct->setCheckable(true);
        repOffAct->setChecked(!repeatOn);
        QAction *repAllAct = repMenu->addAction("Repeat all");
        repAllAct->setCheckable(true);
        repAllAct->setChecked(repeatOn && !repeatTrack);
        QAction *repOneAct = repMenu->addAction("Repeat track");
        repOneAct->setCheckable(true);
        repOneAct->setChecked(repeatOn && repeatTrack);
        
        // -- Windows submenu --
        QMenu *winMenu = menu.addMenu("Windows");
        winMenu->setStyleSheet(menuStyle);
        QAction *eqTogAct = winMenu->addAction("Equalizer\tAlt+G");
        eqTogAct->setCheckable(true);
        eqTogAct->setChecked(eqBtnOn);
        QAction *plTogAct = winMenu->addAction("Playlist editor\tAlt+E");
        plTogAct->setCheckable(true);
        plTogAct->setChecked(plBtnOn);
        QAction *vidTogAct = winMenu->addAction("Video window");
        vidTogAct->setCheckable(true);
        vidTogAct->setChecked(videoWindow && videoWindow->isVisible());
        QAction *mlTogAct = winMenu->addAction("Media library\tAlt+L");
        mlTogAct->setCheckable(true);
        mlTogAct->setChecked(mediaLibraryWindow && mediaLibraryWindow->isVisible());
        winMenu->addSeparator();
        QAction *milkdropAct = winMenu->addAction("Milkdrop visualization");
        
        // -- Visualization submenu --
        QMenu *visMenu = menu.addMenu("Visualization");
        visMenu->setStyleSheet(menuStyle);
        QAction *visOffAct = visMenu->addAction("Off");
        visOffAct->setCheckable(true);
        visOffAct->setChecked(visMode == 0);
        QAction *visSpecAct = visMenu->addAction("Spectrum analyzer");
        visSpecAct->setCheckable(true);
        visSpecAct->setChecked(visMode == 1);
        QAction *visOscAct = visMenu->addAction("Oscilloscope");
        visOscAct->setCheckable(true);
        visOscAct->setChecked(visMode == 2);
        QAction *visVuAct = visMenu->addAction("VU meter");
        visVuAct->setCheckable(true);
        visVuAct->setChecked(visMode == 3);
        visMenu->addSeparator();
        QAction *visMilkdropAct = visMenu->addAction("Milkdrop visualization...");
        
        menu.addSeparator();
        
        QAction *aboutAct = menu.addAction("About Winamp...");
        menu.addSeparator();
        QAction *quitAct = menu.addAction("Exit");

        // === Handle selection ===
        QAction *sel = menu.exec(globalPos);
        if (!sel) return;
        
        if (sel == playFileAct) onPlayFile();
        else if (sel == playLocAct) onPlayLocation();
        else if (sel == addBmAct) onAddBookmark();
        else if (sel == aotAct) onToggleAlwaysOnTop(sel->isChecked());
        else if (sel == dsizeAct) onToggleDoubleSize();
        else if (sel == shadeAct) onToggleShadeMode();
        else if (sel == stopAfterAct) stopAfterCurrent = sel->isChecked();
        else if (sel == prefsAct) {
            PreferencesDialog *prefs = new PreferencesDialog(this);
            connect(prefs, &PreferencesDialog::skinChanged, this, &WinampWindow::onSkinChanged);
            connect(prefs, &PreferencesDialog::settingChanged, this, [this](const QString &key, const QVariant &value) {
                if (key == "showNotifications") {
                    showSongNotifications = value.toBool();
                } else if (key == "aot") {
                    onToggleAlwaysOnTop(value.toBool());
                } else if (key == "doubleSize") {
                    if (value.toBool() != doubleSize) {
                        doubleSize = value.toBool();
                        setFixedSize(doubleSize ? 550 : 275, doubleSize ? 232 : 116);
                        update();
                    }
                } else if (key == "stopAfterCurrent") {
                    stopAfterCurrent = value.toBool();
                }
            });
            prefs->setAttribute(Qt::WA_DeleteOnClose);
            prefs->exec();
        }
        else if (sel == jumpTimeAct) {
            bool ok;
            QString timeStr = QInputDialog::getText(this, "Jump to Time",
                "Enter time (MM:SS or seconds):", QLineEdit::Normal, "", &ok);
            if (ok && !timeStr.isEmpty()) {
                qint64 jumpMs = 0;
                if (timeStr.contains(':')) {
                    QStringList parts = timeStr.split(':');
                    if (parts.size() >= 2)
                        jumpMs = (parts[0].toInt() * 60 + parts[1].toInt()) * 1000;
                } else {
                    jumpMs = timeStr.toInt() * 1000;
                }
                player->setPosition(qBound(0LL, jumpMs, player->duration()));
            }
        }
        else if (sel == jumpFileAct) onJumpToFile();
        else if (sel == shuffAct) { shuffleOn = sel->isChecked(); update(); }
        else if (sel == repOffAct) { repeatOn = false; repeatTrack = false; update(); }
        else if (sel == repAllAct) { repeatOn = true; repeatTrack = false; update(); }
        else if (sel == repOneAct) { repeatOn = true; repeatTrack = true; update(); }
        else if (sel == eqTogAct) {
            eqBtnOn = sel->isChecked();
            if (eqBtnOn) eqWindow->show(); else eqWindow->hide();
            update();
        }
        else if (sel == plTogAct) {
            plBtnOn = sel->isChecked();
            if (plBtnOn) playlistWindow->show(); else playlistWindow->hide();
            update();
        }
        else if (sel == vidTogAct) {
            if (sel->isChecked()) videoWindow->show(); else videoWindow->hide();
        }
        else if (sel == mlTogAct) {
            if (sel->isChecked()) mediaLibraryWindow->show(); else mediaLibraryWindow->hide();
        }
        else if (sel == milkdropAct) openMilkdrop();
        else if (sel == visOffAct) { visMode = 0; update(); }
        else if (sel == visSpecAct) { visMode = 1; update(); }
        else if (sel == visOscAct) { visMode = 2; update(); }
        else if (sel == visVuAct) { visMode = 3; update(); }
        else if (sel == visMilkdropAct) openMilkdrop();
        else if (sel == aboutAct) onShowAbout();
        else if (sel == quitAct) close();
        // Recent files
        else if (sel->data().toString().startsWith("/") || sel->data().toString().startsWith("file:")) {
            QString path = sel->data().toString();
            if (QFile::exists(path)) {
                playFile(path);
            }
        }
        // Bookmarks
        else if (sel->data().toString().startsWith("bm:")) {
            int idx = sel->data().toString().mid(3).toInt();
            if (idx >= 0 && idx < bmMgr.bookmarks.size()) {
                QString path = bmMgr.bookmarks[idx].path;
                if (path.startsWith("http://") || path.startsWith("https://"))
                    playUrl(path);
                else if (QFile::exists(path))
                    playFile(path);
            }
        }
    }

    void mousePressEvent(QMouseEvent *event) override {
        if (event->button() == Qt::RightButton) {
            int x = event->pos().x();
            int y = event->pos().y();
            // Right-click on repeat button: show repeat mode menu
            if (x >= 210 && x < 238 && y >= 89 && y < 104) {
                QMenu menu;
                menu.setStyleSheet(
                    "QMenu { background-color: #2b2d3d; color: #00ff00; border: 1px solid #555; font-size: 9pt; }"
                    "QMenu::item:selected { background-color: #0000c6; }"
                    "QMenu::item:checked { font-weight: bold; }"
                );
                QAction *repOffAct = menu.addAction("Repeat off");
                repOffAct->setCheckable(true);
                repOffAct->setChecked(!repeatOn);
                QAction *repAllAct = menu.addAction("Repeat all");
                repAllAct->setCheckable(true);
                repAllAct->setChecked(repeatOn && !repeatTrack);
                QAction *repOneAct = menu.addAction("Repeat track");
                repOneAct->setCheckable(true);
                repOneAct->setChecked(repeatOn && repeatTrack);
                QAction *sel = menu.exec(event->globalPosition().toPoint());
                if (sel == repOffAct) { repeatOn = false; repeatTrack = false; }
                else if (sel == repAllAct) { repeatOn = true; repeatTrack = false; }
                else if (sel == repOneAct) { repeatOn = true; repeatTrack = true; }
                update();
                return;
            }
            showContextMenu(event->globalPosition().toPoint());
            return;
        }
        int x = event->pos().x();
        int y = event->pos().y();
        
        // Time display area click: toggle elapsed/remaining
        if (x >= 36 && x < 99 && y >= 26 && y < 40) {
            showRemainingTime = !showRemainingTime;
            update();
            return;
        }
        
        // Visualization area click: (27,40)-(99,61)
        // Single click: cycle modes 0->1->2->3->0 (off/spectrum/osc/vu)
        // (double-click opens Milkdrop — handled in mouseDoubleClickEvent)
        if (x >= 27 && x < 99 && y >= 40 && y < 61) {
            visMode++;
            if (visMode > 3) visMode = 0;
            // Reset viz state when switching modes
            memset(saBarHeight, 0, sizeof(saBarHeight));
            memset(saPeakHeight, 0, sizeof(saPeakHeight));
            memset(saPeakVel, 0, sizeof(saPeakVel));
            update();
            return;
        }
        
        // Clutterbar toggle/buttons (matches Windows Ui.cpp do_clutterbar)
        // Toggle bar: x=10-18, y=22-30
        if (x >= 10 && x < 18 && y >= 22 && y < 30) {
            clutterbarOpen = !clutterbarOpen;
            update();
            return;
        }
        // Clutterbar buttons (only when open)
        if (clutterbarOpen && x >= 11 && x < 18) {
            // AOT (Always On Top) button: y=33-41
            if (y >= 33 && y < 41) {
                alwaysOnTop = !alwaysOnTop;
                setWindowFlag(Qt::WindowStaysOnTopHint, alwaysOnTop);
                show(); // Re-show to apply flag change
                update();
                return;
            }
            // File Info button: y=42-49
            if (y >= 42 && y < 49) {
                // Show file info dialog (same as Alt+3)
                if (!currentFile.isEmpty()) {
                    FileInfoDialog *dlg = new FileInfoDialog(currentFile, player, this);
                    dlg->show();
                }
                return;
            }
            // Double Size button: y=49-55
            if (y >= 49 && y < 55) {
                doubleSize = !doubleSize;
                if (doubleSize) {
                    setFixedSize(275 * 2, 116 * 2);
                } else {
                    setFixedSize(275, 116);
                }
                update();
                return;
            }
            // Visualization menu button: y=58-65 (TODO: implement vis menu)
            if (y >= 58 && y < 65) {
                // Could show visualization options menu
                update();
                return;
            }
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
            applyVolume();
            update();
            return;
        }
        
        // Balance slider: (177,57) to (215,70) 
        if (x >= 177 && x <= 215 && y >= 57 && y <= 70) {
            isDraggingBalance = true;
            balance = ((x - 177) * 254) / 38 - 127;
            balance = qBound(-127, balance, 127);
            // Apply stereo balance via QAudioOutput
            // Qt6 doesn't have direct balance, but we can approximate with stereo channel volumes
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
        
        // Show tooltips for controls (matching Windows tooltips)
        QString tooltip;
        if (y >= 88 && y <= 106) {
            if (x >= 16 && x < 39) tooltip = "Previous Track";
            else if (x >= 39 && x < 62) tooltip = "Play";
            else if (x >= 62 && x < 85) tooltip = "Pause";
            else if (x >= 85 && x < 108) tooltip = "Stop";
            else if (x >= 108 && x < 130) tooltip = "Next Track";
        } else if (y >= 89 && y <= 105 && x >= 136 && x < 158) {
            tooltip = "Eject / Open File";
        } else if (y >= 89 && y <= 104) {
            if (x >= 164 && x < 211) tooltip = "Toggle Shuffle";
            else if (x >= 210 && x < 238) tooltip = "Toggle Repeat";
        } else if (y >= 58 && y <= 70) {
            if (x >= 219 && x < 242) tooltip = "Toggle Equalizer";
            else if (x >= 242 && x < 265) tooltip = "Toggle Playlist";
        } else if (y >= 57 && y <= 70) {
            if (x >= 107 && x <= 175) tooltip = "Volume Control";
            else if (x >= 177 && x <= 215) tooltip = "Balance/Pan Control";
        } else if (y >= 72 && y <= 82 && x >= 16 && x <= 264) {
            tooltip = "Seek Position";
        } else if (y >= 26 && y < 40 && x >= 36 && x < 99) {
            tooltip = "Time Display (click to toggle)";
        } else if (y >= 40 && y < 61 && x >= 27 && x < 99) {
            tooltip = "Visualization (click to cycle modes, double-click for Milkdrop)";
        } else if (y >= 22 && y < 30 && x >= 10 && x < 18) {
            tooltip = "Toggle Clutterbar";
        }
        
        if (!tooltip.isEmpty()) {
            QToolTip::showText(event->globalPosition().toPoint(), tooltip, this);
        } else {
            QToolTip::hideText();
        }
        
        // Volume drag
        if (isDraggingVolume) {
            volume = ((x - 107) * 255) / 68;
            if (volume > 255) volume = 255;
            if (volume < 0) volume = 0;
            applyVolume();
            update();
        }
        
        // Balance drag
        if (isDraggingBalance) {
            balance = ((x - 177) * 254) / 38 - 127;
            balance = qBound(-127, balance, 127);
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
        isDraggingBalance = false;
        isDraggingPos = false;
        isDragging = false;
    }
    
    void openFile() {
        QString fileName = QFileDialog::getOpenFileName(this, "Open Audio File", "",
            "Audio Files (*.mp3 *.wav *.flac *.ogg *.m4a *.aac *.wma);;All Files (*)");
        if (!fileName.isEmpty()) {
            currentFile = fileName;
            player->setSource(QUrl::fromLocalFile(fileName));
            player->play();
            playlistWindow->addTrack(fileName);
            RecentFilesManager::instance().addFile(fileName);
        }
    }
    
    // Drag-and-drop on main window (matches Windows ole.cpp)
    void dragEnterEvent(QDragEnterEvent *event) override {
        if (event->mimeData()->hasUrls())
            event->acceptProposedAction();
    }
    
    void dropEvent(QDropEvent *event) override {
        const QMimeData *mimeData = event->mimeData();
        if (mimeData->hasUrls()) {
            QList<QUrl> urls = mimeData->urls();
            for (const QUrl &url : urls) {
                QString path = url.toLocalFile();
                if (!path.isEmpty()) {
                    QFileInfo fi(path);
                    if (fi.isDir()) {
                        // Add all audio files from directory
                        QDir dir(path);
                        QStringList filters = {"*.mp3", "*.wav", "*.flac", "*.ogg", "*.m4a", "*.aac"};
                        QStringList files = dir.entryList(filters, QDir::Files, QDir::Name);
                        for (const QString &f : files)
                            playlistWindow->addTrack(dir.absoluteFilePath(f));
                    } else {
                        playlistWindow->addTrack(path);
                    }
                }
            }
            // Play the first dropped file
            if (!urls.isEmpty()) {
                QString firstPath = urls.first().toLocalFile();
                if (!firstPath.isEmpty() && QFileInfo(firstPath).isFile()) {
                    playFile(firstPath);
                }
            }
            event->acceptProposedAction();
        }
    }
    
    // Preload the next track for smoother transitions (gapless playback)
    void preloadNextTrack() {
        int curIdx = playlistWindow->currentTrackIndex();
        int count = playlistWindow->trackCount();
        if (count == 0) return;
        
        int nextIdx;
        if (shuffleOn) {
            // For shuffle, we can't really preload since it's random
            return;
        } else {
            nextIdx = curIdx + 1;
        }
        
        if (nextIdx < count) {
            QString nextFile = playlistWindow->trackAt(nextIdx);
            if (!nextFile.isEmpty() && QFile::exists(nextFile)) {
                nextPlayer->setSource(QUrl::fromLocalFile(nextFile));
                // Don't play yet, just preload
            }
        } else if (repeatOn && count > 0) {
            // If repeat all is on, preload first track
            QString nextFile = playlistWindow->trackAt(0);
            if (!nextFile.isEmpty() && QFile::exists(nextFile)) {
                nextPlayer->setSource(QUrl::fromLocalFile(nextFile));
            }
        }
    }
    
public:
    void playTrack(const QString &fileName) {
        if (!fileName.isEmpty() && QFile::exists(fileName)) {
            currentFile = fileName;
            // Reset media info — will be refreshed by metaDataChanged and processAudioBuffer
            mediaBitrate = 0;
            mediaSampleRate = 0;
            mediaChannels = 0;
            metaTitle.clear();
            
            // Auto-load EQ preset if AUTO is enabled (matches Windows eq_autoload from Play.cpp line 58)
            if (eqWindow && eqWindow->isAutoEnabled()) {
                eqWindow->autoLoadPreset(fileName);
            }
            
            player->setSource(QUrl::fromLocalFile(fileName));
            player->play();
            RecentFilesManager::instance().addFile(fileName);
            updateTrayTooltip();
            
            // Show song change notification (matches Windows balloon tooltips)
            if (showSongNotifications && trayIcon) {
                QString title = metaTitle.isEmpty() ? QFileInfo(fileName).completeBaseName() : metaTitle;
                trayIcon->showMessage("Winamp", title, QSystemTrayIcon::Information, 3000);
            }
            
            // Preload next track for gapless playback
            preloadNextTrack();
        }
    }
    
    // Apply volume to audio outputs — respects EQ DSP path
    // When EQ DSP is active, volume is applied in the DSP chain, not via QAudioOutput
    void applyVolume() {
        if (!eqDspActive) {
            audioOutput->setVolume(volume / 255.0f);
        }
        // nextAudioOutput always gets volume (it plays before DSP takes over)
        nextAudioOutput->setVolume(volume / 255.0f);
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
        s.setValue("balance", balance);
        s.setValue("shuffle", shuffleOn);
        s.setValue("repeat", repeatOn);
        s.setValue("repeatTrack", repeatTrack);
        s.setValue("eqVisible", eqBtnOn);
        s.setValue("plVisible", plBtnOn);
        s.setValue("visMode", visMode);
        s.setValue("showRemainingTime", showRemainingTime);
        if (!currentFile.isEmpty()) {
            s.setValue("lastFile", currentFile);
        }
        s.endGroup();
        
        s.beginGroup("WindowState");
        s.setValue("alwaysOnTop", alwaysOnTop);
        s.setValue("doubleSize", doubleSize);
        s.setValue("shadeMode", shadeMode);
        s.setValue("stopAfterCurrent", stopAfterCurrent);
        s.setValue("showSongNotifications", showSongNotifications);
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
        balance = s.value("balance", 0).toInt();
        applyVolume();
        shuffleOn = s.value("shuffle", false).toBool();
        repeatOn = s.value("repeat", false).toBool();
        repeatTrack = s.value("repeatTrack", false).toBool();
        eqBtnOn = s.value("eqVisible", false).toBool();
        plBtnOn = s.value("plVisible", true).toBool();  // Show playlist by default for testing
        visMode = s.value("visMode", 1).toInt();
        showRemainingTime = s.value("showRemainingTime", false).toBool();
        QString lastFile = s.value("lastFile").toString();
        if (!lastFile.isEmpty() && QFile::exists(lastFile)) {
            currentFile = lastFile;
        }
        s.endGroup();
        
        s.beginGroup("WindowState");
        alwaysOnTop = s.value("alwaysOnTop", false).toBool();
        doubleSize = s.value("doubleSize", false).toBool();
        shadeMode = s.value("shadeMode", false).toBool();
        stopAfterCurrent = s.value("stopAfterCurrent", false).toBool();
        showSongNotifications = s.value("showSongNotifications", true).toBool();
        if (alwaysOnTop) {
            setWindowFlag(Qt::WindowStaysOnTopHint, true);
            show();
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
    QMediaPlayer *nextPlayer;  // Preload next track for gapless playback
    QAudioOutput *nextAudioOutput;
    bool usingNextPlayer;  // Track which player is active
    
    // Real EQ DSP processing (ported from Windows eq10dsp.cpp / In.cpp)
    // Audio flow: QMediaPlayer → QAudioBufferOutput → EQ10 DSP → QAudioSink
    QAudioSink *audioSink = nullptr;
    QIODevice *audioSinkDevice = nullptr;
    eq10_t eqState[2];     // EQ filter state per channel (stereo max)
    int eqSampleRate = 0;  // Current configured sample rate
    int eqChannels = 0;    // Current configured channel count
    bool eqDspActive = false; // Whether DSP path is active
    QTimer *timer;
    QTimer *scrollTimer;
    QString currentFile;
    QPoint dragPosition;
    bool isDragging;
    int volume;  // 0-255 like original
    int hoveredButton;
    int pressedButton;
    bool shuffleOn, repeatOn, eqBtnOn, plBtnOn;
    bool repeatTrack; // repeat-one mode (like Windows "manual playlist advance")
    bool stopAfterCurrent; // stop after current track finishes (like Windows g_stopaftercur)
    bool isDraggingVolume, isDraggingPos, isDraggingBalance;
    int scrollOffset;
    int balance; // -127 (left) to +127 (right), 0 = center (matches Windows pan127)
    bool doubleSize;   // 2x scaling mode (like Windows config_dsize)
    bool shadeMode;    // compact shade mode (like Windows config_windowshade)
    bool alwaysOnTop;  // always on top (like Windows config_aot)
    bool clutterbarOpen; // clutterbar expanded (left side O/A/I/D/V buttons)
    bool showSongNotifications = true; // show desktop notification on song change
    
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
    bool showRemainingTime = false; // Toggle elapsed/remaining time display
    QString metaTitle;              // Song title from metadata (Artist - Title)
    
    // VU meter data (matches Windows vis_VU)
    float vuData[2];   // RMS levels for L/R channels (0.0 - 1.0)
    
    // Easter egg state
    char eggStr[9];
    int eggStat;
    
    // System tray icon (matches Windows SYSTRAY.cpp)
    QSystemTrayIcon *trayIcon = nullptr;
    QMenu *trayMenu = nullptr;
    
    PlaylistWindow *playlistWindow;
    EqualizerWindow *eqWindow;
    VideoWindow *videoWindow = nullptr;
    MilkdropWindow *milkdropWindow = nullptr;
    MediaLibraryWindow *mediaLibraryWindow = nullptr;
    
    void openMilkdrop() {
        if (milkdropWindow) {
            milkdropWindow->raise();
            milkdropWindow->activateWindow();
            return;
        }
        milkdropWindow = new MilkdropWindow();
        connect(milkdropWindow, &QObject::destroyed, this, [this]() {
            milkdropWindow = nullptr;
        });
        connect(milkdropWindow, &MilkdropWindow::fullscreenChanged, this, [this](bool fs) {
                if (fs) {
                    // Hide all Winamp windows when Milkdrop goes fullscreen
                    eqWasVisible = eqWindow && eqWindow->isVisible();
                    plWasVisible = playlistWindow && playlistWindow->isVisible();
                    mainWasVisible = isVisible();
                    if (eqWindow) eqWindow->hide();
                    if (playlistWindow) playlistWindow->hide();
                    hide();
                } else {
                    // Restore windows when exiting fullscreen
                    if (mainWasVisible) show();
                    if (eqWasVisible && eqWindow) eqWindow->show();
                    if (plWasVisible && playlistWindow) playlistWindow->show();
                }
            });
        milkdropWindow->show();
        milkdropWindow->raise();
        milkdropWindow->activateWindow();
    }
    bool eqWasVisible = false;
    bool plWasVisible = false;
    bool mainWasVisible = true;
    
    void setupSystemTray() {
        if (!QSystemTrayIcon::isSystemTrayAvailable()) return;
        
        trayIcon = new QSystemTrayIcon(this);
        trayIcon->setIcon(windowIcon().isNull() ? QIcon::fromTheme("audio-headphones") : windowIcon());
        trayIcon->setToolTip("Winamp 5.666 for Linux");
        
        trayMenu = new QMenu(this);
        trayMenu->addAction("Winamp", this, [this]() {
            show();
            raise();
            activateWindow();
        });
        trayMenu->addSeparator();
        trayMenu->addAction("Previous Track", this, [this]() {
            if (playlistWindow) playlistWindow->prevTrack();
        });
        QAction *playPauseAction = trayMenu->addAction("Play", this, [this]() {
            if (player->playbackState() == QMediaPlayer::PlayingState) {
                player->pause();
            } else if (player->playbackState() == QMediaPlayer::PausedState) {
                player->play();
            } else if (playlistWindow && playlistWindow->trackCount() > 0) {
                playlistWindow->playCurrentTrack();
            }
        });
        Q_UNUSED(playPauseAction);
        trayMenu->addAction("Stop", this, [this]() { player->stop(); update(); });
        trayMenu->addAction("Next Track", this, [this]() {
            if (playlistWindow) playlistWindow->nextTrack();
        });
        trayMenu->addSeparator();
        trayMenu->addAction("Open File...", this, [this]() { openFile(); });
        trayMenu->addSeparator();
        trayMenu->addAction("Exit", qApp, &QApplication::quit);
        
        trayIcon->setContextMenu(trayMenu);
        
        connect(trayIcon, &QSystemTrayIcon::activated, this, [this](QSystemTrayIcon::ActivationReason reason) {
            if (reason == QSystemTrayIcon::Trigger || reason == QSystemTrayIcon::DoubleClick) {
                if (isVisible()) {
                    hide();
                    if (eqWindow) eqWindow->hide();
                    if (playlistWindow) playlistWindow->hide();
                } else {
                    show();
                    raise();
                    activateWindow();
                    if (eqBtnOn && eqWindow) eqWindow->show();
                    if (plBtnOn && playlistWindow) playlistWindow->show();
                }
            }
        });
        
        trayIcon->show();
    }
    
    void updateTrayTooltip() {
        if (!trayIcon) return;
        QString tip = "Winamp";
        if (!metaTitle.isEmpty()) {
            tip = metaTitle;
        } else if (!currentFile.isEmpty()) {
            tip = QFileInfo(currentFile).completeBaseName();
        }
        // Tray tooltip is limited to 127 chars on most platforms
        if (tip.length() > 120) tip = tip.left(117) + "...";
        trayIcon->setToolTip(tip);
    }
    
    void mouseDoubleClickEvent(QMouseEvent *event) override {
        int x = event->pos().x();
        int y = event->pos().y();
        // Double-click on visualization area opens Milkdrop
        if (x >= 27 && x < 99 && y >= 40 && y < 61) {
            openMilkdrop();
            return;
        }
        QWidget::mouseDoubleClickEvent(event);
    }
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

// MPRIS2 out-of-line method implementations (need full WinampWindow definition)
#ifdef QT_DBUS_LIB
void Mpris2PlayerAdaptor::Next() {
    WinampWindow *w = qobject_cast<WinampWindow*>(parent());
    if (w) {
        PlaylistWindow *pl = w->getPlaylistWindow();
        if (pl) pl->nextTrack();
    }
}
void Mpris2PlayerAdaptor::Previous() {
    WinampWindow *w = qobject_cast<WinampWindow*>(parent());
    if (w) {
        PlaylistWindow *pl = w->getPlaylistWindow();
        if (pl) pl->prevTrack();
    }
}
void Mpris2PlayerAdaptor::OpenUri(const QString &uri) {
    QUrl url(uri);
    if (url.isLocalFile()) {
        WinampWindow *w = qobject_cast<WinampWindow*>(parent());
        if (w) w->playTrack(url.toLocalFile());
    }
}
#endif

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);
    app.setApplicationName("Winamp");
    app.setApplicationVersion("5.666");
    app.setOrganizationName("Nullsoft");

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
    
    // Load language pack
    QString langCode = settings.value("language", "en").toString();
    Translator::instance().loadLanguage(langCode);

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

    // Splash screen (matches Windows SPLASH.cpp)
    QPixmap splashPix;
    QSplashScreen *splash = nullptr;
    // Look for SPLASH.BMP in the loaded skin or fallback paths
    QStringList splashCandidates = candidates;
    splashCandidates << appDir + "/../Src/resources" << appDir + "/../../Src/resources";
    for (const QString &path : splashCandidates) {
        QString splashFile = QDir(path).filePath("SPLASH.BMP");
        if (QFile::exists(splashFile)) {
            splashPix.load(splashFile);
            break;
        }
        // Try lowercase
        splashFile = QDir(path).filePath("splash.bmp");
        if (QFile::exists(splashFile)) {
            splashPix.load(splashFile);
            break;
        }
    }
    if (!splashPix.isNull()) {
        splash = new QSplashScreen(splashPix);
        splash->show();
        app.processEvents();
    }

    WinampWindow w;
    
    // Process command-line arguments (matches Windows cmdline.cpp)
    // Supported: file paths to play, directories to scan, -play, -pause, -stop, -enqueue
    QStringList filesToPlay;
    bool enqueueMode = false;
    for (int i = 1; i < argc; i++) {
        QString arg = QString::fromLocal8Bit(argv[i]);
        if (arg == "-enqueue" || arg == "--enqueue") {
            enqueueMode = true;
        } else if (arg == "-play" || arg == "--play") {
            // Will auto-play after adding files
        } else if (arg == "-pause" || arg == "--pause") {
            QTimer::singleShot(100, &w, [&w]() {
                if (w.getPlayer()->playbackState() == QMediaPlayer::PlayingState)
                    w.getPlayer()->pause();
            });
        } else if (arg == "-stop" || arg == "--stop") {
            QTimer::singleShot(100, &w, [&w]() { w.getPlayer()->stop(); });
        } else if (arg.startsWith("-")) {
            // Unknown flag — ignore
        } else {
            // Treat as file or directory path
            QFileInfo fi(arg);
            if (fi.isDir()) {
                // Scan directory for audio files
                QDir dir(arg);
                QStringList audioExts = {"*.mp3", "*.wav", "*.ogg", "*.flac", "*.m4a", "*.aac", "*.wma", "*.opus"};
                for (const QFileInfo &entry : dir.entryInfoList(audioExts, QDir::Files, QDir::Name))
                    filesToPlay << entry.absoluteFilePath();
            } else if (fi.exists()) {
                filesToPlay << fi.absoluteFilePath();
            }
        }
    }
    
    // Add CLI files to playlist and optionally play
    if (!filesToPlay.isEmpty()) {
        PlaylistWindow *pl = w.getPlaylistWindow();
        if (pl) {
            if (!enqueueMode) {
                // Clear existing playlist before adding CLI files
                // (enqueue mode preserves existing playlist)
            }
            for (const QString &f : filesToPlay)
                pl->addTrack(f);
            // Auto-play first file unless in enqueue mode
            if (!enqueueMode && pl->trackCount() > 0) {
                pl->setCurrentTrackIndex(0);
                w.playTrack(pl->trackAt(0));
            }
        }
    }
    
    if (splash) {
        QTimer::singleShot(1500, splash, &QSplashScreen::close);
    }
    
    w.show();
    
    return app.exec();
}

#include "winamp_authentic.moc"
