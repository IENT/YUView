git config --global credential.helper store
powershell Add-Content "$HOME\.git-credentials" "https://$($env:access_token):x-oauth-basic@github.com`n"
git config --global user.email "blaeser@ient.rwth-aachen.de"
git config --global user.name "Appveyor"
git clone -b master --single-branch https://github.com/IENT/YUViewReleases.git --depth 1
cd YUViewReleases
git rm --ignore-unmatch win\autoupdate\*
git rm --ignore-unmatch win\installers\*
xcopy /E ..\deploy\* win\autoupdate\. /I/Y
xcopy ..\YUViewSetup.msi win\installers\ /Y
xcopy ..\YUView-Win.zip win\installers\ /Y
git add win\autoupdate\*
git add win\installers\*
git status
git commit -a --allow-empty -m "Appveyor build %APPVEYOR_BUILD_NUMBER%, %APPVEYOR_BUILD_VERSION% based on %APPVEYOR_REPO_COMMIT%"
git push
