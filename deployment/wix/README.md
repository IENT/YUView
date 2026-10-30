# Build the Windows installer

The installer uses the WiX Toolset 7 MSBuild SDK and requires the .NET SDK 6 or newer.
Build the Windows application into `deploy` first, then provide the matching x64 Visual C++
merge module at `deployment/wix/Microsoft_VC143_CRT_x64.msm`.

From the repository root, build the MSI with:

```powershell
dotnet build deployment/wix/YUView.wixproj --configuration Release
```

The deployment directory defaults to `deploy`, and the default MSI version is `2.14.1`.
Override either value when needed:

```powershell
dotnet build deployment/wix/YUView.wixproj --configuration Release `
  -p:PayloadDir="C:\path\to\deploy" -p:ProductVersion=2.14.1
```

The output is `deployment/wix/bin/Release/YUViewSetup.msi`.