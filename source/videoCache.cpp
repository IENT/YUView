#include "videoCache.h"

videoCache::videoCache(playlistItemVideo *video, QObject *parent):QObject(parent)
{
  parentVideo = video;
  // TODO: this is just a value for testing...
  maxCacheSize=5000000;
  Cache.setMaxCost(maxCacheSize);
  stateCacheIsRunning = false;
  stateCancelCaching = false;
  QObject::connect(this,SIGNAL(SignalFrameCached()),parentVideo,SLOT(updateFrameCached()));
}

videoCache::~videoCache()
{
  clearCache();
}


void videoCache::cancelCaching()
{
  // TODO: this is not the best way to cancel the caching
  QMutexLocker locker(&mutex);
  stateCancelCaching=true;
}

void videoCache::clearCache()
{
  // manually empty the cache if requested
  QMutexLocker locker(&mutex);
  Cache.clear();
}

bool videoCache::setMaxCacheSize(int sizeInMB)
{
  QMutexLocker locker(&mutex);
  // TODO: some check if the cache size can be increased or decreased necessary?
  Cache.setMaxCost(sizeInMB<<20);
  return true;
}

// add a pixmap to the cache if it not already in
// return true if it is a new frame, so we can keep track of the actual amount we buffered
bool videoCache::addToCache(CacheIdx cIdx, QPixmap *pixmap)
{
  QMutexLocker locker(&mutex);
  if (!Cache.contains(cIdx))
    {
      Cache.insert(cIdx,pixmap,costPerFrame);
      cacheList.push_back(cIdx);
      return true;
    }
  return false;
}

// returns true if were able to read from the cache and sets the pointer to the pixmap
bool videoCache::readFromCache(CacheIdx cIdx, QPixmap *&pixmap)
{
  pixmap = NULL;
  QMutexLocker locker(&mutex);
  if (Cache.contains(cIdx))
    {
      pixmap = Cache.object(cIdx);
      return true;
    }
  return false;
}

bool videoCache::removeFromCache(CacheIdx cIdx)
{
  QMutexLocker locker(&mutex);
  return Cache.remove(cIdx);
}

void videoCache::addRangeToQueue(indexRange cacheRange)
{
  QMutexLocker locker(&mutex);
  cacheQueue.enqueue(cacheRange);
}

// this is the main worker thread, it quits if all jobs are finished off,
// so we don't block the thread and waste unneccesary processor time
// Other option: never quit the run loop and wait for new jobs
void videoCache::run()
{
  double cacheRate = 0.0;
  if (!stateCacheIsRunning)
    {
      stateCacheIsRunning = true;
      stateCancelCaching = false;
      QMutexLocker locker(&mutex);
      while (cacheQueue.size()>0)
        {
          qDebug() << "Start caching a chunk" << endl;
          locker.relock();
          indexRange cacheRange = cacheQueue.dequeue();
          locker.unlock();
          int framesCached = 0;
          cacheRate = startCaching(cacheRange.first,cacheRange.second,framesCached);
          emit SignalFrameCached();
          qDebug() << "Caching a chunk complete with cacheRate : " << QString::number(cacheRate) << " FPS of " << QString::number(framesCached) << "Frames" << endl;
          // TODO: emit the cacheRate to the controller, but check if frames have been cached
          locker.relock();
          // check after each job, if a cancel has been requested and exit the loob
          if (stateCancelCaching)
            break;
          locker.unlock();
        }
      stateCacheIsRunning = false;
    }
  // TODO: nothing done with this signal yet anywhere
  emit SignalFrameCached();
}

double videoCache::startCaching(int startFrame, int stopFrame, int& framesCached)
{
  int currentFrameToBuffer = startFrame;
  int actualFramesBuffered = 0;

  QTime bufferTimer;
  bufferTimer.start();

  for(currentFrameToBuffer = startFrame; currentFrameToBuffer<=stopFrame;currentFrameToBuffer++)
    {
      // check if cancel has been requested and exit the loop
      mutex.lock();
      if (stateCancelCaching)
      {
          mutex.unlock();
          break;
      }
      mutex.unlock();
      // call the loadIntoCache function from the playlistItem
      if(parentVideo->loadIntoCache(currentFrameToBuffer))
        {
          actualFramesBuffered++;
          if (actualFramesBuffered%10==0)
            {
              emit SignalFrameCached();
            }
        }
    }
  // TODO: maybe return a negative number if no or just a few frames have been cached
  framesCached = actualFramesBuffered;
  double cacheRate =((double)actualFramesBuffered/(double)bufferTimer.elapsed())*1000;
  return cacheRate;
}
