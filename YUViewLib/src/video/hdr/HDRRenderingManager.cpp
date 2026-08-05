#include "HDRRenderingManager.h"
#include "qmainwindow.h"
#include <QApplication>
#include <QDebug>
#include <QTimer>
#include <QMutexLocker>
#include <QWidget>
#include <QProcess>
#include <QSettings>
#include <limits>


static HDRRenderingManager* s_instance = nullptr;

HDRRenderingManager* HDRRenderingManager::instance()
{

  static QMutex g_singletonMutex;
  QMutexLocker locker(&g_singletonMutex);
  if (!s_instance) {
    s_instance = new HDRRenderingManager(qApp);
  }
  return s_instance;
}

HDRRenderingManager::HDRRenderingManager(QObject* parent)
  : QObject(parent)
  , m_hdrWindow(nullptr)
  , m_hdrContainer(nullptr)
  , m_hdrDetectionWorker(nullptr)
  , m_useHDRRendering(false)
  , m_isHDRWidgetReady(false)
{
  m_hdrCapabilities.isHDRSupported = false;
  
}

HDRRenderingManager::~HDRRenderingManager()
{
  cleanupHDRResources();
}

void HDRRenderingManager::setHDRRenderingEnabled(bool enabled)
{
  if (enabled) {
    {
      QMutexLocker locker(&m_stateMutex);
      m_useHDRRendering = true;
    }
    
    // Start asynchronous HDR detection (outside lock to avoid deadlock)
    startHDRDetection();
    
  } else {
    {
      QMutexLocker locker(&m_stateMutex);
      m_useHDRRendering = false;
      m_isHDRWidgetReady = false;
    }
    
    // Clean up HDR widget if exists (cleanupHDRResources() has its own locking)
    cleanupHDRResources();
  }
}


void HDRRenderingManager::applyHDRWindowSettings(HDR_WindowType* window)
{
  if (!window)
    return;

  // Single source of truth for HDR render mode + tone-map target.
  QSettings settings;
  const int hdrMode = settings.value("HDRMode", 0).toInt();
  if (hdrMode == 2) {
    // Linear mode - pass-through to display with scRGB
    window->setRenderMode(HDR_WindowType::Mode_BT2020_Linear_16bit);
  } else if (hdrMode == 1) {
    // HLG mode - converted to PQ in shader
    window->setRenderMode(HDR_WindowType::Mode_BT2020_HLG_10bit);
  } else {
    // PQ mode (default)
    window->setRenderMode(HDR_WindowType::Mode_BT2020_PQ_10bit);
  }

  const int toneMapNits = settings.value("ToneMapTargetNits", 1000).toInt();
  window->setToneMapTargetNits(
      static_cast<float>(qBound(500, toneMapNits, 10000)));
}

// Internal helper to create HDR window with QWidget container
// The actual window type (OpenGL or RHI) is determined at compile time
static QWidget* createHDRContainerForWindow(HDR_WindowType*& outWindow,
                                            QWidget* parent,
                                            const HDRDetection::HDRCapabilities& caps)
{
  outWindow = new HDR_WindowType();
  outWindow->setHDRCapabilities(caps);

  if (caps.isHDRSupported) {
    HDRRenderingManager::applyHDRWindowSettings(outWindow);
  }
  // Note: If HDR is not supported, this function should not be called.
  // The application will restart in SDR mode via onHDRDetectionComplete().
  // If we reach here without HDR support, use PQ as fallback.
  QWidget* container = QWidget::createWindowContainer(outWindow, parent);
  container->setFocusPolicy(Qt::StrongFocus);
  container->setAttribute(Qt::WA_NativeWindow, true);
  return container;
}

void HDRRenderingManager::startHDRDetection()
{
  QMutexLocker locker(&m_stateMutex);
  
  // Create HDR detection worker if not already created
  if (!m_hdrDetectionWorker) {
    m_hdrDetectionWorker = new HDRDetectionWorker(this);
    
    // Connect signals
    connect(m_hdrDetectionWorker, &HDRDetectionWorker::detectionComplete,
            this, &HDRRenderingManager::onHDRDetectionComplete);
    connect(m_hdrDetectionWorker, &HDRDetectionWorker::detectionFailed,
            this, &HDRRenderingManager::onHDRDetectionFailed);
  }
  
  // Get reference to worker and release lock before starting detection
  HDRDetectionWorker* worker = m_hdrDetectionWorker;
  locker.unlock();
  
  worker->startDetection();
}

bool HDRRenderingManager::isHDRDetectionInProgress() const
{
  QMutexLocker locker(&m_stateMutex);
  return m_hdrDetectionWorker && m_hdrDetectionWorker->isDetecting();
}

void HDRRenderingManager::updateHDRFrame(const QImage& frame)
{
  // Only require active HDR and a valid window; let the window guard its own readiness.
  QPointer<HDR_WindowType> window;
  bool                     shouldRetryAttach = false;
  {
    QMutexLocker locker(&m_stateMutex);
    if ((!m_hdrWindow) || !m_useHDRRendering) {
      return;
    }
    window = m_hdrWindow;
    shouldRetryAttach = (m_hdrContainer == nullptr);
  }

  if (shouldRetryAttach && !m_attachScheduled) {
    m_attachScheduled = true;
    QTimer::singleShot(0, this, [this]() {
      m_attachScheduled = false;
      this->attemptAttachToSplitView();
    });
  }

  // QPointer turns null automatically if cleanup deletes the window after unlocking.
  if (window) {
    window->updateFrame(frame);
  }
}

// GPU-based YUV->RGB conversion path for HDR 10-bit rendering
// This bypasses CPU conversion and uploads raw YUV data directly to GPU textures
void HDRRenderingManager::updateHDRFrameYUV(const QByteArray& yuvData,
                                             int width, int height,
                                             const video::yuv::PixelFormatYUV& format,
                                             video::yuv::ColorConversion colorConversion)
{
  QPointer<HDR_WindowType> window;
  bool                     shouldRetryAttach = false;
  {
    QMutexLocker locker(&m_stateMutex);
    if ((!m_hdrWindow) || !m_useHDRRendering) {
      return;
    }
    window = m_hdrWindow;
    shouldRetryAttach = (m_hdrContainer == nullptr);
  }

  if (shouldRetryAttach && !m_attachScheduled) {
    m_attachScheduled = true;
    QTimer::singleShot(0, this, [this]() {
      m_attachScheduled = false;
      this->attemptAttachToSplitView();
    });
  }

  if (window) {
    window->updateFrameYUV(yuvData, width, height, format, colorConversion);
  }
}

// Zero-copy version using move semantics for caller-owned buffers
void HDRRenderingManager::updateHDRFrameYUVMove(QByteArray&& yuvData,
                                                 int width, int height,
                                                 const video::yuv::PixelFormatYUV& format,
                                                 video::yuv::ColorConversion colorConversion)
{
  QPointer<HDR_WindowType> window;
  bool                     shouldRetryAttach = false;
  {
    QMutexLocker locker(&m_stateMutex);
    if ((!m_hdrWindow) || !m_useHDRRendering) {
      return;
    }
    window = m_hdrWindow;
    shouldRetryAttach = (m_hdrContainer == nullptr);
  }

  if (shouldRetryAttach && !m_attachScheduled) {
    m_attachScheduled = true;
    QTimer::singleShot(0, this, [this]() {
      m_attachScheduled = false;
      this->attemptAttachToSplitView();
    });
  }

  if (window) {
    window->updateFrameYUVMove(std::move(yuvData), width, height, format, colorConversion);
  }
}

void HDRRenderingManager::onHDRDetectionComplete(const HDRDetection::HDRCapabilities& capabilities)
{
  if (capabilities.isHDRSupported) {
    // Store capabilities and enable HDR rendering atomically
    {
      QMutexLocker locker(&m_stateMutex);
      m_hdrCapabilities = capabilities;
      m_useHDRRendering = true;
    }

    // Ensure we have an HDR window instance ready
    {
      QMutexLocker locker(&m_stateMutex);
      if (!m_hdrWindow) {
        m_hdrWindow = new HDR_WindowType();
        m_hdrWindow->setHDRCapabilities(capabilities);

        if (capabilities.isHDRSupported) {
          // Delegate to the shared settings applier so render-mode and tone
          // mapping logic lives in exactly one place.
          HDRRenderingManager::applyHDRWindowSettings(m_hdrWindow);
        }

        // Track readiness when rendering backend initializes later
        connect(m_hdrWindow, &HDR_WindowType::widgetInitialized, this, [this]() {
          QMutexLocker readyLocker(&m_stateMutex);
          m_isHDRWidgetReady = true;
        });
      }
    }

    // Try to attach to SplitViewWidget now; if not yet available, retry shortly
    attemptAttachToSplitView();
  } else {
    // HDR is not supported on current display - restart with HDR disabled
    qWarning() << "[HDR] Display does not support HDR:" << capabilities.errorMessage;
    
    // Disable HDR in settings (so restart will use SDR mode)
    QSettings settings;
    settings.setValue("Enable10BitDisplay", false);
    settings.sync();
    
    // Show restart message to user
    QString errorMsg = QString("HDR not supported: %1\n\nApplication will restart in SDR mode.")
                      .arg(capabilities.errorMessage.isEmpty() ? 
                           "Display does not support 10-bit BT.2020 PQ" : 
                           capabilities.errorMessage);
    
    emit hdrDetectionFailed(errorMsg);
    
    // Schedule application restart (delay to allow message display)
    QTimer::singleShot(2000, qApp, []() {
      qApp->quit();
      QProcess::startDetached(qApp->applicationFilePath(), qApp->arguments());
    });
  }
}

void HDRRenderingManager::onHDRDetectionFailed(const QString& error)
{
  qWarning() << "[HDR] Detection failed:" << error;
  
  // Disable HDR in settings
  QSettings settings;
  settings.setValue("Enable10BitDisplay", false);
  settings.sync();
  
  emit hdrDetectionFailed(error);
  
  // Schedule application restart
  QTimer::singleShot(2000, qApp, []() {
    qApp->quit();
    QProcess::startDetached(qApp->applicationFilePath(), qApp->arguments());
  });
}

void HDRRenderingManager::cleanupHDRResources()
{
    // Do not keep raw pointers to the HDR container/window during application shutdown.
  // The QWidget returned from createWindowContainer() usually owns the QWindow and will delete it.
  // We therefore delete the container first (if present). If the window exists without a container
  // (e.g., detection succeeded but we never attached to the UI), we delete the window explicitly.

  QPointer<QWidget> containerToDelete;
  QPointer<HDR_WindowType> windowToDelete;
  HDRDetectionWorker* workerToDelete = nullptr;

  {
    QMutexLocker locker(&m_stateMutex);
    m_isHDRWidgetReady = false;

    containerToDelete = m_hdrContainer;
    windowToDelete = m_hdrWindow;
    workerToDelete = m_hdrDetectionWorker;

    // Clear members first so other threads stop using them immediately.
    m_hdrContainer = nullptr;
    m_hdrWindow = nullptr;
    m_hdrDetectionWorker = nullptr;
  }

  // Clear any pending frame before destroying resources to avoid driver crashes.
  if (windowToDelete) {
    windowToDelete->clearFrame();
  }

  // If we have a container, it typically owns the window (and will delete it).
  if (containerToDelete) {
    delete containerToDelete;
    containerToDelete = nullptr;
  } else if (windowToDelete) {
    delete windowToDelete;
    windowToDelete = nullptr;
  }

  if (workerToDelete) {
    workerToDelete->terminateWorker();
    delete workerToDelete;
    workerToDelete = nullptr;
  }
}

// Attempt to find the SplitViewWidget and attach the HDR window to it.
// Retry with a longer budget to tolerate delayed UI restore/layout completion.
void HDRRenderingManager::attemptAttachToSplitView()
{
  // 40 retries * 250ms ~= 10s total budget
  const int kMaxRetries = 40;
  const int kRetryIntervalMs = 250;

  // Check if already attached
  {
    QMutexLocker locker(&m_stateMutex);
    if (m_hdrContainer && m_hdrWindow) {
      return;
    }
  }

  // Find SplitViewWidget in the main window hierarchy.
  // Priority:
  // 1) objectName == "displaySplitView" (main view)
  // 2) visible candidates
  // 3) largest candidate by area
  splitViewWidget* splitView = nullptr;
  QWidgetList      topLevelWidgets = QApplication::topLevelWidgets();

  const auto selectFromMainWindow = [&](QMainWindow* mainWindow) -> splitViewWidget* {
    if (!mainWindow)
      return nullptr;

    splitViewWidget* named = mainWindow->findChild<splitViewWidget*>("displaySplitView");
    if (named) {
      return named;
    }

    const auto candidates = mainWindow->findChildren<splitViewWidget*>();
    splitViewWidget* bestCandidate = nullptr;
    int              bestScore = std::numeric_limits<int>::min();

    for (splitViewWidget* candidate : candidates) {
      const bool visibleToMain = candidate->isVisibleTo(mainWindow);
      const bool windowVisible = candidate->window() && candidate->window()->isVisible();
      const QRect rect = candidate->rect();
      const int area = rect.width() * rect.height();

      int score = area;
      if (windowVisible)
        score += 1000000;
      if (visibleToMain)
        score += 1000000;
      if (mainWindow->isVisible())
        score += 500000;


      if (!bestCandidate || score > bestScore) {
        bestCandidate = candidate;
        bestScore = score;
      }
    }

    if (bestCandidate) {
    }

    return bestCandidate;
  };

  // Pass 1: visible main windows first.
  for (QWidget* widget : topLevelWidgets) {
    if (QMainWindow* mainWindow = qobject_cast<QMainWindow*>(widget)) {
      if (!mainWindow->isVisible())
        continue;
      splitView = selectFromMainWindow(mainWindow);
      if (splitView)
        break;
    }
  }

  // Pass 2: if still not found, include hidden/unmapped windows as fallback.
  if (!splitView) {
    for (QWidget* widget : topLevelWidgets) {
      if (QMainWindow* mainWindow = qobject_cast<QMainWindow*>(widget)) {
        splitView = selectFromMainWindow(mainWindow);
        if (splitView)
          break;
      }
    }
  }

  if (splitView) {
    HDRDetection::HDRCapabilities caps;
    HDR_WindowType* existingWindow = nullptr;
    {
      QMutexLocker locker(&m_stateMutex);
      caps = m_hdrCapabilities;
      existingWindow = m_hdrWindow;
    }

    QWidget* container = nullptr;

    // Check if we already have an HDR window (created in onHDRDetectionComplete)
    if (existingWindow) {
      container = QWidget::createWindowContainer(existingWindow, splitView);
      container->setFocusPolicy(Qt::StrongFocus);
      container->setAttribute(Qt::WA_NativeWindow, true);
    } else {
      HDR_WindowType* createdWindow = nullptr;
      container = createHDRContainerForWindow(createdWindow, splitView, caps);
      {
        QMutexLocker locker(&m_stateMutex);
        m_hdrWindow = createdWindow;
      }
    }

    {
      QMutexLocker locker(&m_stateMutex);
      m_hdrContainer = container;
    }

    splitView->setHDROverlayContainer(container, existingWindow ? existingWindow : m_hdrWindow.data());

    emit hdrRenderingStateChangedWindow(true, existingWindow ? existingWindow : m_hdrWindow.data());

    // Reset retry state on success
    m_attachRetryCount = 0;
    m_attachScheduled = false;
    return;
  }

  // Schedule retry if limits allow.
  // Keep HDR mode enabled; do not fall back to SDR here.
  if (m_attachRetryCount < kMaxRetries) {
    m_attachRetryCount++;
    if (!m_attachScheduled) {
      m_attachScheduled = true;
      qWarning() << "[HDR-ATTACH] Split view not ready. Retrying"
                 << m_attachRetryCount << "/" << kMaxRetries
                 << "after" << kRetryIntervalMs << "ms";
      QTimer::singleShot(kRetryIntervalMs, this, [this]() {
        m_attachScheduled = false;
        this->attemptAttachToSplitView();
      });
    }
  } else {
    // Do not permanently give up: UI could become ready after delayed restore/layout.
    qWarning() << "[HDR-ATTACH] Failed to attach HDR window after" << kMaxRetries
               << "attempts. HDR stays enabled; re-arming attach retry loop.";
    m_attachRetryCount = 0;
    if (!m_attachScheduled) {
      m_attachScheduled = true;
      QTimer::singleShot(1000, this, [this]() {
        m_attachScheduled = false;
        this->attemptAttachToSplitView();
      });
    }
  }
}


bool HDRRenderingManager::isHDRRenderingActive() const
{
  QMutexLocker locker(&m_stateMutex);
  return m_useHDRRendering;
}

HDR_WindowType* HDRRenderingManager::getHDRWindow() const
{
  QMutexLocker locker(&m_stateMutex);
  return m_hdrWindow;
}

HDRDetection::HDRCapabilities HDRRenderingManager::getHDRCapabilities() const
{
  QMutexLocker locker(&m_stateMutex);
  return m_hdrCapabilities;  // Deep copy to avoid reference issues
}


bool HDRRenderingManager::isHDRRenderingActive_locked() const
{
  // Assumes caller already holds m_stateMutex
  return m_useHDRRendering;
}

void HDRRenderingManager::setHDRWidgetReady_locked(bool ready)
{
  // Assumes caller already holds m_stateMutex
  m_isHDRWidgetReady = ready;
}

bool HDRRenderingManager::isHDRWidgetReady_locked() const
{
  // Assumes caller already holds m_stateMutex  
  return m_isHDRWidgetReady;
}
