# Bug Fixes - Code Review Phase 1

1. **FileInfoDialog Memory Leak**:
   - **File**: [main.cpp](file:///home/ender/winamp-linux/src/main.cpp) (around line 4109)
   - **Symptom**: Repeatedly clicking "File Info" leaked dialog instances.
   - **Fix**: Added `dlg->setAttribute(Qt::WA_DeleteOnClose);`.

2. **QSplashScreen Memory Leak**:
   - **File**: [main.cpp](file:///home/ender/winamp-linux/src/main.cpp) (around line 5131)
   - **Symptom**: Splash screen was closed but never deleted, leaking on every launch.
   - **Fix**: Replaced direct slot call with a lambda doing `splash->close(); splash->deleteLater();` via QTimer single shot.

3. **mediaLibraryWindow Memory Leak**:
   - **File**: [main.cpp](file:///home/ender/winamp-linux/src/main.cpp)
   - **Symptom**: Media library window was constructed on heap but omitted from `~WinampWindow()` destructor.
   - **Fix**: Added `delete mediaLibraryWindow;`.

4. **Circular XML Includes Stack Overflow**:
   - **File**: [modern_skin.h](file:///home/ender/winamp-linux/src/modern_skin.h)
   - **Symptom**: Stack overflow crash on circular or deep skin XML includes.
   - **Fix**: Enforced a recursion depth limit (10) and canonical path cycle tracking set (`QSet`).
