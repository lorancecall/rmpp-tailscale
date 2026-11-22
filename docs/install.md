# Installing rmpp-tailscale

The usual way is **rmpp setup** from [rmpp-packages](https://github.com/lorancecall/rmpp-packages#install): it sets
up developer-mode SSH, Vellum and this repository's package, and **Restore** puts the add-ons back after a
reMarkable OS update. This page is for everything else.

## With Vellum on the command line

On the tablet, once (adds the rmpp package repository and its signing key to Vellum):

```sh
wget -qO- https://lorancecall.github.io/rmpp-packages/install.sh | sh
```

Then:

```sh
vellum add rmpp-tailscale      # install; xochitl restarts once
vellum upgrade         # later: updates
vellum reenable        # after a reMarkable OS update
vellum del rmpp-tailscale      # remove
```

The Tailscale binaries (~70 MB) live in `/home/root/.vellum/share/rmpp-tailscale`, its state in
`/home/root/.local/share/rmpp-tailscale` (kept when the package is removed).

To remove **everything** the rmpp add-ons put on the tablet, use **Remove everything** in rmpp setup.

## Build and install by hand

For development. Cross-compiled with the Arm GNU aarch64 toolchain against Qt 6.10 headers and the tablet's Qt
libraries (`SDK=../sdk` by default: `bin/aarch64-none-linux-gnu-g++`, `qt/usr/include/qt6`, `devlibs/`):

```sh
./build.sh     # -> build/ (`librmpp-tailscale.so`, `rmpp-tailscale.rcc`)
```

Install the package first (above, or rmpp setup): it sets up everything around the library (preload link, environment, boot script, `tailscaled` units and binaries).
Then replace the library with your build:

```sh
./deploy.sh    # ssh alias rmpp: copies build/ to /usr/lib/rmpp/rmpp-tailscale/, restarts xochitl
```

The package recipe (VELBUILD) is in
[rmpp-packages/packages/rmpp-tailscale](https://github.com/lorancecall/rmpp-packages/tree/main/packages/rmpp-tailscale); the next
`vellum upgrade` or reinstall puts the released files back.

On the tablet, never run `systemctl stop xochitl` (it powers the tablet off); run `systemctl reset-failed xochitl`
before `systemctl restart xochitl` (repeated restarts otherwise hit systemd's limit). The root file system is
read-only: `mount -o remount,rw /` before changing files under `/usr/lib/rmpp`, `mount -o remount,ro /` after.
