# rmpp-tailscale

<img src="docs/rmpp-tailscale.gif" alt="rmpp-tailscale demo" width="405">

*Settings › Tailscale: on/off, this Paper Pro's address, SSH and your tailnet's devices (example data).*

[Tailscale](https://tailscale.com) for the reMarkable Paper Pro, with a **Settings > Tailscale** page: log in by
scanning a QR code, see your tailnet, and reach the Paper Pro over SSH from anywhere. Runs without xovi and keeps
working after a reboot.

Made for and tested on the reMarkable Paper Pro with OS 3.29. Other reMarkable models are not supported.
Updates for newer OS versions may take some time to follow.

> Part of the **rmpp add-ons**, built on [rmpp-packages](https://github.com/lorancecall/rmpp-packages): it is
> installed by rmpp setup, loaded into xochitl at every boot by rmpp-preload (no xovi), and kept up to date with
> Vellum. How that differs from xovi: [how it works](https://github.com/lorancecall/rmpp-packages/blob/main/docs/how-it-works.md).

## Install

Start at **[rmpp-packages](https://github.com/lorancecall/rmpp-packages#install)**: it explains developer mode and
has **rmpp setup**, the installer for all rmpp add-ons.

- In rmpp setup (the desktop app or the script), tick **Tailscale VPN and remote SSH** (rmpp-tailscale; not selected by
  default), then open Settings > Tailscale, turn it on and scan the QR code.
- It installs `rmpp-preload` with it ([all packages and dependencies](https://github.com/lorancecall/rmpp-packages/blob/main/docs/packages.md)).
- After a reMarkable OS update: **Restore** in rmpp setup.
- Installing without rmpp setup, uninstalling, using it next to other mods, and building: [docs/install.md](docs/install.md).

## Use

Settings > **Tailscale**:

- turn Tailscale on and off (stays that way across reboots)
- log in from the Paper Pro: the page shows the login link as a **QR code** to scan with your phone
- status: device name, Tailscale IPs, account and tailnet, the other devices on your tailnet
- **SSH over Tailscale**: `ssh root@<Paper Pro's Tailscale IP or MagicDNS name>` from anywhere on your tailnet
- check for and install Tailscale updates (`tailscale update`, signed packages from pkgs.tailscale.com)
- log out

The Tailscale binaries take about 70 MB in /home.

## How it works

- The Paper Pro kernel has no TUN driver, so `tailscaled` runs with `--tun=userspace-networking`.
  Other devices can reach the Paper Pro; programs on the Paper Pro reach the tailnet through the SOCKS5 proxy
  at `localhost:1055`.
- `rmpp-tailscaled.service` runs the official static arm64 binaries from `/home/root/.vellum/share/rmpp-tailscale`
  (the root file system is too small and read-only). State is in `/home/root/.local/share/rmpp-tailscale`.
  It only starts when `/home/root/.config/rmpp-tailscale/enabled` exists (the Settings toggle).
- reMarkable's SSH sockets listen on `wlan0`/`usb0` only; userspace Tailscale hands incoming connections
  to loopback, so `rmpp-tailscale-ssh.socket` adds the same dropbear server on `lo` (developer mode only).
- `librmpp-tailscale.so` is loaded into xochitl by `rmpp-preload` and injects the Settings row and page
  (QML in `qml/`, no moc: QML talks to C++ through a `QQmlPropertyMap`). It drives the `tailscale` CLI.
- `tailscale update` replaces the binaries in place; upgrading the package puts the packaged version back.

## Build

Building and the development install: [docs/install.md](docs/install.md#build-and-install-by-hand).

## License

GPL-3.0-only. `third_party/qrcodegen` is Project Nayuki's QR Code generator (MIT).
Tailscale is a trademark of Tailscale Inc.; this project is not affiliated with Tailscale.
