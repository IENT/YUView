# Change Log

## 2026-08-10 — Push unit-test widgets + silence FormatGuessing warning

- Ensure `YUViewUnitTest` has `QT += widgets` (was committed but not yet on remote).
- Drop unused `#include <QLabel>` from `SaveUi.h` (still needs widgets for QWidget/QLayout).
- Change `FormatGuessingParameters.h` helpers from `static` to `inline` so TUs that
  include the header without calling them do not emit `-Wunused-function`.
