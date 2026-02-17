#include "equalizerwindow.h"
#include "winampwindow.h"
#include "winampbitmaps.h"
#include "modernskinengine.h"
#include "eqpresets.h"
#include "translator.h"
#include "skinutils.h"

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

void EqualizerWindow::paintEvent(QPaintEvent *) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, false);

    if (g_isModernSkin && g_modernSkin) {
        paintModernEQ(p);
        return;
    }

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
void EqualizerWindow::drawEqFrequencyCurve(QPainter &p) {
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

void EqualizerWindow::drawEqSlider(QPainter &p, int which, int destX) {
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

void EqualizerWindow::paintModernEQ(QPainter &p) {
    auto &ms = *g_modernSkin;
    int w = width();
    int h = height();

    // ---- Base texture fill ----
    QPixmap baseTex = ms.getBitmap("wasabi.frame.basetexture");
    if (!baseTex.isNull()) {
        for (int ty = 0; ty < h; ty += baseTex.height())
            for (int tx = 0; tx < w; tx += baseTex.width())
                p.drawPixmap(tx, ty, baseTex);
    } else {
        p.fillRect(0, 0, w, h, QColor(43, 45, 61));
    }

    // ---- Titlebar (18px) ----
    QPixmap tbLeft = ms.getBitmap("wasabi.frame.top.left");
    QPixmap tbCenter = ms.getBitmap("wasabi.frame.top");
    QPixmap tbRight = ms.getBitmap("wasabi.frame.top.right");

    if (!tbLeft.isNull()) p.drawPixmap(0, 0, tbLeft);
    if (!tbCenter.isNull()) {
        for (int tx = tbLeft.width(); tx < w - tbRight.width(); tx += tbCenter.width()) {
            int tw = qMin(tbCenter.width(), w - tbRight.width() - tx);
            p.drawPixmap(tx, 0, tbCenter, 0, 0, tw, tbCenter.height());
        }
    }
    if (!tbRight.isNull()) p.drawPixmap(w - tbRight.width(), 0, tbRight);

    // Titlebar text background
    QPixmap tbTextLeft = ms.getBitmap(isActiveWindow() ?
        "wasabi.titlebar.left.active" : "wasabi.titlebar.left.inactive");
    QPixmap tbTextCenter = ms.getBitmap(isActiveWindow() ?
        "wasabi.titlebar.center.active" : "wasabi.titlebar.center.inactive");
    QPixmap tbTextRight = ms.getBitmap(isActiveWindow() ?
        "wasabi.titlebar.right.active" : "wasabi.titlebar.right.inactive");

    if (!tbTextLeft.isNull()) p.drawPixmap(10, 5, tbTextLeft);
    if (!tbTextCenter.isNull()) {
        for (int tx = 20; tx < w - 55; tx += tbTextCenter.width()) {
            int tw = qMin(tbTextCenter.width(), w - 55 - tx);
            p.drawPixmap(tx, 5, tbTextCenter, 0, 0, tw, tbTextCenter.height());
        }
    }
    if (!tbTextRight.isNull()) p.drawPixmap(w - 55, 5, tbTextRight);

    // Title text
    p.setPen(QColor(200, 200, 220));
    p.setFont(QFont("Arial", 7, QFont::Bold));
    p.drawText(15, 14, "EQUALIZER");

    // Close button
    QPixmap closeBg = ms.getBitmap("wasabi.button.bg.title");
    QPixmap closeBtn = ms.getBitmap("wasabi.button.exit");
    if (!closeBg.isNull()) p.drawPixmap(w - 18, 4, closeBg);
    if (!closeBtn.isNull()) p.drawPixmap(w - 17, 4, closeBtn);

    // ---- EQ content area ----
    // Center the 318px-wide EQ content in the window
    const int cx = (w - 318) / 2;  // content X offset (centered)
    const int cy = 18;              // content Y offset (below titlebar)

    // EQ drawer background (318x89)
    QPixmap eqBg = ms.getBitmap("drawer.eq.bg");
    // Fill the content area below titlebar with dark bg
    p.fillRect(6, cy, w - 12, 89, QColor(27, 28, 40));
    if (!eqBg.isNull()) {
        p.drawPixmap(cx, cy, eqBg);
    }

    // Text overlays
    QPixmap txtDark = ms.getBitmap("drawer.eq.txtoverlay.dark");
    QPixmap txtBright = ms.getBitmap("drawer.eq.txtoverlay.bright");
    if (!txtDark.isNull()) p.drawPixmap(cx, cy, txtDark);
    if (!txtBright.isNull()) p.drawPixmap(cx, cy + 82, txtBright);

    // Frequency labels (ISO band names at bottom)
    QPixmap labelIso = ms.getBitmap("drawer.eq.label.iso");
    if (!labelIso.isNull()) {
        p.drawPixmap(cx + 135, cy + 82, labelIso);
    }

    // Side borders
    QPixmap plL = ms.getBitmap("player.pl.left");
    QPixmap plR = ms.getBitmap("player.pl.right");
    for (int by = cy; by < h - 6; by += 5) {
        int bh = qMin(5, h - 6 - by);
        if (!plL.isNull()) p.drawPixmap(0, by, plL, 0, 0, 6, bh);
        if (!plR.isNull()) p.drawPixmap(w - 6, by, plR, 0, 0, 6, bh);
    }

    // Bottom border
    QPixmap plBC = ms.getBitmap("player.pl.bottomcenter");
    if (!plBC.isNull()) {
        for (int tx = 6; tx < w - 6; tx += plBC.width()) {
            int tw = qMin(plBC.width(), w - 6 - tx);
            p.drawPixmap(tx, h - 6, plBC, 0, 0, tw, 6);
        }
    }

    // ---- EQ Sliders (positions from configdrawer.xml) ----
    // All sliders: w=13, h=80, y=1 (relative to content)
    // Preamp at x=82; bands at x=134,152,170,188,206,224,242,260,278,296
    const int sliderW = 13;
    const int sliderH = 80;
    const int sliderRelY = 1;
    const int preampX = 82;
    const int bandStartX = 134;
    const int bandSpacing = 18;

    QPixmap thumb = ms.getBitmap("player.main.eq.button");
    QPixmap thumbHover = ms.getBitmap("player.main.eq.button.hover");
    int thumbH = thumb.isNull() ? 11 : thumb.height(); // 23px
    int thumbW = thumb.isNull() ? 13 : thumb.width();   // 13px

    auto drawSlider = [&](int sliderIdx, int relX) {
        int pos = (sliderIdx == 0) ? preampValue : eqValues[sliderIdx - 1];
        int absX = cx + relX;
        int absY = cy + sliderRelY;

        // Groove line
        int grooveCX = absX + sliderW / 2;
        p.setPen(QColor(20, 21, 35, 120));
        p.drawLine(grooveCX, absY, grooveCX, absY + sliderH);

        // Thumb position: pos 63=top, 0=bottom
        int travel = sliderH - thumbH;
        int thumbY = absY + travel - (pos * travel) / 63;

        if (!thumb.isNull()) {
            p.drawPixmap(absX, thumbY, thumb);
        } else {
            // Fallback thumb
            p.setPen(QColor(80, 85, 120));
            p.setBrush(QColor(55, 58, 80));
            p.drawRoundedRect(absX, thumbY, 13, 11, 2, 2);
            p.setBrush(Qt::NoBrush);
        }
    };

    // Preamp slider
    drawSlider(0, preampX);

    // 10 band sliders
    for (int i = 0; i < 10; i++) {
        drawSlider(i + 1, bandStartX + i * bandSpacing);
    }

    // ---- ON / AUTO / PRESETS buttons (from configdrawer.xml) ----
    // ON at (16, 47), AUTO at (16, 61), PRESETS at (16, 75) relative to content
    QPixmap onBtn = ms.getBitmap("drawer.eq.button.on");
    QPixmap autoBtn = ms.getBitmap("drawer.eq.button.auto");
    QPixmap presetsBtn = ms.getBitmap("drawer.eq.button.presets");

    if (!onBtn.isNull()) {
        p.drawPixmap(cx + 16, cy + 47, onBtn);
    } else {
        p.setPen(QColor(160, 160, 180));
        p.setFont(QFont("Arial", 6, QFont::Bold));
        p.drawText(cx + 16, cy + 54, "ON");
    }
    // ON LED indicator
    p.setPen(Qt::NoPen);
    p.setBrush(eqEnabled ? QColor(0, 220, 0) : QColor(50, 50, 70));
    p.drawEllipse(cx + 36, cy + 46, 5, 5);
    p.setBrush(Qt::NoBrush);

    if (!autoBtn.isNull()) {
        p.drawPixmap(cx + 16, cy + 61, autoBtn);
    } else {
        p.setPen(QColor(160, 160, 180));
        p.setFont(QFont("Arial", 6, QFont::Bold));
        p.drawText(cx + 16, cy + 68, "AUTO");
    }
    // AUTO LED indicator
    p.setPen(Qt::NoPen);
    p.setBrush(autoEnabled ? QColor(0, 220, 0) : QColor(50, 50, 70));
    p.drawEllipse(cx + 49, cy + 60, 5, 5);
    p.setBrush(Qt::NoBrush);

    if (!presetsBtn.isNull()) {
        p.drawPixmap(cx + 16, cy + 75, presetsBtn);
    } else {
        p.setPen(QColor(160, 160, 180));
        p.setFont(QFont("Arial", 6, QFont::Bold));
        p.drawText(cx + 16, cy + 82, "PRESETS");
    }

    // ---- EQ scale labels (+12, 0, -12) at x=104 ----
    p.setPen(QColor(140, 140, 160));
    p.setFont(QFont("Arial", 5));
    p.drawText(cx + 104, cy + 12, "+12");
    p.drawText(cx + 109, cy + 40, "0");
    p.drawText(cx + 104, cy + 72, "-12");
}

void EqualizerWindow::mousePressEvent(QMouseEvent *event) {
    int x = event->pos().x();
    int y = event->pos().y();

    if (g_isModernSkin && g_modernSkin) {
        // Modern skin layout
        int tbH = 18;
        // Titlebar
        if (y < tbH) {
            if (x >= width() - 18) { hide(); return; }
            isDragging = true;
            dragPosition = event->globalPosition().toPoint() - frameGeometry().topLeft();
            return;
        }
        // Content offset: center 318px EQ in window
        const int cx = (width() - 318) / 2, cy = 18;
        int rx = x - cx, ry = y - cy; // relative to content

        // ON button: rel (16,47) size 20x9
        if (rx >= 16 && rx < 36 && ry >= 47 && ry < 56) {
            eqEnabled = !eqEnabled; update(); return;
        }
        // AUTO button: rel (16,61) size 33x9
        if (rx >= 16 && rx < 49 && ry >= 61 && ry < 70) {
            autoEnabled = !autoEnabled; update(); return;
        }
        // PRESETS button: rel (16,75) size 49x9
        if (rx >= 16 && rx < 65 && ry >= 75 && ry < 84) {
            showPresetsMenu(mapToGlobal(QPoint(cx + 16, cy + 84))); return;
        }
        // Slider dragging - sliders: h=80, y=1 relative to content
        // Preamp at relX=82, bands at 134+i*18, width=13
        if (ry >= 1 && ry <= 81) {
            if (rx >= 82 && rx < 95) {
                draggingSlider = 0;
                updateSliderFromY(y); return;
            }
            for (int i = 0; i < 10; i++) {
                int sx = 134 + i * 18;
                if (rx >= sx && rx < sx + 13) {
                    draggingSlider = i + 1;
                    updateSliderFromY(y); return;
                }
            }
        }
        return;
    }

    // Classic skin layout
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

void EqualizerWindow::updateSliderFromY(int y) {
    if (g_isModernSkin) {
        // Modern: sliders at cy+1 to cy+81, thumb 23px tall, travel = 80-23 = 57
        const int cy = 18;
        const int sliderTop = cy + 1;
        const int sliderH = 80;
        const int thumbH = 23;
        int travel = sliderH - thumbH;
        int pos = 63 - ((y - sliderTop) * 63) / travel;
        pos = qBound(0, pos, 63);
        if (draggingSlider == 0) preampValue = pos;
        else eqValues[draggingSlider - 1] = pos;
        update();
        return;
    }
    int pos = 63 - ((y - 38) * 63) / 52;
    if (pos < 0) pos = 0;
    if (pos > 63) pos = 63;
    if (draggingSlider == 0) preampValue = pos;
    else eqValues[draggingSlider - 1] = pos;
    update();
}

void EqualizerWindow::mouseMoveEvent(QMouseEvent *event) {
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

void EqualizerWindow::mouseReleaseEvent(QMouseEvent *event) {
    isDragging = false;
    draggingSlider = -1;
}

void EqualizerWindow::mouseDoubleClickEvent(QMouseEvent *event) {
    // Double-click on titlebar toggles shade mode
    int tbH = (g_isModernSkin) ? 18 : 14;
    if (event->pos().y() < tbH) {
        if (!g_isModernSkin) toggleShadeMode();
        return;
    }
    QWidget::mouseDoubleClickEvent(event);
}

void EqualizerWindow::showPresetsMenu(QPoint globalPos) {
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

void EqualizerWindow::saveSettings(QSettings &s) {
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

void EqualizerWindow::loadSettings(QSettings &s) {
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

void EqualizerWindow::loadPresetFromFile() {
    QString fileName = QFileDialog::getOpenFileName(this, "Load EQ Preset",
        QDir::homePath() + "/.config/winamp/eqpresets",
        "EQ Presets (*.eqf);;All Files (*)");
    if (!fileName.isEmpty()) loadPresetFile(fileName);
}

void EqualizerWindow::loadPresetFile(const QString &path) {
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

void EqualizerWindow::savePresetToFile() {
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

void EqualizerWindow::deletePresetFile() {
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

void EqualizerWindow::checkSnap() {
    if (!mainWindow) return;

    const int snapDist = 15;
    QPoint mainPos = mainWindow->pos();
    QSize mainSize = mainWindow->size();
    QPoint myPos = pos();

    // Snap below main, aligned to left edge
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
