/*  This file is part of YUView - The YUV player with advanced analytics toolset
*   <https://github.com/IENT/YUView>
*   Copyright (C) 2018  Institut für Nachrichtentechnik, RWTH Aachen University, GERMANY
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

#ifndef CHARTWORKER_H
#define CHARTWORKER_H

#include <QObject>
#include "chartHandler.h"
#include "yuvcharts.h"

// This debug setting has two values:
// 1: Basic operation is written to qDebug: If a new item is selected, what is the decision to cache/remove next?
//    When is caching of a frame started?
// 2: Show all details. What are the threads doing when? What is removed when? ...
#define CACHING_DEBUG_OUTPUT 0
#if CACHING_DEBUG_OUTPUT && !NDEBUG
#include <QDebug>
#define DEBUG_CACHING qDebug
#if CACHING_DEBUG_OUTPUT == 2
#define DEBUG_CACHING_DETAIL qDebug
#else
#define DEBUG_CACHING_DETAIL(fmt,...) ((void)0)
#endif
#else
#define DEBUG_CACHING(fmt,...) ((void)0)
#define DEBUG_CACHING_DETAIL(fmt,...) ((void)0)
#endif

#define CACHING_THREAD_JOBS_OUTPUT 0
#if CACHING_THREAD_JOBS_OUTPUT && !NDEBUG
#include <QDebug>
#define DEBUG_JOBS qDebug
#else
#define DEBUG_JOBS(fmt,...) ((void)0)
#endif

/**
 * @brief The ChartWorker class
 * class that support calculating-jobs from the charthandler.
 * The class can be used in threads
 */
class ChartWorker : public QObject
{
  Q_OBJECT

public:
  /**
   * @brief ChartWorker
   * default-constructor
   *
   * @param aParent
   * parent for destructor
   */
  ChartWorker(QObject* aParent);

  /**
   * @brief setWorking
   * changes the state of working
   *
   * @param aState
   * state true: worker is working
   * state false: working is waiting
   */
  void setWorking(bool aState) { this->mWorking = aState; }

  /**
   * @brief isWorking
   * returns the state of working
   *
   * @return
   * state true: worker is working
   * state false: working is waiting
   */
  bool isWorking() { return this->mWorking; }

  /**
   * @brief coord
   * returns the itemWidgetCoord, which the data to calculate includes
   *
   * @return
   * itemWidgetCoord data
   */
  itemWidgetCoord coord() const;

  /**
   * @brief setCoord
   * new itemWidgetCoord o calculate
   *
   * @param aCoord
   * itemWidgetCoord which is to set
   */
  void setCoord(const itemWidgetCoord& aCoord);

  /**
   * @brief processLoadingJob
   * Process the job in the thread that this worker was moved to. This function can be directly
   * called from the main thread. It will still process the call in the separate thread.
   *
   * @param aCoord
   * data to process
   */
  void processLoadingJob(itemWidgetCoord aCoord);

  /**
   * @brief chartHandler
   * getting the actual activ charthandler
   *
   * @return
   * active charthandler
   */
  ChartHandler* chartHandler() const;

  /**
   * @brief setChartHandler
   * setter for ChartHandler
   *
   * @param aHandler
   * new active chartHandler
   */
  void setChartHandler(ChartHandler* aHandler);

  /**
   * @brief result
   * returns the calculated result. The result is ready, after the signal ChartWorker::loadingFinished was send
   *
   * @return
   * calculated chartSettingsData as result
   */
  chartSettingsData result() {return this->mCachedData;}

  /**
   * @brief id
   * ID of the workerobject
   * each workerobject has an individual ID
   *
   * @return
   * actual ID
   */
  int id() {return this->mId;}

  /**
   * @brief noEmit
   * marker that no emit of the signal ChartWorker::loadingFinished should do
   */
  void noEmit() {this->mDoEmit = false;}

signals:
  /**
   * @brief loadingFinished
   * after calculating the loadingFinished signal will emit
   *
   * @param aId
   * Id of the workerobject
   */
  void loadingFinished(int aId);

private slots:
  /**
   * @brief processLoadingJobInternal
   * Process the job in the thread that this worker was moved to.
   *
   * @param aCoord
   * data to process
   */
  void processLoadingJobInternal(itemWidgetCoord aCoord);
private:
  // id of the worker
  int mId;
  // marker, that the object is working
  bool mWorking;
  // switch to emit the signal
  bool mDoEmit = true;
  // handler to calculate the result
  ChartHandler* mChartHandler = nullptr;
  // holder for result
  chartSettingsData mCachedData;
  // holder for data
  itemWidgetCoord mCoord;

  // static init for id
  static int chart_id_counter;
};

/**
 * @brief The ChartWorkerThread class
 * Thread class to handle an ChartWorker-Object
 * An object of this class can started and if you want to start a job, just push the data to the workerand start the operation with the worker
 */
class ChartWorkerThread : public QThread
{
  Q_OBJECT

public:

  /**
   * @brief ChartWorkerThread
   * default constructor
   *
   * @param aParent
   * a parent
   *
   * @param aHandler
   * a handler to handle the calculating operations
   */
  ChartWorkerThread(QObject* aParent, ChartHandler* aHandler);

  /**
   * @brief quitWhenDone
   * thread will be quit, after the calculating job is done
   */
  void quitWhenDone();

  /**
   * @brief setChartHandler
   * setter for new ChartHandler
   *
   * @param aHandler
   * new active ChartHandler
   */
  void setChartHandler(ChartHandler* aHandler) {this->mChartHandler.reset(aHandler);}

  /**
   * @brief worker
   * getter for the active worker
   *
   * @return
   * active worker
   */
  ChartWorker* worker() { return mThreadWorker.data(); }

  /**
   * @brief chartHandler
   * getter for active handler
   *
   * @return
   * active chartHandler
   */
  ChartHandler* chartHandler() {return this->mChartHandler.data(); }

  /**
   * @brief reset
   */
  void reset();

  /**
   * @brief isQuitting
   * returns the status, thrad is quitting or not
   *
   * @return
   * bool: true--> thread is quitting
   * false --> thread runs
   */
  bool isQuitting() { return mQuitting; }

private:
  // pointer on threadworker
  QScopedPointer<ChartWorker> mThreadWorker;
  // pointer on handler
  QScopedPointer<ChartHandler> mChartHandler;
  // marker for quitting the thread
  bool mQuitting;  // Are er quitting the job? If yes, do not push new jobs to it.

};

#endif // CHARTWORKER_H
