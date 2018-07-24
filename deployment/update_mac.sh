#!/bin/sh

update_mac() {
  git config --global user.email "travis@travis-ci.org"
  git config --global user.name "Travis CI"
  git clone -b deploy_mac --single-branch https://${GH_TOKEN}@github.com/IENT/YUViewReleases.git --depth 1
  cd YUViewReleases
  cp ../YUView-MacOs.zip .
  git add . *.zip
  git commit --message "Travis build: $TRAVIS_BUILD_NUMBER"
  git push --quiet 
}

update_mac
