#pragma once

#include <QByteArray>
#include <QPlainTextEdit>
#include <QWidget>

class HexViewWidget : public QWidget
{
  Q_OBJECT

public:
  HexViewWidget(QWidget *parent = nullptr);

  void setData(const QByteArray &data);
  void setHighlight(int byteOffset, int byteLength);
  void clearHighlight();
  void clear();

private:
  // Rebuild the full text content (call only when currentData changes).
  void rebuildText();
  // Re-apply ExtraSelections for the current highlight (cheap, no text rebuild).
  void applyHighlight();

  QPlainTextEdit *view;
  QByteArray      currentData;

  int highlightOffset{-1};
  int highlightLength{-1};
};
