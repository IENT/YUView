/*  This file is part of YUView - The YUV player with advanced analytics toolset
*   <https://github.com/IENT/YUView>
*   Copyright (C) 2017  Institut für Nachrichtentechnik, RWTH Aachen University, GERMANY
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

#ifndef CHARTWIDGET_H
#define CHARTWIDGET_H

#include <QPointer>
#include <QStackedWidget>
#include <QVBoxLayout>
#include <QWidget>
#include "ui_chartWidget.h"

/**
 * @brief The ChartWidget class
 * the ChartWidget class can display the charts. It uses a QStackedWidget
 */
class ChartWidget : public QWidget
{
    Q_OBJECT

public:
  /**
   * @brief ChartWidget
   * default-construtcor
   *
   * @param parent
   * which QWidget will be the parent widget
   *
   */
  explicit ChartWidget(QWidget *parent = 0);

  /**
    *@brief
    *default-destructor
    */
  ~ChartWidget();

  /**
   * @brief setChartWidget
   * places a Widget and add it to an stack
   *
   * @param aWidget
   * widget which is to place
   */
  void setChartWidget(QWidget* aWidget);

  //
  /**
   * @brief removeChartWidget
   * removes an widget from the stack
   *
   * @param aWidget
   * widget which will be removed
   */
  void removeChartWidget(QWidget* aWidget);

  /**
   * @brief getDefaultWidget
   * returns an empty widget as default
   *
   * @return
   * default-widget
   */
  QWidget* getDefaultWidget() {return &mEmptyWidget;}

private:
    // knowing the last shown widget
    QWidget *mWidget;

    // topLayout as basic
    QVBoxLayout mTopLayout;
    // changeing the different Widgets
    QStackedWidget mStack;
    // basic default widget
    QWidget mEmptyWidget;
};

#endif // CHARTWIDGET_H
