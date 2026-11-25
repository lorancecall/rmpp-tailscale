# packaging

The Vellum package of this add-on: `rmpp-tailscale`.
[rmpp-packages](https://github.com/lorancecall/rmpp-packages) builds, signs and publishes it from here (its
`scripts/registry.py` finds this repository by the topic `rmpp`). After a release, update `pkgver` and the
sums in the VELBUILD on the default branch; the next build (daily, or
`gh workflow run publish.yml -R lorancecall/rmpp-packages`) publishes it. Format and rules:
[MODS.md](https://github.com/lorancecall/rmpp-packages/blob/main/MODS.md).

Tailscale itself (`_tsver`) is kept current by `.github/workflows/tailscale-bump.yml`: every week it runs
`packaging/bump-tailscale.sh` and, when pkgs.tailscale.com has a newer stable release, opens a PR that sets
`_tsver`, raises the patch version of `pkgver` (`pkgrel` stays 0) and updates the tarball's sum. Merging it creates
the release `v<pkgver>` automatically (`.github/workflows/release.yml`, with the previous release's files), and the
next build publishes it.
