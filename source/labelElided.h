/*  This file is part of YUView - The YUV player with advanced analytics toolset
*   <https://github.com/IENT/YUView>
*   Copyright (C) 2015  Institut für Nachrichtentechnik, RWTH Aachen University, GERMANY
*
*   This program is free software; you can redistribute it and/or modify
*   it under the terms of the GNU General Public License as published by
*   the Free Software Foundation; either version 3 of the License, or
*   (at your option) any later version.
*
*   In addition, as a special exception, the copyright holders give
*   permission to link the code of portions of this program with the
*   OpenSSL library under certain conditions as described in each
*   individual source file, and distribute linked combinations including
*   the two.
*   
*   You must obey the GNU General Public License in all respects for all
*   of the code used other than OpenSSL. If you modify file(s) with this
*   exception, you may extend this exception to your version of the
*   file(s), but you are not obligated to do so. If you do not wish to do
*   so, delete this exception statement from your version. If you delete
*   this exception statement from all source files in the program, then
*   also delete it here.
*
*   This program is distributed in the hope that it will be useful,
*   but WITHOUT ANY WARRANTY; without even the implied warranty of
*   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
*   GNU General Public License for more details.
*
*   You should have received a copy of the GNU General Public License
*   along with this program. If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef LABELELIDED_H
#define LABELELIDED_H

#include <QLabel>

// A custom minimum size QLabel that elides text if necessary (add ...  in the middle of the text).
// The label initializes with a minimum width of 20pixels. However, it can get as big as the text
// is wide. The QLabelElided will also show the full text as tool-tip if it was elided.
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
  void setToolTip(const QString & newToolTip)
  {
    if (m_toolTip == newToolTip) return;
    m_toolTip = newToolTip;
    setElidedText();
  }

protected:
  void resizeEvent(QResizeEvent *event) Q_DECL_OVERRIDE { QLabel::resizeEvent(event); setElidedText(); }

private:
  void setElidedText()
  {
    // Set elided text and tool-tip (if the text was elided)
    QFontMetrics metrics(font());
    QString textElided = metrics.elidedText(m_text, Qt::ElideMiddle, size().width());
    if (textElided != m_text)
      QLabel::setToolTip(QStringLiteral("%1%2%3").arg(m_text).arg(m_toolTip.isEmpty() ? QString() : "\n").arg(m_toolTip));
    else
      QLabel::setToolTip(m_toolTip);
    QLabel::setText(textElided);
  }
  using QLabel::setPicture; // This widget only supports displaying single-line text
  using QLabel::setPixmap;
  using QLabel::setWordWrap;
  QString m_text;
  QString m_toolTip;
};

#endif // LABELELIDED_H
