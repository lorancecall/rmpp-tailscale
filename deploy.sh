#!/bin/sh
# Development: replace the installed package's library and resources with build/ (ssh alias rmpp), restart xochitl.
# Install the package first (rmpp setup or `vellum add rmpp-tailscale`): it sets up the rest (units, env, preload link).
# Never `systemctl stop xochitl`: stopping it powers the tablet off.
set -e
cd "$(dirname "$0")"
scp -q build/librmpp-tailscale.so build/rmpp-tailscale.rcc rmpp:/home/root/
ssh rmpp 'set -e
test -d /usr/lib/rmpp/rmpp-tailscale || { echo "install the rmpp-tailscale package first"; exit 1; }
mount -o remount,rw /
for f in librmpp-tailscale.so rmpp-tailscale.rcc; do cp /home/root/$f /usr/lib/rmpp/rmpp-tailscale/$f.new; mv /usr/lib/rmpp/rmpp-tailscale/$f.new /usr/lib/rmpp/rmpp-tailscale/$f; rm /home/root/$f; done
sync
systemctl reset-failed xochitl || true
systemctl restart xochitl
sleep 8
mount -o remount,ro / || echo "WARN: root still rw"
systemctl is-active xochitl'
