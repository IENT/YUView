#include "HDRGlobalState.h"

HDRGlobalState* HDRGlobalState::instance()
{
    static HDRGlobalState instance;
    return &instance;
}

bool HDRGlobalState::isHDRModeEnabled() const
{
    QMutexLocker locker(&m_mutex);
    return m_hdrModeEnabled;
}

void HDRGlobalState::setHDRModeEnabled(bool enabled)
{
    bool changed = false;
    {
        QMutexLocker locker(&m_mutex);
        if (m_hdrModeEnabled != enabled) {
            m_hdrModeEnabled = enabled;
            changed = true;
        }
    }
    
    if (changed) {
        emit hdrModeChanged(enabled);
    }
}

bool HDRGlobalState::isHDRRequestedByUser() const
{
    QMutexLocker locker(&m_mutex);
    return m_hdrRequestedByUser;
}

void HDRGlobalState::setHDRRequestedByUser(bool requested)
{
    QMutexLocker locker(&m_mutex);
    m_hdrRequestedByUser = requested;
}