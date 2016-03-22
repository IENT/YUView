#ifndef VIDEOCACHE_H
#define VIDEOCACHE_H

#include <QtCore>
#include <QPixmap>
#include <QCache>
#include <QMutex>
#include <QMutexLocker>
#include <QQueue>
#include "typedef.h"
#include "videoHandler.h"

class videoHandler;

class videoCache : public QObject
{
  Q_OBJECT

public:
  videoCache(videoHandler* video, QObject *parent = 0);
  virtual ~videoCache();

  // any type of playlistItemVideo can access its cache via these functions,
  // they are thread-safe by using a mutex locker

  bool addToCache(CacheIdx cIdx, QPixmap *pixmap);
  bool readFromCache(CacheIdx cIdx, QPixmap *&pixmap);
  bool removeFromCache(CacheIdx cIdx);

  // add cache jobs to the queue
  void addRangeToQueue(indexRange cacheRange);

  // the cost per frame depends on the size of the pixmap,
  // the owning playlistItemVideo can set it
  void setCostPerFrame(int cost) {costPerFrame = cost;}

  // TODO: the maximum cache size must be controlled by the playbackController in some way
  bool setMaxCacheSize(int sizeInMB);

  // some functions for testing the state of the cache
  bool isCacheRunning() {return stateCacheIsRunning;}
  bool isCacheCancelled() { return stateCancelCaching;}
  int getCacheSize() { return Cache.size(); }

  // these functions could also be slots
  void cancelCaching();
  void clearCache ();

public slots:
  // the main worker thread, which takes caching jobs from the cacheQueue
  void run();

signals:
    // TODO: none of these are used yet
    void CachingFinished();
    void CacheFull();
    void SignalFrameCached();
    void error(QString err);

protected:
    // called internally and returns the cacheRate in FPS and the number of frames that were
    // successfully cached. TODO: send this info back to the playbackController
    double startCaching(int startFrame, int stopFrame, int& framesCached);

private:
  bool stateCacheIsRunning;
  bool stateCancelCaching;

  int maxCacheSize;
  int costPerFrame;

  // main lock
  QMutex mutex;
  QCache<CacheIdx,QPixmap> Cache;

  // this list might be helpful/needed for debugging,
  // to check the contents of our cache.
  // TODO: we could also use it, to check the buffer contents, which might be faster than acutally testing the buffer?!
  QList<CacheIdx> cacheList;

  // FIFO queue that holds the jobs as index range pairs
  QQueue<indexRange> cacheQueue;

  // we have a pointer to the owning object, so we can access its loadIntoCache function
  // which might have different implementations, depending on the type of the video
  videoHandler *parentVideo;
};

#endif // VIDEOCACHE_H
