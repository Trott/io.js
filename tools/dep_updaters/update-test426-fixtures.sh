#!/bin/sh

set -e

TARGET_DIR="$(dirname "$0")/../../test/fixtures/test426"
README="$(dirname "$0")/../../test/test426/README.md"
TARBALL_URL=$(curl -fsIo /dev/null -w '%header{Location}' https://github.com/tc39/source-map-tests/archive/HEAD.tar.gz)
SHA=$(basename "$TARBALL_URL")

TMP_DIR="$(mktemp -d)"
curl -f "$TARBALL_URL" | tar -xzf - -C "$TMP_DIR"

rsync -a --delete "$TMP_DIR"/source-map-tests-"$SHA"/ "$TARGET_DIR"/

rm -rf "$TMP_DIR"

sed -i.bak "s#https://github.com/tc39/source-map-tests/commit/[0-9a-f]*#https://github.com/tc39/source-map-tests/commit/$SHA#" "$README"
rm "$README.bak"

echo "test426 fixtures updated to $SHA."
