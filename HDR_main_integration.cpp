/*
 * HDR Integration Code for YUView main.cpp
 * 
 * This code should be integrated into YUViewApp/src/yuviewapp.cpp
 * The HDR surface format setup must occur BEFORE QApplication construction
 * to enable native 10-bit/16-bit HDR rendering capabilities.
 * 
 * Based on Krita's proven HDR implementation approach.
 */

#include <QCoreApplication>
#include <QSurfaceFormat>
#include <QDebug>

// Add these includes to the existing yuviewapp.cpp
#include <common/Typedef.h>
#include <ui/YUViewApplication.h>
#include <video/HDRDetection.h>  // Add this new include

int main(int argc, char *argv[])
{
    // ========================================
    // HDR INITIALIZATION (ADD THIS SECTION)
    // ========================================
    
    // Early HDR capability detection - must be done before QApplication creation
    // This performs a minimal check to see if HDR should be enabled
    qDebug() << "YUView: Detecting HDR capabilities...";
    
    // Create a temporary QCoreApplication just for HDR detection
    // This is needed because some HDR detection APIs require an application context
    QCoreApplication tempApp(argc, argv);
    
    // Detect HDR capabilities
    auto hdrDetection = HDRDetection::instance();
    auto hdrCapabilities = hdrDetection->detectHDRCapabilities();
    
    QSurfaceFormat hdrFormat;
    bool useHDR = false;
    
    if (hdrCapabilities.isHDRSupported) {
        qDebug() << "HDR display detected:" << hdrCapabilities.displayName;
        qDebug() << "HDR mode:" << HDRDetection::getHDRModeDescription(hdrCapabilities.supportedMode);
        qDebug() << "Max luminance:" << hdrCapabilities.maxLuminance << "nits";
        qDebug() << "Bits per channel:" << hdrCapabilities.bitsPerChannel;
        
        // Configure HDR surface format based on detected capabilities
        hdrFormat = HDRDetection::getHDRSurfaceFormat(hdrCapabilities.supportedMode);
        useHDR = true;
        
    } else {
        qDebug() << "HDR not supported:" << hdrCapabilities.errorMessage;
        qDebug() << "Using standard 8-bit SDR rendering";
        
        // Fall back to standard SDR format
        hdrFormat = HDRDetection::getHDRSurfaceFormat(HDRDetection::SDR_Only);
    }
    
    // Configure additional OpenGL context parameters
    hdrFormat.setProfile(QSurfaceFormat::CoreProfile);
    hdrFormat.setVersion(3, 3);  // Minimum OpenGL 3.3 required for HDR shaders
    hdrFormat.setSwapBehavior(QSurfaceFormat::DoubleBuffer);
    hdrFormat.setSwapInterval(1);  // Enable V-Sync for smooth HDR playback
    
    // Set the HDR format as the default for the entire application
    // This MUST be done before QApplication construction
    QSurfaceFormat::setDefaultFormat(hdrFormat);
    
    if (useHDR) {
        qDebug() << "HDR surface format configured:";
        qDebug() << "  Red buffer size:" << hdrFormat.redBufferSize();
        qDebug() << "  Green buffer size:" << hdrFormat.greenBufferSize(); 
        qDebug() << "  Blue buffer size:" << hdrFormat.blueBufferSize();
        qDebug() << "  Alpha buffer size:" << hdrFormat.alphaBufferSize();
        qDebug() << "  OpenGL version:" << hdrFormat.majorVersion() << "." << hdrFormat.minorVersion();
    }
    
    // Clean up temporary application
    tempApp.quit();
    
    // ========================================
    // END HDR INITIALIZATION
    // ========================================

    // Original YUView initialization code continues here...
    
#if QT_VERSION >= QT_VERSION_CHECK(5, 6, 0) && QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
    QCoreApplication::setAttribute(Qt::AA_EnableHighDpiScaling); // DPI support
    QCoreApplication::setAttribute(Qt::AA_UseHighDpiPixmaps); // DPI support
#endif
    QCoreApplication::setAttribute(Qt::AA_SynthesizeMouseForUnhandledTouchEvents, false);
    QCoreApplication::setAttribute(Qt::AA_SynthesizeTouchForUnhandledMouseEvents, false);

    qRegisterMetaType<recacheIndicator>("recacheIndicator");
    
    // Create the main YUView application
    // HDR surface format is now configured and will be used by all OpenGL contexts
    YUViewApplication app(argc, argv);

    return app.returnCode;
}

/*
 * INTEGRATION INSTRUCTIONS:
 * 
 * 1. Add the HDR initialization section to the existing yuviewapp.cpp main() function
 * 2. Add the new include: #include <video/HDRDetection.h>
 * 3. Ensure HDRDetection.h/cpp are compiled and linked with YUViewLib
 * 4. Update YUViewLib.pro to include the new HDR source files
 * 
 * CRITICAL NOTES:
 * - The QSurfaceFormat::setDefaultFormat() call MUST occur before QApplication creation
 * - The HDR detection creates a temporary QCoreApplication for API compatibility
 * - This approach follows Krita's proven HDR initialization pattern
 * - The surface format affects ALL QOpenGLWidget instances in the application
 * 
 * BUILD SYSTEM INTEGRATION:
 * Add to YUViewLib.pro:
 * 
 * SOURCES += \
 *     src/video/HDRDetection.cpp \
 *     src/video/HDR_VideoWidget.cpp
 * 
 * HEADERS += \
 *     src/video/HDRDetection.h \
 *     src/video/HDR_VideoWidget.h
 * 
 * # Windows-specific libraries for HDR detection
 * win32 {
 *     LIBS += -ldxgi -luser32
 * }
 * 
 * # Add shader resources
 * RESOURCES += resources/shaders/shaders.qrc
 * 
 * And create resources/shaders/shaders.qrc:
 * 
 * <RCC>
 *     <qresource prefix="/shaders">
 *         <file>hdr_vertex.vert</file>
 *         <file>hdr_fragment.frag</file>
 *     </qresource>
 * </RCC>
 */