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
    /**
     * @brief HDR transfer function modes supported by the system
     * 
     * SDR_Only: Standard Dynamic Range (8-bit)
     * BT2020_PQ_10bit: HDR10/PQ using SMPTE ST.2084 Perceptual Quantizer
     * BT2020_HLG_10bit: HLG using ARIB STD-B67 Hybrid Log-Gamma
     */
    enum HDRMode {
        SDR_Only = 0,
        BT2020_PQ_10bit = 1,      // 10-bit HDR10 PQ mode (SMPTE ST.2084)
        BT2020_HLG_10bit = 2,     // 10-bit HLG mode (ARIB STD-B67)
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
    
    // Get human-readable description of HDR mode
    static QString getHDRModeDescription(HDRMode mode);
    
    // Get the currently detected HDR capabilities (cached)
    const HDRCapabilities& getCurrentCapabilities() const { return m_currentCapabilities; }

signals:
    void hdrCapabilitiesChanged(const HDRCapabilities& capabilities);

private:
    explicit HDRDetection(QObject* parent = nullptr);
    
    // HDR detection helper functions (implementation varies by platform)
    HDRCapabilities detectHDRCapabilities_Windows(QScreen* screen);
    bool checkDXGIHDRSupport(const QString& displayName, HDRCapabilities& caps);
    
private:
    static HDRDetection* s_instance;
    HDRCapabilities m_currentCapabilities;
    bool m_capabilitiesDetected;
};

#endif // HDRDETECTION_H
