#!/bin/bash

set -e

# Upgrade APT:
export DEBIAN_FRONTEND=noninteractive
apt-get update  -yq
apt-get install -yq apt-utils
apt-get update  -yq
apt-get dist-upgrade -yq

apt-get install -yq build-essential git software-properties-common

if [ "$LINUX_SHORT_NAME" == "ubuntu16.04" ]
then
  add-apt-repository ppa:beineri/opt-qt-5.12.3-xenial -y
  apt-get update -qq
  apt-get install -qq qt512base qt512charts-no-lgpl
else
  apt-get install -yq qt5-default \
                      libqt5charts5 \
                      libqt5charts5-dev
fi
