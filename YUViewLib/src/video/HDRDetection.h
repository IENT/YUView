#ifndef HDRDETECTION_H
#define HDRDETECTION_H

#include <QObject>
#include <QString>  
#include <QSurfaceFormat>
#include <QScreen>
#include <QWidget>

// Note: Windows headers moved to .cpp file to avoid macro pollution

class HDRDetection : public QObject
{
    Q_OBJECT

public:
    enum HDRMode {
        SDR_Only = 0,
        BT2020_PQ_10bit = 1       // 10-bit HDR10 mode (R:10, G:10, B:10, A:2)
    };

    struct HDRCapabilities {
        bool isHDRSupported;
        HDRMode supportedMode;
        float maxLuminance;       // Maximum luminance in nits
        float minLuminance;       // Minimum luminance in nits  
        int bitsPerChannel;       // Color bit depth per channel
        QString displayName;      // Display device name
        QString errorMessage;     // Error description if HDR not supported
        
        HDRCapabilities() 
            : isHDRSupported(false)
            , supportedMode(SDR_Only)
            , maxLuminance(100.0f)
            , minLuminance(0.1f)
            , bitsPerChannel(8)
            , displayName("Unknown")
            , errorMessage("") {}
    };

    static HDRDetection* instance();
    
    // Main HDR capability detection method
    HDRCapabilities detectHDRCapabilities(QWidget* parent = nullptr);
    
    // Check if HDR is currently active on specific screen
    static bool isHDRActiveOnScreen(QScreen* screen);
    
    // Get appropriate QSurfaceFormat for HDR mode
    static QSurfaceFormat getHDRSurfaceFormat(HDRMode mode);
    
    // Check if specific HDR mode is supported
    bool isHDRModeSupported(HDRMode mode) const;
    
    // Get human-readable description of HDR mode
    static QString getHDRModeDescription(HDRMode mode);
    
    // Get the currently detected HDR capabilities (cached)
    const HDRCapabilities& getCurrentCapabilities() const { return m_currentCapabilities; }

signals:
    void hdrCapabilitiesChanged(const HDRCapabilities& capabilities);

private:
    explicit HDRDetection(QObject* parent = nullptr);
    
    // Simplified HDR detection: Windows DXGI only (PRD requirement)
#ifdef Q_OS_WIN
    HDRCapabilities detectHDRCapabilities_Windows(QScreen* screen);
    bool checkDXGIHDRSupport(const QString& displayName, HDRCapabilities& caps);
    bool checkWinRTAdvancedColor(HDRCapabilities& caps);
#endif
    
    // Qt-based HDR detection (fallback)
    HDRCapabilities detectHDRCapabilities_Qt(QScreen* screen);
    
private:
    static HDRDetection* s_instance;
    HDRCapabilities m_currentCapabilities;
    bool m_capabilitiesDetected;
};

#endif // HDRDETECTION_H