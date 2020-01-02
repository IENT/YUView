REM Get the windows runtime merge module
copy "C:\Program Files (x86)\Common Files\Merge Modules\Microsoft_VC140_CRT_x64.msm" .
REM Harvest all files in the deploy direcotry
C:\Users\travis\build\wix311\heat.exe dir "..\..\deploy" -gg -dr APPLICATIONFOLDER -srd -sreg -cg YUViewComponents -out harvestedDirectory.wxs
REM Compile the wxs file of the harvested files and the YUView.wxs file into objects
C:\Users\travis\build\wix311\candle.exe -dConfiguration=Release -dOutDir=bin\Release\ -dTargetExt=.msi -dTargetFileName=YUViewSetup.msi -dTargetName=YUViewSetup -out obj\Release\ -arch x64 -ext "C:\Program Files (x86)\WiX Toolset v3.11\bin\\WixUIExtension.dll" YUView.wxs
C:\Users\travis\build\wix311\candle.exe -dConfiguration=Release -dOutDir=bin\Release\ -dTargetExt=.msi -dTargetFileName=YUViewSetup.msi -dTargetName=YUViewSetup -out obj\Release\ -arch x64 harvestedDirectory.wxs
REM Link the object files into the actual msi installer
C:\Users\travis\build\wix311\Light.exe -b ..\..\deploy -out bin\Release\YUViewSetup.msi -pdbout bin\Release\YUViewSetup.wixpdb -cultures:null -ext "C:\Program Files (x86)\WiX Toolset v3.11\bin\\WixUIExtension.dll" -contentsfile obj\Release\YUViewSetup.wixproj.BindContentsFileListnull.txt -outputsfile obj\Release\YUViewSetup.wixproj.BindOutputsFileListnull.txt -builtoutputsfile obj\Release\YUViewSetup.wixproj.BindBuiltOutputsFileListnull.txt -wixprojectfile D:\Projekte\YUViewSetup\YUViewSetup.wixproj obj\Release\YUView.wixobj obj\Release\harvestedDirectory.wixobj
