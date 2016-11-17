/*  YUView - YUV player with advanced analytics toolset
*   Copyright (C) 2015  Institut für Nachrichtentechnik
*                       RWTH Aachen University, GERMANY
*
*   YUView is free software; you can redistribute it and/or modify
*   it under the terms of the GNU General Public License as published by
*   the Free Software Foundation; either version 2 of the License, or
*   (at your option) any later version.
*
*   YUView is distributed in the hope that it will be useful,
*   but WITHOUT ANY WARRANTY; without even the implied warranty of
*   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
*   GNU General Public License for more details.
*
*   You should have received a copy of the GNU General Public License
*   along with YUView.  If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef LABELELIDED_H
#define LABELELIDED_H

#include <QLabel>

// A custom minimum size QLabel that elides text if necessary (add ...  in the middle of the text).
// The label initializes with a minimum width of 20pixels. However, it can get as big as the text
// is wide. The QLabelElided will also show the full text as tooltip if it was elided.
// See http://stackoverflow.com/q/21284720/1329652 for alternate implementation using styles.
class labelElided : public QLabel
{
  Q_OBJECT
  Q_PROPERTY(QString text READ text WRITE setText)

public:
  // The constructor will set the label to a very small minimum size. If you want the label
  // to be bigger by default, you have to set the minimum size manually.
  explicit labelElided(QWidget *parent = 0) : QLabel(parent) { setMinimumSize(20, 1); }
  explicit labelElided(const QString &newText, QWidget *parent = 0) : QLabel(parent) { setMinimumSize(20,1); setText(newText); }
  QString text() const { return m_text; }
  void setText(const QString &newText)
  {
    if (m_text == newText) return;
    m_text = newText;
    setElidedText();
  }

protected:
  void resizeEvent(QResizeEvent *event) Q_DECL_OVERRIDE { QLabel::resizeEvent(event); setElidedText(); }

private:
  void setElidedText()
  {
    // Set elided text and tooltip (if the text was elided)
    QFontMetrics metrics(font());
    QString textElided = metrics.elidedText(m_text, Qt::ElideMiddle, size().width());
    if (textElided != m_text)
      setToolTip(m_text);
    QLabel::setText(textElided);
  }
  using QLabel::setPicture; // This widget only supports displaying single-line text
  using QLabel::setPixmap;
  using QLabel::setWordWrap;
  QString m_text;
};

#endif // LABELELIDED_H
