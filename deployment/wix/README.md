# Build the Windows installer

The installer uses the WiX Toolset 7 MSBuild SDK and requires the .NET SDK 6 or newer.
Build the Windows application into `deploy` first. The MSI installs YUView; the Burn bundle
installs the Visual C++ Redistributable when a sufficiently recent x64 runtime is not already
present, then installs the MSI.

From the repository root, download Microsoft's current x64 redistributable and build both
packages:

```powershell
New-Item -ItemType Directory -Force deployment/wix/redist | Out-Null
Invoke-WebRequest -Uri https://aka.ms/vs/17/release/vc_redist.x64.exe `
  -OutFile deployment/wix/redist/vc_redist.x64.exe

dotnet build deployment/wix/YUView.wixproj --configuration Release `
  -p:PayloadDir="$PWD\deploy" -p:ProductVersion=2.14.1
dotnet build deployment/wix/bundle/YUViewBundle.wixproj --configuration Release `
  -p:ProductVersion=2.14.1
```

The standalone MSI is written to `deployment/wix/bin/Release/YUViewSetup.msi`. The recommended
installer users should run is `deployment/wix/bin/Bundle/Release/YUViewSetup.exe`.

The bundle checks the 64-bit Visual C++ runtime registry version and requires at least
`14.30.30704.0`, the initial Visual Studio 2022 runtime baseline. Keep that threshold aligned
with the minimum runtime required by the Windows build. The bundle's UpgradeCode must remain
stable across releases.
