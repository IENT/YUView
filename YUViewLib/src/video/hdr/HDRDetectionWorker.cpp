#include "HDRDetectionWorker.h"
#include <QDebug>
#include <QMetaObject>
#include <QMutexLocker>
#include <QThread>

HDRDetectionWorker::HDRDetectionWorker(QObject* parent)
    : QObject(nullptr)  // intentionally no QObject parent: see below
    , m_workerThread(new QThread)
    , m_isDetecting(false)
    , m_hdrDetection(nullptr)
{
        // Move this QObject onto a dedicated worker thread so that DXGI
    // enumeration in performDetection() runs off the GUI thread. Previously
    // "startDetection()" was a QTimer::singleShot on the caller's thread,
    // which blocked the UI during adapter/output enumeration on slow
    // systems. All slot invocations on "this" now execute on m_workerThread
    // via queued connections.
    //
    // Note on ownership: QObject::moveToThread is only legal when the object
    // has no parent, so we deliberately pass nullptr to the QObject base
    // class here. The caller passes a logical parent to express ownership
    // intent (and for symmetry with the old API), but the actual lifetime
    // is managed explicitly by HDRRenderingManager::cleanupHDRResources,
    // which calls terminateWorker() before deleting us from the GUI thread.
    Q_UNUSED(parent);
    m_workerThread->setObjectName(QStringLiteral("HDRDetectionWorker"));
    moveToThread(m_workerThread);
    m_workerThread->start();
}

HDRDetectionWorker::~HDRDetectionWorker()
{
    if (m_workerThread) {
        if (m_workerThread->isRunning()) {
            qWarning() << "[HDRDetectionWorker] Worker thread still running in destructor";
            m_workerThread->quit();
            m_workerThread->wait(3000);
        }
        delete m_workerThread;
        m_workerThread = nullptr;
    }
}

void HDRDetectionWorker::terminateWorker()
{
    if (!m_workerThread) {
        return;
    }

    // Must be called from a thread other than m_workerThread.
    if (QThread::currentThread() == m_workerThread) {
        qWarning() << "[HDRDetectionWorker] terminateWorker() called from worker thread";
        return;
    }

    disconnect(this, nullptr, nullptr, nullptr);

    m_workerThread->quit();
    if (!m_workerThread->wait(3000)) {
        qWarning() << "[HDRDetectionWorker] Worker thread did not stop gracefully, forcing termination";
        m_workerThread->terminate();
        m_workerThread->wait(1000);
    }

    // Move back to the caller thread before deleting the QThread object.
    moveToThread(QThread::currentThread());

    delete m_workerThread;
    m_workerThread = nullptr;
}

void HDRDetectionWorker::startDetection()
{
    {
        QMutexLocker locker(&m_mutex);
        if (m_isDetecting) {
            return;
        }
        m_isDetecting = true;
    }

    // Post performDetection() onto the worker thread's event loop. With
    // Qt::QueuedConnection the call returns immediately on the caller's
    // thread; performDetection runs on m_workerThread.
    QMetaObject::invokeMethod(this, "performDetection", Qt::QueuedConnection);
}

bool HDRDetectionWorker::isDetecting() const
{
    QMutexLocker locker(&m_mutex);
    return m_isDetecting;
}

void HDRDetectionWorker::performDetection()
{
    
    try {
        // Initialize HDR detection (this may take time)
        if (!m_hdrDetection) {
            m_hdrDetection = HDRDetection::instance();
        }
        
        // Perform HDR capability detection (now non-blocking due to DXGI disable)
        auto capabilities = m_hdrDetection->detectHDRCapabilities(nullptr);
        
        
        // Emit result back to caller
        emit detectionComplete(capabilities);
        
    } catch (const std::exception& e) {
        QString error = QString("HDR detection failed with exception: %1").arg(e.what());
        qWarning() << error;
        emit detectionFailed(error);
        
    } catch (...) {
        QString error = "HDR detection failed with unknown exception";
        qWarning() << error;
        emit detectionFailed(error);
    }
    
    // Mark detection as complete
    {
        QMutexLocker locker(&m_mutex);
        m_isDetecting = false;
    }
}
