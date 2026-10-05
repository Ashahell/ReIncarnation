#!/bin/bash
# usage: ri_dell_push.sh LOCAL_FILE RAM_NAME
#
# Push one file to the Dell E6320 lane and start it as `Run RAM:<RAM_NAME>`.
#
# WHY THIS IS IN THE REPO, IN lane/ AND NOT scripts/ (2026-10-05). This pair of
# helpers lived only in /tmp/opencode and was destroyed by every host reboot --
# eight losses documented in the wiki across this lane. The repo rule recorded
# on 2026-09-27 is "git holds everything load-bearing, /tmp only regenerables",
# and these are load-bearing: without them there is no way to put a binary on
# the guest or read a log back.
#
# They are NOT in scripts/, because that directory is gated to hold exactly the
# five shared build/audit scripts (scripts/ri_audit.sh: `test "$(ls scripts |
# wc -l)" = 5`), and the gate is doing its job: these are lane plumbing for one
# machine, not shared build infrastructure. tools/ is also wrong -- it is gated
# against hard-coded Amiga paths (the `RAM:` in the launch form below would trip
# it), which is a real constraint and not one to route around. So: lane/.
#
# The two forms that actually work, and the one that does not:
#   Run RAM:<name>      works. This is the launch form.
#   Run <absolute path>  wedges the agent. Do not use it.
#
# The put goes through the bulk channel when the file is >= BULK_MIN
# (1_000_000 bytes), which every RIAPP binary is: 1014432 B is over the line.
# That is why the bridge needs its bulk port open -- see the note in
# ~/.config/systemd/user/spike-laptop.service.
set -e
SPIKE="${SPIKE:-/home/miller/Work/projects/Vulkan4Aros/scripts/spike_server.py}"
SPOOL="${RI_DELL_SPOOL:-/tmp/spike_spool_laptop}"
LOCAL="$1"
RAM="$2"
test -n "$LOCAL" && test -n "$RAM" || { echo "usage: $0 LOCAL_FILE RAM_NAME" >&2; exit 2; }
test -f "$LOCAL" || { echo "no such file: $LOCAL" >&2; exit 2; }
case "$RAM" in
  *:*) echo "RAM_NAME must be a bare name; the launcher supplies the volume" >&2; exit 2 ;;
esac
python3 "$SPIKE" submit --spool "$SPOOL" --wait 240 \
  --put "$LOCAL:RAM:$RAM" --exec "Run RAM:$RAM"