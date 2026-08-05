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
#include <QColorSpace>
#include <QDebug>
#include <QSettings>
#include <QMessageBox>

#include <common/Typedef.h>
#include <ui/YUViewApplication.h>


int main(int argc, char *argv[])
{
  // ========================================
  // BASIC OPENGL SETUP - MUST BE FIRST
  // ========================================
  
  // Set basic OpenGL attributes before any application creation
  // Qt6 enables high-DPI scaling by default.
  QCoreApplication::setAttribute(Qt::AA_SynthesizeMouseForUnhandledTouchEvents,false);
  QCoreApplication::setAttribute(Qt::AA_SynthesizeTouchForUnhandledMouseEvents,false);

  // ========================================
  // STARTUP-BASED HDR DECISION LOGIC (PRD Requirement 5.1-5.5)
  // Simplified branch structure: HDR vs SDR path determined once at startup
  // ========================================

  qRegisterMetaType<recacheIndicator>("recacheIndicator");
  
  // Set application identity before using QSettings
  QCoreApplication::setApplicationName("YUView");
  QCoreApplication::setOrganizationName("Institut für Nachrichtentechnik, RWTH Aachen University");
  QCoreApplication::setOrganizationDomain("ient.rwth-aachen.de");
  
  // Read HDR preference from configuration (PRD Requirement 5.2)
  QSettings settings;
  const bool userWantsHDR = settings.value("Enable10BitDisplay", false).toBool();
  
  bool hdrModeEnabled = false;
  bool hardwareFallbackOccurred = false;
  QString fallbackMessage;
  
  
  // Simplified pure branch structure for HDR/SDR decision
  // When userWantsHDR is true: configure 10-bit OpenGL surface format for HDR rendering
  // When userWantsHDR is false: use Qt default format, rely on standard QPainter path
  // This eliminates the redundant 8-bit -> 10-bit reconfiguration pattern
  if (userWantsHDR) {
    // HDR PATH: Configure 10-bit OpenGL surface format with Qt 6.8+ native HDR color space
    // This format is required for HDR_VideoWindow to render 10-bit content correctly
    QSurfaceFormat hdrFormat;
    hdrFormat.setProfile(QSurfaceFormat::CoreProfile);
    hdrFormat.setVersion(3, 3);
    hdrFormat.setSwapBehavior(QSurfaceFormat::DoubleBuffer);
    hdrFormat.setSwapInterval(1);
    // HDR RGBA16F requires 16-bit per channel for linear light rendering
    hdrFormat.setRedBufferSize(16);
    hdrFormat.setGreenBufferSize(16);
    hdrFormat.setBlueBufferSize(16);
    hdrFormat.setAlphaBufferSize(16);
    
    // Qt 6.8+ Native HDR: Use extended sRGB linear color space for RGBA16F
    // The QRhi swap chain will be configured with HDRExtendedSrgbLinear format
    // which uses scRGB linear light (values > 1.0 represent HDR content)
    // PQ/HLG/Linear OETF will be applied in fragment shader (homework assignment)
#if QT_VERSION >= QT_VERSION_CHECK(6, 8, 0)
    hdrFormat.setColorSpace(QColorSpace(QColorSpace::SRgbLinear));
#else
#endif
    
    QSurfaceFormat::setDefaultFormat(hdrFormat);
    
    hdrModeEnabled = true;
  } else {
    // SDR PATH: Use Qt default format, no explicit QSurfaceFormat configuration needed
    // Standard QPainter rendering path handles 8-bit display automatically
    hdrModeEnabled = false;
  }
  
  // Create the main YUView application with HDR decision made
  
  YUViewApplication app(argc, argv, hdrModeEnabled, hardwareFallbackOccurred, fallbackMessage);

  return app.returnCode;
}
