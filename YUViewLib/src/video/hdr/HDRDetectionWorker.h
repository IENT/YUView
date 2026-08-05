#ifndef HDRDETECTIONWORKER_H
#define HDRDETECTIONWORKER_H

#include <QObject>
#include <QThread>
#include <QMutex>
#include "HDRDetection.h"

class HDRDetectionWorker : public QObject
{
    Q_OBJECT

public:
    explicit HDRDetectionWorker(QObject* parent = nullptr);
    ~HDRDetectionWorker();

    // Start HDR detection in background thread
    void startDetection();
    
    // Check if detection is already running
    bool isDetecting() const;

    // Stop the worker thread and move this object back to the caller thread so
    // it can be deleted safely from the GUI thread.
    void terminateWorker();

public slots:
    void performDetection();

signals:
    // Emitted when HDR detection is complete
    void detectionComplete(const HDRDetection::HDRCapabilities& capabilities);
    
    // Emitted when detection fails
    void detectionFailed(const QString& error);

private:
    QThread* m_workerThread;
    mutable QMutex m_mutex;
    bool m_isDetecting;
    HDRDetection* m_hdrDetection;
};

#endif // HDRDETECTIONWORKER_H