#!/bin/bash
# usage: ri_dell_get.sh REMOTE_PATH LOCAL_DEST
#
# Pull one file off the Dell E6320 lane. REMOTE_PATH is an AROS path with a
# volume, e.g. "Vk4aros:RIAPP.LOG". Vk4aros is the DELL'S OWN stick -- the
# song library is at Vk4aros:ReIncarnation/songs/local/..., which is a
# different layout from the riqemu1 copy and has bitten this lane before.
#
# The protocol has NO ranged read, so this streams the whole file; the server
# truncates what it keeps to the last 24576 B and says so. Judge a verdict by
# the exit status of the submit, never by counting matching lines: a wedged
# guest produces zero matches and zero failures at the same time.
#
# See ri_dell_push.sh for why these live in the repo.
set -e
SPIKE="${SPIKE:-/home/miller/Work/projects/Vulkan4Aros/scripts/spike_server.py}"
SPOOL="${RI_DELL_SPOOL:-/tmp/spike_spool_laptop}"
REMOTE="$1"
DEST="$2"
test -n "$REMOTE" && test -n "$DEST" || { echo "usage: $0 REMOTE_PATH LOCAL_DEST" >&2; exit 2; }
mkdir -p "$(dirname "$DEST")"
python3 "$SPIKE" submit --spool "$SPOOL" --wait 240 --get "$REMOTE:$DEST"