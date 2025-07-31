#include "HDRDetectionWorker.h"
#include <QDebug>
#include <QMutexLocker>
#include <QTimer>

HDRDetectionWorker::HDRDetectionWorker(QObject* parent)
    : QObject(parent)
    , m_workerThread(nullptr)
    , m_isDetecting(false)
    , m_hdrDetection(nullptr)
{
}

HDRDetectionWorker::~HDRDetectionWorker()
{
    if (m_workerThread) {
        m_workerThread->quit();
        m_workerThread->wait(3000); // Wait up to 3 seconds
        if (m_workerThread->isRunning()) {
            qWarning() << "HDR detection thread did not terminate gracefully, forcing termination";
            m_workerThread->terminate();
            m_workerThread->wait(1000);
        }
        delete m_workerThread;
    }
}

void HDRDetectionWorker::startDetection()
{
    QMutexLocker locker(&m_mutex);
    
    if (m_isDetecting) {
        qDebug() << "HDR detection already in progress, ignoring request";
        return;
    }
    
    qDebug() << "Starting simplified HDR detection (non-blocking)...";
    m_isDetecting = true;
    
    // Use QTimer to perform detection in next event loop iteration
    // This prevents UI blocking while we fix the threading implementation
    QTimer::singleShot(50, this, &HDRDetectionWorker::performDetection);
}

bool HDRDetectionWorker::isDetecting() const
{
    QMutexLocker locker(&m_mutex);
    return m_isDetecting;
}

void HDRDetectionWorker::performDetection()
{
    qDebug() << "HDR detection worker started (simplified, non-blocking)";
    
    try {
        // Initialize HDR detection (this may take time)
        if (!m_hdrDetection) {
            m_hdrDetection = HDRDetection::instance();
        }
        
        // Perform HDR capability detection (now non-blocking due to DXGI disable)
        auto capabilities = m_hdrDetection->detectHDRCapabilities(nullptr);
        
        qDebug() << "HDR detection completed. Supported:" << capabilities.isHDRSupported;
        
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