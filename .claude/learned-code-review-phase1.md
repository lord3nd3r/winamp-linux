# Learned - Code Review Phase 1

1. **AUTOMOC and Unused DBus Headers**:
   When using Qt, files with `Q_OBJECT` macros are processed by MOC. If they inherit from classes that are conditionally defined (like `QDBusAbstractAdaptor` only when `QT_DBUS_LIB` is active), the generated `moc_*.cpp` code will still reference the class unless the entire class definition is also wrapped inside the same conditional directives in the header. Wrap entire classes in `#if defined(QT_DBUS_LIB)` to prevent compilation failures when building without DBus.

2. **Python Interpreter Initialization Errors**:
   Initializing `py::scoped_interpreter guard{}` can throw `std::runtime_error` during static/member initialization before the constructor body runs. It's safer to instantiate it inside a try/catch using `std::unique_ptr` and `std::make_unique` to gracefully catch startup exceptions and fall back to disabling plugins.

3. **Compiler Warnings in Headers**:
   `static` declarations in headers (like functions or const chars) create a separate copy in each translation unit. This results in compiler warnings if a source file includes the header but doesn't reference the variable/function. Using `inline` (for functions) or `inline constexpr` (for variables) avoids these warnings cleanly.
