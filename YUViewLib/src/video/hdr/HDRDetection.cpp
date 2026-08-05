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
        } else {
            HDRCapabilities caps;
            caps.errorMessage = "No screen available for HDR detection";
            return caps;
        }
    }

#ifdef Q_OS_WIN
    m_currentCapabilities = detectHDRCapabilities_Windows(screen);
#endif

    m_capabilitiesDetected = true;
    // OPTIMIZATION: Remove verbose logging in release mode for performance
    
    emit hdrCapabilitiesChanged(m_currentCapabilities);
    
    return m_currentCapabilities;
}

// Removed: isHDRActiveOnScreen, getHDRSurfaceFormat, isHDRModeSupported
// These are no longer needed as format is set at startup

QString HDRDetection::getHDRModeDescription(HDRMode mode)
{
    switch (mode) {
    case BT2020_PQ_10bit:
        return "HDR10/BT.2020 PQ (10-bit)";
    case BT2020_HLG_10bit:
        return "HLG/BT.2020 (10-bit)";
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
    return caps;

}

bool HDRDetection::checkDXGIHDRSupport(const QString& displayName, HDRCapabilities& caps)
{
        // This used to ignore the incoming displayName and return the first
    // HDR-capable output it happened to enumerate. On multi-monitor setups
    // that produced wrong answers ("window lives on screen A, caps come from
    // screen B"). We now:
    //   1) Match desc1.DeviceName against the caller-provided name first,
    //      and only fall back to "any HDR output" if we never find the
    //      requested screen.
    //   2) Balance every successful CoInitializeEx with a CoUninitialize on
    //      every exit path, so COM lifetime on this thread is clean.

    bool comInitialized = false;
    HRESULT hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    if (SUCCEEDED(hr)) {
        comInitialized = true;
    } else if (hr != RPC_E_CHANGED_MODE) {
        caps.errorMessage = "Failed to initialize COM for DXGI";
        return false;
    }

    // RAII-style cleanup for CoInitialize without pulling in extra headers.
    struct ComGuard {
        bool active;
        ~ComGuard() { if (active) CoUninitialize(); }
    } comGuard{comInitialized};

    bool found = false;
    HDRCapabilities matched;        // result matching displayName
    HDRCapabilities firstAnyHdr;    // fallback if exact match fails
    bool haveAnyHdr = false;

    try {
        IDXGIFactory1* pFactory = nullptr;
        hr = CreateDXGIFactory1(__uuidof(IDXGIFactory1), (void**)&pFactory);
        if (FAILED(hr) || !pFactory) {
            caps.errorMessage = "Failed to create DXGI factory";
            return false;
        }

        IDXGIAdapter1* pAdapter = nullptr;
        for (UINT i = 0; pFactory->EnumAdapters1(i, &pAdapter) != DXGI_ERROR_NOT_FOUND; ++i) {
            IDXGIOutput* pOutput = nullptr;
            for (UINT j = 0; pAdapter->EnumOutputs(j, &pOutput) != DXGI_ERROR_NOT_FOUND; ++j) {
                IDXGIOutput6* pOutput6 = nullptr;
                hr = pOutput->QueryInterface(__uuidof(IDXGIOutput6), (void**)&pOutput6);
                if (SUCCEEDED(hr) && pOutput6) {
                    DXGI_OUTPUT_DESC1 desc1 = {};
                    if (SUCCEEDED(pOutput6->GetDesc1(&desc1))) {
                        const bool isHdrOutput =
                            (desc1.BitsPerColor >= 10) &&
                            (desc1.ColorSpace == DXGI_COLOR_SPACE_RGB_FULL_G2084_NONE_P2020) &&
                            (desc1.MaxLuminance > 100.0f);
                        if (isHdrOutput) {
                            HDRCapabilities local;
                            local.isHDRSupported = true;
                            local.supportedMode = BT2020_PQ_10bit;
                            local.maxLuminance = desc1.MaxLuminance;
                            local.minLuminance = desc1.MinLuminance;
                            local.bitsPerChannel = desc1.BitsPerColor;
                            local.displayName = QString::fromWCharArray(desc1.DeviceName);

                            const bool nameMatches =
                                !displayName.isEmpty() &&
                                local.displayName.compare(displayName, Qt::CaseInsensitive) == 0;

                            if (nameMatches) {
                                matched = local;
                                found = true;
                            } else if (!haveAnyHdr) {
                                firstAnyHdr = local;
                                haveAnyHdr = true;
                            }
                        }
                    }
                    pOutput6->Release();
                }
                pOutput->Release();
                if (found) break;
            }
            pAdapter->Release();
            if (found) break;
        }

        pFactory->Release();
    } catch (...) {
        caps.errorMessage = "Exception occurred during DXGI HDR detection";
        return false;
    }

    if (found) {
        caps = matched;
        return true;
    }

    if (haveAnyHdr) {
        // Fallback for the case where the caller did not specify a screen
        // (empty displayName) or when the QScreen name did not match any
        // DXGI output name exactly. This preserves the old behavior, but
        // at least we only take the fallback when the precise match fails.
        caps = firstAnyHdr;
        if (!displayName.isEmpty() &&
            caps.displayName.compare(displayName, Qt::CaseInsensitive) != 0) {
            caps.errorMessage =
                QStringLiteral("No DXGI HDR output matched screen '%1'; using '%2' as fallback")
                    .arg(displayName, caps.displayName);
        }
        return true;
    }

    caps.errorMessage = "No HDR-capable displays found via DXGI";
    return false;
}


#else
// Non-Windows platforms: HDR detection not implemented
HDRDetection::HDRCapabilities HDRDetection::detectHDRCapabilities_Windows(QScreen* screen)
{
    Q_UNUSED(screen);
    HDRCapabilities caps;
    caps.errorMessage = "HDR detection only available on Windows";
    return caps;
}

bool HDRDetection::checkDXGIHDRSupport(const QString& displayName, HDRCapabilities& caps)
{
    Q_UNUSED(displayName);
    caps.errorMessage = "DXGI HDR detection only available on Windows";
    return false;
}
#endif


