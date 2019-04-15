#include "chartWorker.h"

ChartWorker::ChartWorker(QObject* aParent)
  : QObject(aParent)
{
  // we init first and set some defaults
  // we are not working
  this->mWorking = false;
  // getting an id
  this->mId = ChartWorker::chart_id_counter++;
}

ChartWorkerThread::ChartWorkerThread(QObject* aParent, ChartHandler* aHandler)
  : QThread(aParent)
{
  // init the thread
  // set the handler
  this->setChartHandler(aHandler);
  // set the worker
  // create an worker and move it to another thread
  this->mThreadWorker.reset(new ChartWorker(nullptr));
  this->mThreadWorker->moveToThread(this);
  // we are not quitting the thread
  this->mQuitting = false;
  // set handler to the worker
  this->worker()->setChartHandler(this->chartHandler());
}

void ChartWorkerThread::quitWhenDone()
{
  // we are qutting the thread, so set the marker that no new job can pushed
  this->mQuitting = true;
  if (this->mThreadWorker->isWorking())
  {
    // We must wait until the worker is done.
    DEBUG_CACHING("ChartWorkerThread::quitWhenDone waiting for worker to finish...");
    connect(worker(), &ChartWorker::loadingFinished, this, [=]{DEBUG_CACHING("ChartWorkerThread::quitWhenDone worker done -> quit"); quit();});
  }
  else
  {
    DEBUG_CACHING("ChartWorkerThread::quitWhenDone quit now");
    quit();
  }
}

void ChartWorkerThread::reset()
{
}

itemWidgetCoord ChartWorker::coord() const
{
  return mCoord;
}

void ChartWorker::setCoord(const itemWidgetCoord& aCoord)
{
  this->mCoord = aCoord;
}

void ChartWorker::processLoadingJob(itemWidgetCoord aCoord)
{
  DEBUG_JOBS("ChartWorker::processLoadingJob invoke processLoadingJobInternal");
  this->setCoord(aCoord);
  QMetaObject::invokeMethod(this, "processLoadingJobInternal", Q_ARG(itemWidgetCoord, aCoord));
}

void ChartWorker::processLoadingJobInternal(itemWidgetCoord aCoord)
{
  DEBUG_JOBS("ChartWorker::processLoadingJobInternal");
  this->setWorking(true);

  // at this point we can calculate the data
  // This is performed in the thread (the loading thread with higher priority.

  //at this point, we have to decide which function we call
  if(dynamic_cast<playlistItemStatisticsFile*> (aCoord.mItem))
    this->mCachedData = this->mChartHandler->createStatisticsChartSettings(aCoord);
  else if(dynamic_cast<playlistItemRawFile*> (aCoord.mItem))
    this->mCachedData = this->mChartHandler->createStatisticsChartSettings(aCoord);
  else
    this->mCachedData.mSettingsIsValid = false;

  // check that we can should do an emit
  if(this->mDoEmit)
    emit loadingFinished(this->mId);

  // reset the thread to the default values
  this->mDoEmit = true;
  this->setWorking(false);
  DEBUG_JOBS("ChartWorker::processLoadingJobInternal emit loadingFinished");
}

ChartHandler* ChartWorker::chartHandler() const
{
  return this->mChartHandler;
}

void ChartWorker::setChartHandler(ChartHandler* aHandler)
{
  this->mChartHandler = aHandler;
}
