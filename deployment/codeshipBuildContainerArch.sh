#!/usr/bin/env bash

set -e

pacman -Syu --noconfirm
pacman -S base-devel --noconfirm
pacman -S git --noconfirm
pacman -S qt5 --noconfirm
