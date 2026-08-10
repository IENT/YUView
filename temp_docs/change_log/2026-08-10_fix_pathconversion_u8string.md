# Change Log

## 2026-08-10 — Fix C++20 PathConversion CI break on Linux/macOS

### Cause
`std::filesystem::path::u8string()` returns `std::u8string` (`char8_t`) under C++20.
`QString::fromStdString` only accepts `std::string`, so GCC/Clang CI failed in
`functions::fsPathToQString` (Linux + macOS).

### Fix
Decode UTF-8 via `QString::fromUtf8` from the `u8string` byte view (Windows path
still uses `wstring`).
