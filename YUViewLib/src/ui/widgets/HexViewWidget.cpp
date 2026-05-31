#include "HexViewWidget.h"

#include <QFontDatabase>
#include <QTextBlock>
#include <QVBoxLayout>

static const int BYTES_PER_LINE = 16;

HexViewWidget::HexViewWidget(QWidget *parent)
    : QWidget(parent)
{
  auto *layout = new QVBoxLayout(this);
  layout->setContentsMargins(0, 0, 0, 0);

  this->view = new QPlainTextEdit(this);
  this->view->setReadOnly(true);
  this->view->setFocusPolicy(Qt::NoFocus);
  this->view->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));
  this->view->setLineWrapMode(QPlainTextEdit::NoWrap);
  this->view->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOn);
  this->view->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
  this->view->setStyleSheet("QPlainTextEdit { background: #1e1e1e; color: #d4d4d4; }");

  layout->addWidget(this->view);
}

void HexViewWidget::setData(const QByteArray &data)
{
  this->currentData     = data;
  this->highlightOffset = -1;
  this->highlightLength = -1;
  this->rebuildDisplay();
}

void HexViewWidget::setHighlight(int byteOffset, int byteLength)
{
  this->highlightOffset = byteOffset;
  this->highlightLength = byteLength;
  this->rebuildDisplay();
}

void HexViewWidget::clearHighlight()
{
  this->highlightOffset = -1;
  this->highlightLength = -1;
  this->rebuildDisplay();
}

void HexViewWidget::clear()
{
  this->currentData.clear();
  this->highlightOffset = -1;
  this->highlightLength = -1;
  if (!this->view)
    return;
  this->view->setExtraSelections(QList<QTextEdit::ExtraSelection>());
  this->view->clear();
}

void HexViewWidget::rebuildDisplay()
{
  if (!this->view)
    return;
  this->view->setExtraSelections(QList<QTextEdit::ExtraSelection>());
  this->view->clear();
  if (this->currentData.isEmpty())
    return;

  const auto *bytes = reinterpret_cast<const unsigned char *>(this->currentData.constData());
  int          size = this->currentData.size();

  QString text;
  text.reserve(size * 4 + size / BYTES_PER_LINE * 10);

  int lineStart = 0;
  while (lineStart < size)
  {
    int lineEnd = qMin(lineStart + BYTES_PER_LINE, size);
    int lineLen = lineEnd - lineStart;

    // Offset
    text += QString("%1  ").arg(lineStart, 8, 16, QLatin1Char('0'));

    // Hex bytes
    QString hexPart;
    QString asciiPart;

    for (int i = 0; i < BYTES_PER_LINE; i++)
    {
      if (i < lineLen)
      {
        int byteIdx = lineStart + i;
        hexPart += QString("%1").arg(bytes[byteIdx], 2, 16, QLatin1Char('0')).toUpper();
        unsigned char c = bytes[byteIdx];
        asciiPart += (c >= 32 && c <= 126) ? QChar(c) : QChar('.');
      }
      else
      {
        hexPart += "  ";
        asciiPart += ' ';
      }
      if (i == BYTES_PER_LINE / 2 - 1)
        hexPart += ' ';
      hexPart += ' ';
    }

    text += hexPart;
    text += " |";
    text += asciiPart;
    text += "|\n";

    lineStart += BYTES_PER_LINE;
  }

  this->view->setPlainText(text);

  if (this->highlightOffset >= 0 && this->highlightLength > 0)
  {
    int hiStart = this->highlightOffset;
    int hiEnd   = qMin(hiStart + this->highlightLength, size);

    QList<QTextEdit::ExtraSelection> selections;

    for (int i = hiStart; i < hiEnd; i++)
    {
      int lineIdx   = i / BYTES_PER_LINE;
      int colInLine = i % BYTES_PER_LINE;

      int linePos  = lineIdx;
      int colStart = colInLine;

      // Calculate position in plain text
      QTextBlock block = this->view->document()->findBlockByNumber(linePos);
      if (!block.isValid())
        continue;

      // Hex column position
      // Format: "XXXXXXXX  HH HH HH HH HH HH HH HH  HH HH HH HH HH HH HH HH  |AAAA...|\n"
      // where X = offset (8), then 2 spaces, then 16*3 hex chars + 1 separator space
      int hexCol = 10 + colStart * 3;
      if (colStart >= 8)
        hexCol += 1; // extra space between groups

      QTextCursor cursor(block);
      cursor.setPosition(block.position() + hexCol);

      auto sel = QTextEdit::ExtraSelection();
      sel.format.setBackground(QColor(0, 120, 215));
      sel.format.setForeground(QColor(255, 255, 255));
      sel.cursor = cursor;
      sel.cursor.movePosition(QTextCursor::Right, QTextCursor::KeepAnchor, 2);
      selections.append(sel);

      // Also highlight ASCII (starts at pos 61: 10 prefix + 49 hex + 2 " |")
      int asciiCol = 61 + colStart;

      QTextCursor asciiCursor(block);
      asciiCursor.setPosition(block.position() + asciiCol);

      auto asciiSel          = QTextEdit::ExtraSelection();
      asciiSel.format        = sel.format;
      asciiSel.cursor        = asciiCursor;
      asciiSel.cursor.movePosition(QTextCursor::Right, QTextCursor::KeepAnchor, 1);
      selections.append(asciiSel);
    }

    this->view->setExtraSelections(selections);
  }
}
