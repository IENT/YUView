#pragma once

#include <QObject>
#include <QMutex>

// Singleton class to manage global HDR state across the application
class HDRGlobalState : public QObject
{
    Q_OBJECT

public:
    static HDRGlobalState* instance();
    
    // Thread-safe methods to get/set HDR mode
    bool isHDRModeEnabled() const;
    void setHDRModeEnabled(bool enabled);
    
    // Check if HDR was requested by user (from settings)
    bool isHDRRequestedByUser() const;
    void setHDRRequestedByUser(bool requested);

signals:
    void hdrModeChanged(bool enabled);

private:
    HDRGlobalState() = default;
    ~HDRGlobalState() = default;
    
    // Disable copy and move
    HDRGlobalState(const HDRGlobalState&) = delete;
    HDRGlobalState& operator=(const HDRGlobalState&) = delete;
    HDRGlobalState(HDRGlobalState&&) = delete;
    HDRGlobalState& operator=(HDRGlobalState&&) = delete;
    
    mutable QMutex m_mutex;
    bool m_hdrModeEnabled = false;
    bool m_hdrRequestedByUser = false;
};