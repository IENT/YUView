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
    int hiStart  = this->highlightOffset;
    int hiEnd    = qMin(hiStart + this->highlightLength, size);
    int lastByte = hiEnd - 1;

    int startLine = hiStart / BYTES_PER_LINE;
    int startCol  = hiStart % BYTES_PER_LINE;
    int endLine   = lastByte / BYTES_PER_LINE;
    int endCol    = lastByte % BYTES_PER_LINE;

    auto hexCol = [](int col) {
      int c = 10 + col * 3;
      if (col >= 8)
        c += 1;
      return c;
    };

    // Format: "XXXXXXXX  HH HH ... HH  HH ... HH  |AAAA...|\n"
    // offset(8) + "  "(2) + hexPart(49) + " |"(2) = 61
    const int ASCII_START = 61;

    QList<QTextEdit::ExtraSelection> selections;

    auto format          = QTextCharFormat();
    format.setBackground(QColor(0, 120, 215));
    format.setForeground(QColor(255, 255, 255));

    for (int line = startLine; line <= endLine; line++)
    {
      QTextBlock block = this->view->document()->findBlockByNumber(line);
      if (!block.isValid())
        continue;

      int colStart = (line == startLine) ? startCol : 0;
      int colEnd   = (line == endLine)   ? endCol   : (BYTES_PER_LINE - 1);

      auto hexSel          = QTextEdit::ExtraSelection();
      hexSel.format        = format;
      hexSel.cursor        = QTextCursor(this->view->document());
      hexSel.cursor.setPosition(block.position() + hexCol(colStart));
      hexSel.cursor.setPosition(block.position() + hexCol(colEnd) + 2,
                                QTextCursor::KeepAnchor);
      selections.append(hexSel);

      auto asciiSel          = QTextEdit::ExtraSelection();
      asciiSel.format        = format;
      asciiSel.cursor        = QTextCursor(this->view->document());
      asciiSel.cursor.setPosition(block.position() + ASCII_START + colStart);
      asciiSel.cursor.setPosition(block.position() + ASCII_START + colEnd + 1,
                                  QTextCursor::KeepAnchor);
      selections.append(asciiSel);
    }

    this->view->setExtraSelections(selections);
  }
}
