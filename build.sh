#!/bin/sh
# Cross-build librmpp-tailscale.so and rmpp-tailscale.rcc. SDK defaults to ../sdk (Arm GNU aarch64 + Qt 6.10 headers + device libs).
set -e
cd "$(dirname "$0")"
SDK=${SDK:-../sdk}
CXX=${CXX:-$SDK/bin/aarch64-none-linux-gnu-g++}
Q=$SDK/qt/usr/include/qt6
mkdir -p build
"$CXX" -std=c++17 -O2 -fPIC -shared -DQT_NO_VERSION_TAGGING -DQT_NO_DEBUG \
  -I"$Q" -I"$Q/QtCore" -I"$Q/QtGui" -I"$Q/QtQml" -I"$Q/QtQuick" -Ithird_party/qrcodegen \
  src/rmppts.cpp third_party/qrcodegen/qrcodegen.cpp -o build/librmpp-tailscale.so -L"$SDK/devlibs" \
  -l:libQt6Core.so.6 -l:libQt6Gui.so.6 -l:libQt6Qml.so.6 -l:libQt6Quick.so.6 \
  -Wl,--allow-shlib-undefined -Wl,-soname,librmpp-tailscale.so
${RCC:-uvx --from pyside6-essentials==6.10.1 pyside6-rcc} --binary --no-compress rmpp-tailscale.qrc -o build/rmpp-tailscale.rcc
ls -la build
