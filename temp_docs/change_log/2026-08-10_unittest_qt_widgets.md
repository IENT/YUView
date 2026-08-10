# Change Log

## 2026-08-10 — Unit tests need Qt Widgets for SaveUi.h

`FrameHandler.h` → `SaveUi.h` includes `<QLabel>` / widgets types.
`YUViewUnitTest.pro` only had `QT += core xml`, so CI failed with
`QLabel: No such file or directory`. Align with upstream: add `widgets`
(and `$$top_builddir/YUViewLib` to INCLUDEPATH).
