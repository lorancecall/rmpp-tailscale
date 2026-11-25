#!/bin/sh
# Bump the packaged Tailscale (_tsver) in packaging/rmpp-tailscale/VELBUILD to the latest stable release.
# Checks the tarball against Tailscale's published SHA-256, then sets _tsver, bumps the patch number of pkgver
# (plain versions: pkgrel stays 0) and replaces the tarball's sha512sums line. Merging the bump to main makes
# .github/workflows/release.yml create release v<pkgver> with the previous release's files. Prints the new
# Tailscale version, or nothing when already up to date.
# Used by .github/workflows/tailscale-bump.yml; can also be run by hand from the repository root.
set -eu

f=packaging/rmpp-tailscale/VELBUILD
base=https://pkgs.tailscale.com/stable

latest=$(curl -fsS "$base/?mode=json" | sed -n 's/.*"TarballsVersion" *: *"\([0-9.]*\)".*/\1/p')
current=$(sed -n 's/^_tsver=//p' "$f")
[ -n "$latest" ] && [ -n "$current" ] || { echo "could not read versions" >&2; exit 1; }

# Newer only (never downgrade).
[ "$latest" != "$current" ] || exit 0
[ "$(printf '%s\n%s\n' "$current" "$latest" | sort -V | tail -n1)" = "$latest" ] || exit 0

tgz=tailscale_${latest}_arm64.tgz
tmp=$(mktemp -d)
trap 'rm -rf "$tmp"' EXIT
curl -fsSL -o "$tmp/$tgz" "$base/$tgz"
want=$(curl -fsS "$base/$tgz.sha256" | tr -d ' \n')
have=$(sha256sum "$tmp/$tgz" | cut -d' ' -f1)
[ "$want" = "$have" ] || { echo "SHA-256 mismatch for $tgz" >&2; exit 1; }
sum=$(sha512sum "$tmp/$tgz" | cut -d' ' -f1)

ver=$(sed -n 's/^pkgver=//p' "$f")
case $ver in
*.*.*) ;;
*) echo "pkgver $ver is not x.y.z" >&2; exit 1 ;;
esac
next=${ver%.*}.$((${ver##*.} + 1))
sed -i \
	-e "s/^_tsver=.*/_tsver=$latest/" \
	-e "s/^pkgver=.*/pkgver=$next/" \
	-e "s/^pkgrel=.*/pkgrel=0/" \
	-e "s/^[0-9a-f]\{128\}  tailscale_[0-9.]*_arm64\.tgz\$/$sum  $tgz/" \
	"$f"
grep -q "^$sum  $tgz\$" "$f" || { echo "sha512sums line not updated" >&2; exit 1; }
echo "$latest"
