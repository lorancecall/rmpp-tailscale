# Installing rmpp-tailscale

The usual way is **rmpp setup** from [rmpp-packages](https://github.com/lorancecall/rmpp-packages#install), as the
desktop app or the script: tick **Tailscale VPN and remote SSH**. This page covers the rest. Steps that are the same for every rmpp
add-on live in rmpp-packages and are linked here, not repeated.

## Packages

| Package | Installs with it |
|---|---|
| `rmpp-tailscale` | `rmpp-preload` |

Dependencies are installed automatically, in rmpp setup and with `vellum add`. All packages and what needs what:
[packages.md](https://github.com/lorancecall/rmpp-packages/blob/main/docs/packages.md).

## Without rmpp setup

Follow [manual-install.md](https://github.com/lorancecall/rmpp-packages/blob/main/docs/manual-install.md) (Vellum and the rmpp package repository), then on the Paper Pro:

```sh
vellum add rmpp-tailscale
```

## Uninstall

- **Just this add-on:** untick it in rmpp setup and press **Remove unticked installed**, or `vellum del rmpp-tailscale`.
- **What stays:** the Tailscale login and state in `/home/root/.local/share/rmpp-tailscale` and the on/off choice in `/home/root/.config/rmpp-tailscale`. Delete it by hand to start fresh, or use step 3 of
  [uninstall.md](https://github.com/lorancecall/rmpp-packages/blob/main/docs/uninstall.md).
- **Everything rmpp, and before a factory reset:** [uninstall.md](https://github.com/lorancecall/rmpp-packages/blob/main/docs/uninstall.md).

## Next to other mods

`rmpp-tailscale` cannot be installed next to `tailscale` (they do the same): remove those first.

It runs next to xovi and other Vellum packages: [how-it-works.md](https://github.com/lorancecall/rmpp-packages/blob/main/docs/how-it-works.md#next-to-other-mods).

## Build and install by hand

For development. Cross-compiled with the Arm GNU aarch64 toolchain against Qt 6.10 headers and the Paper Pro's Qt
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

On the Paper Pro, never run `systemctl stop xochitl` (it powers the Paper Pro off); run `systemctl reset-failed xochitl`
before `systemctl restart xochitl` (repeated restarts otherwise hit systemd's limit). The root file system is
read-only: `mount -o remount,rw /` before changing files under `/usr/lib/rmpp`, `mount -o remount,ro /` after.
