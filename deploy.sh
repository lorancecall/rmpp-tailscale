#!/bin/sh
# Push build/ to the device (ssh alias rmpp) and restart xochitl. For development; users install the Vellum package.
# Never `systemctl stop xochitl`: stopping it powers the tablet off.
set -e
cd "$(dirname "$0")"
scp -q build/librmpp-tailscale.so build/rmpp-tailscale.rcc rmpp:/home/root/
ssh rmpp 'set -e
mount -o remount,rw /
mkdir -p /usr/lib/rmpp-tailscale /usr/lib/rmpp-preload.d
for f in librmpp-tailscale.so rmpp-tailscale.rcc; do cp /home/root/$f /usr/lib/rmpp-tailscale/$f.new; mv /usr/lib/rmpp-tailscale/$f.new /usr/lib/rmpp-tailscale/$f; rm /home/root/$f; done
ln -sf /usr/lib/rmpp-tailscale/librmpp-tailscale.so /usr/lib/rmpp-preload.d/50-rmpp-tailscale.so
sync
systemctl reset-failed xochitl || true
systemctl restart xochitl
sleep 8
mount -o remount,ro / || echo "WARN: root still rw"
systemctl is-active xochitl'
