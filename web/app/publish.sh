#!/bin/sh
# SPDX-License-Identifier: MPL-2.0
# Copyright (C) 2026 Charles Vestal
# Publish the web app at charles.pizza/amb: build it, copy app/build/app into the site's repo (a sibling
# checkout, ../charles.pizza from fm1-parent, or $SITE), commit and push (the site deploys on push).
#   web/app/publish.sh ["message"]
set -e
cd "$(dirname "$0")/../.."
SITE="${SITE:-$(cd ../.. && pwd)/charles.pizza}"
[ -d "$SITE/.git" ] || { echo "no site checkout at $SITE (set SITE)"; exit 1; }
sh web/app/build.sh
node tests/host/app_test.mjs app/build/app/amb.wasm
git -C "$SITE" pull -q --ff-only
mkdir -p "$SITE/amb"
cp app/build/app/index.html app/build/app/worklet.js app/build/app/amb.wasm "$SITE/amb/"
SHA=$(git rev-parse --short HEAD)
git -C "$SITE" add amb
if git -C "$SITE" diff --cached --quiet; then echo "site: amb unchanged"; exit 0; fi
git -C "$SITE" commit -q -m "${1:-AMB-1 web app} (fm1-amb $SHA)"
git -C "$SITE" push -q
echo "published: https://charles.pizza/amb/ (fm1-amb $SHA)"
