#!/usr/bin/env bash
# Installs a compiler and CMake in a Linux distro container and writes
# suite=full (C++20 test suite) or suite=c (C API test only, for distros
# whose default compiler lacks C++20 library support) to $GITHUB_OUTPUT.
set -euo pipefail

. /etc/os-release
suite=full
case "$ID:${VERSION_ID:-}" in
  ubuntu:20.04 | debian:11)
    suite=c
    apt-get update -q
    DEBIAN_FRONTEND=noninteractive apt-get install -yq gcc
    ;;
  ubuntu:* | debian:*)
    apt-get update -q
    DEBIAN_FRONTEND=noninteractive apt-get install -yq g++ cmake make ca-certificates
    ;;
  fedora:*)
    dnf install -y gcc-c++ cmake make
    ;;
  rocky:8*)
    dnf install -y gcc-toolset-13-gcc-c++ cmake make
    echo "CC=/opt/rh/gcc-toolset-13/root/usr/bin/gcc" >> "$GITHUB_ENV"
    echo "CXX=/opt/rh/gcc-toolset-13/root/usr/bin/g++" >> "$GITHUB_ENV"
    ;;
  rocky:*)
    dnf install -y gcc-c++ cmake make
    ;;
  arch:*)
    pacman -Syu --noconfirm gcc cmake make
    ;;
  *)
    echo "unsupported distro: $ID ${VERSION_ID:-}" >&2
    exit 1
    ;;
esac

ldd --version | sed -n 1p
echo "suite=$suite" >> "$GITHUB_OUTPUT"
