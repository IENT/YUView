#!/usr/bin/env bash

set -e

# Upgrade APT:
export DEBIAN_FRONTEND=noninteractive
apt-get update  -yq
apt-get install -yq apt-utils
apt-get update  -yq
apt-get dist-upgrade -yq

apt-get install -yq build-essential \
                    git \
                    qt5-default
