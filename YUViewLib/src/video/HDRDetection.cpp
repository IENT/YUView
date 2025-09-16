#include "HDRDetection.h"

#include <QApplication>
#include <QDebug>
#include <QOpenGLContext>
#include <QOpenGLFunctions>
#include <QScreen>
#include <QWindow>

#ifdef Q_OS_WIN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <comdef.h>
#include <dxgi1_6.h>
// Undefine Windows macros that might conflict with other code
#ifdef TC_RESERVED
#undef TC_RESERVED
#endif
#ifdef CP_UNSPECIFIED  
#undef CP_UNSPECIFIED
#endif
#ifdef MC_UNSPECIFIED
#undef MC_UNSPECIFIED
#endif
#ifdef CSP_UNKNOWN
#undef CSP_UNKNOWN
#endif
#ifdef CSP_RESERVED
#undef CSP_RESERVED
#endif
#ifdef ERROR
#undef ERROR
#endif
#ifdef min
#undef min
#endif
#ifdef max
#undef max
#endif
// Note: WinRT support temporarily disabled for MinGW compatibility
// #include <winrt/Windows.Graphics.Display.h>
// #include <winrt/base.h>
// using namespace winrt;
// using namespace Windows::Graphics::Display;
#endif

HDRDetection* HDRDetection::s_instance = nullptr;

HDRDetection* HDRDetection::instance()
{
    if (!s_instance) {
        s_instance = new HDRDetection();
    }
    return s_instance;
}

HDRDetection::HDRDetection(QObject* parent)
    : QObject(parent)
    , m_capabilitiesDetected(false)
{
}

HDRDetection::HDRCapabilities HDRDetection::detectHDRCapabilities(QWidget* parent)
{
    qDebug() << "YUView HDR Detection: Starting display capability analysis...";
    
    QScreen* screen = nullptr;
    
    if (parent && parent->window()) {
        screen = parent->window()->screen();
    } else {
        screen = QApplication::primaryScreen();
    }
    
    if (!screen) {
        // Try to get any available screen as fallback
        auto screens = QApplication::screens();
        if (!screens.isEmpty()) {
            screen = screens.first();
            qDebug() << "YUView HDR Detection: Using fallback screen:" << screen->name();
        } else {
            HDRCapabilities caps;
            caps.errorMessage = "No screen available for HDR detection";
            qDebug() << "YUView HDR Detection:" << caps.errorMessage;
            return caps;
        }
    }
    
    qDebug() << "YUView HDR Detection: Analyzing screen:" << screen->name() 
             << "Depth:" << screen->depth() << "bits";

#ifdef Q_OS_WIN
    m_currentCapabilities = detectHDRCapabilities_Windows(screen);
#elif defined(Q_OS_MACOS)
    m_currentCapabilities = detectHDRCapabilities_macOS(screen);
#elif defined(Q_OS_LINUX)
    m_currentCapabilities = detectHDRCapabilities_Linux(screen);
#else
    m_currentCapabilities = detectHDRCapabilities_Qt(screen);
#endif

    m_capabilitiesDetected = true;
    
    // Log detection results
    if (m_currentCapabilities.isHDRSupported) {
        qDebug() << "YUView HDR Detection: ✓ HDR display detected!" 
                 << "Mode:" << getHDRModeDescription(m_currentCapabilities.supportedMode)
                 << "Max luminance:" << m_currentCapabilities.maxLuminance << "nits";
    } else {
        qDebug() << "YUView HDR Detection: ✗ HDR not supported." 
                 << "Reason:" << m_currentCapabilities.errorMessage
                 << "- Using standard 8-bit SDR rendering";
    }
    
    emit hdrCapabilitiesChanged(m_currentCapabilities);
    
    return m_currentCapabilities;
}

bool HDRDetection::isHDRActiveOnScreen(QScreen* screen)
{
    if (!screen) {
        return false;
    }
    
    // Check if screen depth indicates HDR capability
    int depth = screen->depth();
    return depth >= 30; // 10 bits per channel = 30 bits total
}

QSurfaceFormat HDRDetection::getHDRSurfaceFormat(HDRMode mode)
{
    QSurfaceFormat format;
    format.setProfile(QSurfaceFormat::CoreProfile);
    format.setVersion(3, 3);
    format.setSwapBehavior(QSurfaceFormat::DoubleBuffer);
    
    switch (mode) {
    case BT2020_PQ_10bit:
        // 10-bit per channel for HDR10/BT2020_PQ mode
        format.setRedBufferSize(10);
        format.setGreenBufferSize(10);
        format.setBlueBufferSize(10);
        format.setAlphaBufferSize(2);
        // Note: Qt doesn't directly support BT2020 PQ color space
        // This will be handled in the shader pipeline
        break;
        
    case BT709_G10_16bit:
        // 16-bit per channel for scRGB/Linear mode
        format.setRedBufferSize(16);
        format.setGreenBufferSize(16);
        format.setBlueBufferSize(16);
        format.setAlphaBufferSize(16);
        // Note: scRGB color space handling in shader
        break;
        
    case SDR_Only:
    default:
        // Standard 8-bit SDR
        format.setRedBufferSize(8);
        format.setGreenBufferSize(8);
        format.setBlueBufferSize(8);
        format.setAlphaBufferSize(8);
        break;
    }
    
    return format;
}

bool HDRDetection::isHDRModeSupported(HDRMode mode) const
{
    if (!m_capabilitiesDetected) {
        return false;
    }
    
    return m_currentCapabilities.isHDRSupported && 
           (m_currentCapabilities.supportedMode == mode || mode == SDR_Only);
}

QString HDRDetection::getHDRModeDescription(HDRMode mode)
{
    switch (mode) {
    case BT2020_PQ_10bit:
        return "HDR10/BT.2020 PQ (10-bit)";
    case BT709_G10_16bit:
        return "scRGB/Rec.709 Linear (16-bit)";
    case SDR_Only:
    default:
        return "Standard Dynamic Range (8-bit)";
    }
}

#ifdef Q_OS_WIN
HDRDetection::HDRCapabilities HDRDetection::detectHDRCapabilities_Windows(QScreen* screen)
{
    HDRCapabilities caps;
    caps.displayName = screen->name();
    
    // Try DXGI method first (most reliable)
    if (checkDXGIHDRSupport(screen->name(), caps)) {
        return caps;
    }
    
    // Fallback to WinRT Advanced Color API
    if (checkWinRTAdvancedColor(caps)) {
        return caps;
    }
    
    // Final fallback to Qt-based detection
    return detectHDRCapabilities_Qt(screen);
}

bool HDRDetection::checkDXGIHDRSupport(const QString& displayName, HDRCapabilities& caps)
{
    // Re-enabled DXGI HDR detection following Krita's proven approach
    qDebug() << "Performing DXGI HDR detection for display:" << displayName;
    
    try {
        // Initialize COM
        HRESULT hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
        if (FAILED(hr) && hr != RPC_E_CHANGED_MODE) {
            caps.errorMessage = "Failed to initialize COM for DXGI";
            return false;
        }
        
        // Create DXGI factory
        IDXGIFactory1* pFactory = nullptr;
        hr = CreateDXGIFactory1(__uuidof(IDXGIFactory1), (void**)&pFactory);
        if (FAILED(hr)) {
            caps.errorMessage = "Failed to create DXGI factory";
            return false;
        }
        
        // Enumerate adapters
        IDXGIAdapter1* pAdapter = nullptr;
        for (UINT i = 0; pFactory->EnumAdapters1(i, &pAdapter) != DXGI_ERROR_NOT_FOUND; ++i) {
            // Enumerate outputs for this adapter
            IDXGIOutput* pOutput = nullptr;
            for (UINT j = 0; pAdapter->EnumOutputs(j, &pOutput) != DXGI_ERROR_NOT_FOUND; ++j) {
                // Query for IDXGIOutput6 to get HDR information
                IDXGIOutput6* pOutput6 = nullptr;
                hr = pOutput->QueryInterface(__uuidof(IDXGIOutput6), (void**)&pOutput6);
                if (SUCCEEDED(hr)) {
                    DXGI_OUTPUT_DESC1 desc1;
                    hr = pOutput6->GetDesc1(&desc1);
                    if (SUCCEEDED(hr)) {
                        // Check if this is an HDR-capable display
                        if (desc1.BitsPerColor >= 10 && 
                            desc1.ColorSpace == DXGI_COLOR_SPACE_RGB_FULL_G2084_NONE_P2020 &&
                            desc1.MaxLuminance > 100.0f) {
                            
                            caps.isHDRSupported = true;
                            caps.supportedMode = BT2020_PQ_10bit;
                            caps.maxLuminance = desc1.MaxLuminance;
                            caps.minLuminance = desc1.MinLuminance;
                            caps.bitsPerChannel = desc1.BitsPerColor;
                            
                            // Convert wide string to QString
                            caps.displayName = QString::fromWCharArray(desc1.DeviceName);
                            
                            pOutput6->Release();
                            pOutput->Release();
                            pAdapter->Release();
                            pFactory->Release();
                            return true;
                        }
                        
                        // Check for 16-bit scRGB support
                        if (desc1.BitsPerColor >= 16) {
                            caps.isHDRSupported = true;
                            caps.supportedMode = BT709_G10_16bit;
                            caps.maxLuminance = desc1.MaxLuminance;
                            caps.minLuminance = desc1.MinLuminance;
                            caps.bitsPerChannel = desc1.BitsPerColor;
                            caps.displayName = QString::fromWCharArray(desc1.DeviceName);
                            
                            pOutput6->Release();
                            pOutput->Release();
                            pAdapter->Release();
                            pFactory->Release();
                            return true;
                        }
                    }
                    pOutput6->Release();
                }
                pOutput->Release();
            }
            pAdapter->Release();
        }
        
        pFactory->Release();
        caps.errorMessage = "No HDR-capable displays found via DXGI";
        return false;
        
    } catch (...) {
        caps.errorMessage = "Exception occurred during DXGI HDR detection";
        return false;
    }
}

bool HDRDetection::checkWinRTAdvancedColor(HDRCapabilities& caps)
{
    // WinRT support temporarily disabled for MinGW compatibility
    caps.errorMessage = "WinRT Advanced Color detection not available with MinGW";
    return false;
    
    /*
    // Original WinRT code - commented out for MinGW compatibility
    try {
        // Initialize WinRT
        init_apartment();
        
        auto displayInfo = DisplayInformation::GetForCurrentView();
        if (displayInfo) {
            auto advancedColorInfo = displayInfo.GetAdvancedColorInfo();
            
            if (advancedColorInfo.AdvancedColorSupported() && 
                advancedColorInfo.AdvancedColorEnabled()) {
                
                caps.isHDRSupported = true;
                caps.maxLuminance = advancedColorInfo.MaxLuminanceInNits();
                caps.minLuminance = advancedColorInfo.MinLuminanceInNits();
                
                // Default to 10-bit HDR10 mode for WinRT detection
                caps.supportedMode = BT2020_PQ_10bit;
                caps.bitsPerChannel = 10;
                caps.displayName = "Advanced Color Display";
                
                return true;
            }
        }
        
        caps.errorMessage = "Advanced color not supported or enabled";
        return false;
        
    } catch (...) {
        caps.errorMessage = "Exception occurred during WinRT HDR detection";
        return false;
    }
    */
}
#else
// Provide stub implementations for non-Windows platforms
HDRDetection::HDRCapabilities HDRDetection::detectHDRCapabilities_Windows(QScreen* screen)
{
    return detectHDRCapabilities_Qt(screen);
}

bool HDRDetection::checkDXGIHDRSupport(const QString& displayName, HDRCapabilities& caps)
{
    caps.errorMessage = "DXGI HDR detection only available on Windows";
    return false;
}

bool HDRDetection::checkWinRTAdvancedColor(HDRCapabilities& caps)
{
    caps.errorMessage = "WinRT Advanced Color detection only available on Windows";
    return false;
}
#endif

#ifdef Q_OS_MACOS
HDRDetection::HDRCapabilities HDRDetection::detectHDRCapabilities_macOS(QScreen* screen)
{
    HDRCapabilities caps;
    caps.displayName = screen->name();
    caps.errorMessage = "macOS HDR detection not yet implemented";
    
    // TODO: Implement macOS HDR detection using Core Graphics
    // This would involve checking NSScreen properties and ColorSync profiles
    
    return detectHDRCapabilities_Qt(screen);
}
#endif

#ifdef Q_OS_LINUX
HDRDetection::HDRCapabilities HDRDetection::detectHDRCapabilities_Linux(QScreen* screen)
{
    HDRCapabilities caps;
    caps.displayName = screen->name();
    caps.errorMessage = "Linux HDR detection not yet implemented";
    
    // TODO: Implement Linux HDR detection using Wayland/X11 protocols
    // This would involve checking compositor capabilities and EDID information
    
    return detectHDRCapabilities_Qt(screen);
}
#endif

HDRDetection::HDRCapabilities HDRDetection::detectHDRCapabilities_Qt(QScreen* screen)
{
    HDRCapabilities caps;
    caps.displayName = screen->name();
    
    // Check screen color depth - be more strict about HDR detection
    int depth = screen->depth();
    qDebug() << "Screen depth:" << depth << "for display:" << caps.displayName;
    
    // Only consider exactly 30-bit (10+10+10) or 48-bit (16+16+16) as potential HDR
    if (depth == 30) {
        // 30-bit color (10 bits per channel) - potential HDR capability
        // But still need proper HDR display detection, not just bit depth
        caps.isHDRSupported = false; // Conservative: Qt fallback cannot reliably detect HDR
        caps.supportedMode = SDR_Only;
        caps.bitsPerChannel = 8;
        caps.errorMessage = QString("Qt fallback detection: 30-bit depth detected but cannot verify true HDR capability. Use DXGI detection for accurate HDR support.");
    } else if (depth == 48) {
        // 48-bit color (16 bits per channel) - potential high precision
        caps.isHDRSupported = false; // Conservative: Qt fallback cannot reliably detect HDR
        caps.supportedMode = SDR_Only;
        caps.bitsPerChannel = 8;
        caps.errorMessage = QString("Qt fallback detection: 48-bit depth detected but cannot verify true HDR capability. Use DXGI detection for accurate HDR support.");
    } else {
        // Standard depth (24-bit RGB or 32-bit RGBA) - definitely not HDR
        caps.isHDRSupported = false;
        caps.supportedMode = SDR_Only;
        caps.bitsPerChannel = 8;
        caps.errorMessage = QString("Display depth (%1-bit) is standard SDR. HDR requires 30-bit or 48-bit depth plus HDR display capability.").arg(depth);
    }
    
    // Additional Qt-based checks
    QSurfaceFormat format = QSurfaceFormat::defaultFormat();
    if (caps.isHDRSupported) {
        // Verify OpenGL context can support the required bit depths
        if (format.majorVersion() < 3 || 
            (format.majorVersion() == 3 && format.minorVersion() < 3)) {
            caps.isHDRSupported = false;
            caps.errorMessage = "OpenGL 3.3 or higher required for HDR rendering";
        }
    }
    
    return caps;
}

