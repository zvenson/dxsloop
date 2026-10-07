#!/bin/sh
# pull the site and let nginx see it (the docs/ folder is a bind mount: nothing to restart)
set -e
cd "$(dirname "$0")/repo" && git pull -q --ff-only origin main && echo "site: $(git log --oneline -1)"
cd ../zp12repo && git pull -q --ff-only origin main && echo "zp12: $(git log --oneline -1)"
