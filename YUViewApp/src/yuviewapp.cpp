/*  This file is part of YUView - The YUV player with advanced analytics toolset
*   <https://github.com/IENT/YUView>
*   Copyright (C) 2015  Institut für Nachrichtentechnik, RWTH Aachen University, GERMANY
*
*   This program is free software; you can redistribute it and/or modify
*   it under the terms of the GNU General Public License as published by
*   the Free Software Foundation; either version 3 of the License, or
*   (at your option) any later version.
*
*   In addition, as a special exception, the copyright holders give
*   permission to link the code of portions of this program with the
*   OpenSSL library under certain conditions as described in each
*   individual source file, and distribute linked combinations including
*   the two.
*   
*   You must obey the GNU General Public License in all respects for all
*   of the code used other than OpenSSL. If you modify file(s) with this
*   exception, you may extend this exception to your version of the
*   file(s), but you are not obligated to do so. If you do not wish to do
*   so, delete this exception statement from your version. If you delete
*   this exception statement from all source files in the program, then
*   also delete it here.
*
*   This program is distributed in the hope that it will be useful,
*   but WITHOUT ANY WARRANTY; without even the implied warranty of
*   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
*   GNU General Public License for more details.
*
*   You should have received a copy of the GNU General Public License
*   along with this program. If not, see <http://www.gnu.org/licenses/>.
*/

#include <QCoreApplication>
#include <QSurfaceFormat>
#include <QDebug>
#include <QSettings>
#include <QMessageBox>

#include <common/Typedef.h>
#include <ui/YUViewApplication.h>
#include <video/HDRDetection.h>

int main(int argc, char *argv[])
{
  // ========================================
  // BASIC OPENGL SETUP - MUST BE FIRST
  // ========================================
  
  // Set basic OpenGL attributes before any application creation
#if QT_VERSION >= QT_VERSION_CHECK(5, 6, 0) && QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
  QCoreApplication::setAttribute(Qt::AA_EnableHighDpiScaling); // DPI support
  QCoreApplication::setAttribute(Qt::AA_UseHighDpiPixmaps); // DPI support
#endif
  QCoreApplication::setAttribute(Qt::AA_SynthesizeMouseForUnhandledTouchEvents,false);
  QCoreApplication::setAttribute(Qt::AA_SynthesizeTouchForUnhandledMouseEvents,false);

  // ========================================
  // STARTUP-BASED HDR DECISION LOGIC (PRD Requirement 5.1-5.5)
  // ========================================

  // Set default OpenGL format for SDR (will be overridden if HDR is requested and supported)
  QSurfaceFormat defaultFormat;
  defaultFormat.setProfile(QSurfaceFormat::CoreProfile);
  defaultFormat.setVersion(3, 3);
  defaultFormat.setSwapBehavior(QSurfaceFormat::DoubleBuffer);
  defaultFormat.setSwapInterval(1);
  defaultFormat.setRedBufferSize(8);
  defaultFormat.setGreenBufferSize(8);
  defaultFormat.setBlueBufferSize(8);
  defaultFormat.setAlphaBufferSize(8);
  QSurfaceFormat::setDefaultFormat(defaultFormat);

  qRegisterMetaType<recacheIndicator>("recacheIndicator");
  
  // Set application identity before using QSettings
  QCoreApplication::setApplicationName("YUView");
  QCoreApplication::setOrganizationName("Institut für Nachrichtentechnik, RWTH Aachen University");
  QCoreApplication::setOrganizationDomain("ient.rwth-aachen.de");
  
  // Step 1: Read HDR preference from configuration (PRD Requirement 5.2)
  QSettings settings;
  qDebug() << "YUView: Reading settings from organization:" << settings.organizationName();
  qDebug() << "YUView: Reading settings from application:" << settings.applicationName();
  qDebug() << "YUView: Settings file path:" << settings.fileName();
  
  bool userWantsHDR = settings.value("Enable10BitDisplay", false).toBool();
  
  bool hdrModeEnabled = false;
  bool hardwareFallbackOccurred = false;
  QString fallbackMessage;
  
  qDebug() << "YUView: User HDR preference from settings:" << userWantsHDR;
  
  if (userWantsHDR) {
    // Step 2: User wants HDR - perform hardware capability detection
    qDebug() << "YUView: User wants HDR - detecting hardware capabilities...";
    
    auto hdrDetection = HDRDetection::instance();
    auto hdrCapabilities = hdrDetection->detectHDRCapabilities();
    
    if (hdrCapabilities.isHDRSupported) {
      // Step 3a: Hardware supports HDR - initialize HDR pipeline (PRD Requirement 5.2)
      qDebug() << "HDR display detected:" << hdrCapabilities.displayName;
      qDebug() << "HDR mode:" << HDRDetection::getHDRModeDescription(hdrCapabilities.supportedMode);
      qDebug() << "Max luminance:" << hdrCapabilities.maxLuminance << "nits";
      qDebug() << "Bits per channel:" << hdrCapabilities.bitsPerChannel;
      
      // Configure HDR surface format based on detected capabilities
      QSurfaceFormat hdrFormat = defaultFormat;
      
      if (hdrCapabilities.supportedMode == HDRDetection::BT2020_PQ_10bit) {
        // Configure for BT.2020 PQ (10-bit per channel)
        hdrFormat.setRedBufferSize(10);
        hdrFormat.setGreenBufferSize(10);
        hdrFormat.setBlueBufferSize(10);
        hdrFormat.setAlphaBufferSize(2);
        qDebug() << "Configured 10-bit buffer sizes for BT.2020 PQ HDR rendering";
        
      } else if (hdrCapabilities.supportedMode == HDRDetection::BT709_G10_16bit) {
        // Configure for scRGB/Rec.709 Linear (16-bit per channel) 
        hdrFormat.setRedBufferSize(16);
        hdrFormat.setGreenBufferSize(16);
        hdrFormat.setBlueBufferSize(16);
        hdrFormat.setAlphaBufferSize(16);
        qDebug() << "Configured 16-bit buffer sizes for scRGB HDR rendering";
      }
      
      // Apply the HDR surface format as the new default
      QSurfaceFormat::setDefaultFormat(hdrFormat);
      qDebug() << "Updated default surface format for HDR rendering";
      
      hdrModeEnabled = true;
      
    } else {
      // Step 3b: Hardware doesn't support HDR - intelligent fallback (PRD Requirement 5.5)
      qDebug() << "HDR not supported:" << hdrCapabilities.errorMessage;
      qDebug() << "Performing intelligent fallback to SDR mode";
      
      hardwareFallbackOccurred = true;
      fallbackMessage = QString("HDR 模式启用失败：当前显示器或系统配置不支持。已自动以标准模式启动。");
      
      // Auto-correct configuration for next startup (PRD Requirement 5.5)
      QSettings correctionSettings;
      correctionSettings.setValue("Enable10BitDisplay", false);
      correctionSettings.sync();
      qDebug() << "Auto-corrected Enable10BitDisplay setting to false for next startup";
      
      // Keep using default SDR format
      hdrModeEnabled = false;
    }
    
  } else {
    // Step 2b: User doesn't want HDR - use standard SDR pipeline (PRD Requirement 5.4)
    qDebug() << "YUView: User preference is SDR mode - using standard rendering";
    hdrModeEnabled = false;
  }
  
  // Create the main YUView application with HDR decision made
  YUViewApplication app(argc, argv, hdrModeEnabled, hardwareFallbackOccurred, fallbackMessage);

  return app.returnCode;
}
