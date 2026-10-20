#!/bin/bash
# lane_proof.sh -- ONE COMMAND for the combined M4 + R5 + R6 + lamp proof on
# the Dell, so the owner's listening session is as short as possible.
#
# WHAT IT DOES, IN ORDER, AND WHY THAT ORDER:
#
#   1. BUILD every binary the proof needs, from the current tree, for THIS
#      lane's ABI. The two ABIs are NOT interchangeable -- riqemu1 is v1 and
#      the Dell is v11 -- and a v1 binary named RIAPP on the Dell has been the
#      wrong-binary trap three times.
#   2. HASH everything, and PRINT THE HASHES. Not to verify a golden: to
#      record exactly what was deployed, because "it behaved differently
#      yesterday" is unanswerable without knowing which bytes ran.
#   3. PUT to RAM:. Never `avail flush` -- it can unlink the very files
#      being read -- and never `quit` to a monitor, which kills the machine
#      the proof is running on.
#   4. RUN the proof script, which plays the M4 and R5 MIDISEND scripts in
#      sequence and leaves RAM:R6LISTEN.LOG and RAM:RIAPP-EV.LOG behind.
#   5. GET the logs back and print THE COUNTERS FIRST.
#
# WHY THE COUNTERS COME FIRST, AND BEFORE THE LISTENING IS EVEN SET UP:
# `NOTEIN played=` and `refused=` separate three faults that all present as
# "it is quiet":
#
#   played > 0, refused = 0  -> binding is right, fault is downstream
#   refused > 0              -> messages ARE arriving and the MAP is wrong
#   played = 0, refused = 0  -> nothing is arriving: a cluster or receiver
#                               fault, not a mapping fault
#
# None of those can be told apart by ear, and two of them mean no sound is
# coming regardless of how long anyone listens.
#
# USAGE
#   scripts/lane_proof.sh                 # deploy + run + report
#   scripts/lane_proof.sh --no-build      # reuse the binaries already in /tmp
#   scripts/lane_proof.sh --stage-only    # build, hash and PUT, then stop
#   scripts/lane_proof.sh --riqemu1       # the private lane (ABIv1) instead
#
# NO USB-MIDI DEVICE IS INVOLVED. MIDISEND plays a script into the CAMD
# cluster on the SAME MACHINE, so this is CAMD into CAMD. An earlier claim
# that R5 was unhearable for want of a device was wrong and is corrected in
# the ledger.
set -u

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
SPIKE=/home/miller/Work/projects/Vulkan4Aros/scripts/spike_server.py
# ONE build root, shared with the other scripts (see scripts/ri_build_host.sh).
RI_TMP="${RI_TMP:-/tmp/ri}"
export RI_TMP
OUT="$RI_TMP/laneproof"
RUN=300

BUILD=1
STAGE_ONLY=0
LANE=dell

for a in "$@"; do
    case "$a" in
        --no-build)   BUILD=0 ;;
        --stage-only) STAGE_ONLY=1 ;;
        --riqemu1)    LANE=riqemu1 ;;
        -h|--help)    sed -n '2,40p' "$0"; exit 0 ;;
        *) echo "lane_proof: unknown option $a (try --help)"; exit 2 ;;
    esac
done

# ---- the two lanes, and the ABI trap ------------------------------------
# Dell = agent e6320, spool /tmp/spike_spool_laptop, ABI **v11**.
# riqemu1 = spool /tmp/spike_spool_priv, ABI **v1**, and after ANY reset its
# screen MUST come back at 1280x1024 (GRUB is pinned there) -- verify with a
# monitor screendump, or every UI capture is the wrong size and silently so.
if [ "$LANE" = riqemu1 ]; then
    SPOOL=/tmp/spike_spool_priv
    ABI=v1
    RIAPP=RIAPP.V1
    MSEND=MIDISEND.V1
else
    SPOOL=/tmp/spike_spool_laptop
    ABI=v11
    RIAPP=RIAPP.M4
    MSEND=MIDISEND.V11
fi

if [ ! -f "$SPIKE" ]; then
    echo "lane_proof: $SPIKE not found -- the lane tools live in Vulkan4Aros"
    exit 2
fi
if [ ! -d "$SPOOL" ]; then
    # A missing spool is what an unresponsive lane looks like from here. Say so
    # in the lane's own terms rather than failing on a later put.
    echo "lane_proof: spool $SPOOL does not exist."
    echo "            The $LANE agent is not running. It needs an owner"
    echo "            reboot, and this script cannot do that."
    exit 3
fi

mkdir -p "$OUT"

# ---- 1. build -----------------------------------------------------------
# The bare-argument trap: `ri_build_aros.sh . out riapp` prints "STUB BUILD OK"
# and builds NOTHING; the correct invocation is the bare target name.
if [ "$BUILD" = 1 ]; then
    echo "== building for $ABI =="
    # THE ARGUMENT SHAPE, because it is the trap: v11 is
    # `ri_build_v11.sh <src> <out-riapp> [tool]` with the OUTPUT in $2 and
    # the tool in $3. My first version passed `<out> riapp`, which asks for a
    # RIAPP written to the file named `riapp`. A third argument that is not a
    # known tool refuses with exit 2 and writes nothing -- which is the
    # behaviour that makes this safe -- but $2 in the wrong place is not
    # caught by that, it just builds the wrong file.
    if [ "$ABI" = v11 ]; then
        bash "$ROOT/scripts/ri_build_v11.sh" "$ROOT" "$OUT/$RIAPP"        || exit 1
        bash "$ROOT/scripts/ri_build_v11.sh" "$ROOT" "$OUT/$MSEND" midisend || exit 1
    else
        # THE v1 LANE IS NOT THE SAME SCRIPT SHAPE. Two things, both measured
        # rather than assumed:
        #
        #   * `ri_build_aros.sh . out riapp` prints "STUB BUILD OK" and BUILDS
        #     NOTHING. The correct invocation is the BARE target name, and it
        #     writes to its own OUT ($RI_TMP/aros) rather than to a path you
        #     name -- so the copy below is not tidiness, it is the only way to
        #     get the file.
        #   * THERE IS NO `midisend` TARGET ON v1**, unlike v11. MIDISEND is
        #     built as a side effect of the `sections` branch, next to
        #     RISECT. Asking for a `midisend` target gets the RIAPP build and
        #     writes nothing at the expected name -- the same
        #     wrong-binary-under-the-right-name trap the v11 script had to be
        #     given a real target for.
        bash "$ROOT/scripts/ri_build_aros.sh" riapp    || exit 1
        bash "$ROOT/scripts/ri_build_aros.sh" sections || exit 1
        cp "$RI_TMP"/aros/RIAPP    "$OUT/$RIAPP" || exit 1
        cp "$RI_TMP"/aros/MIDISEND "$OUT/$MSEND" || exit 1
    fi
fi

# ---- 2. hash ------------------------------------------------------------
# Printed, not merely checked: the point is to record what ran.
echo "== what is being deployed ($ABI) =="
for f in "$RIAPP" "$MSEND"; do
    [ -f "$OUT/$f" ] || { echo "lane_proof: $OUT/$f missing -- did the build run?"; exit 1; }
    printf '  %-14s %8s bytes  sha256 %s\n' \
        "$f" "$(stat -c %s "$OUT/$f")" "$(sha256sum "$OUT/$f" | cut -c1-20)"
done
# ---- 2a. WRONG-BINARY GUARD, BEFORE ANY PUT ----------------------------
# "Printing sha256s does not catch a correctly built wrong program", and that
# is exactly right: a sha256 says which bytes ran, not what they are. Two
# checks, both of which have caught a real mistake on this lane:
#
#   * THE v1 r12 GATE. The v1 guest faults on the first LVO call if the ABI's
#     r12 base convention is used, so a v1 binary must contain NO
#     `mov %rax,%r12`. This is the v1 line's own gate, applied here to
#     everything about to be staged rather than trusting the build script to
#     have applied it.
#   * THE AROS BUILD LINE. `ri_build_aros.sh . out riapp` prints "STUB BUILD
#     OK" and BUILDS NOTHING, and the v1 script has no `midisend` target at
#     all -- MIDISEND rides the `sections` branch. So a missing or wrong-name
#     binary is a thing this script must refuse rather than deploy.
bad=0
for f in "$RIAPP" "$MSEND"; do
    p="$OUT/$f"
    if [ "$ABI" = v1 ]; then
        r12=$(objdump -d "$p" 2>/dev/null | grep -c 'mov *%rax,%r12')
        if [ "$r12" != "0" ]; then
            echo "  REFUSING $f: $r12 r12 base moves in a v1 binary"
            echo "    (the v1 guest faults on the first LVO call)"
            bad=1
        fi
    fi
    # AROS executables are ELF; a host object or a shell script under an AROS
    # name is the other shape this mistake takes.
    if ! head -c 4 "$p" | grep -q $'\x7fELF'; then
        echo "  REFUSING $f: not an ELF executable"
        bad=1
    fi
done
if [ "$bad" = 1 ]; then
    echo "lane_proof: refusing to PUT. A correctly BUILT WRONG PROGRAM is not"
    echo "            caught by a sha256 -- it has one, and it is correct."
    exit 4
fi
echo "  wrong-binary guard: both binaries are $ABI and r12-clean"

cp "$ROOT/docs/evidence/midi/m4-levi.mid.txt"  "$OUT/M4-LEVI.MID.TXT"
cp "$ROOT/docs/evidence/midi/r5-notein.mid.txt" "$OUT/R5-NOTEIN.MID.TXT"
cp "$ROOT/docs/evidence/midi/lane_proof.run"   "$OUT/LANE-PROOF.RUN"

# ---- 3. put -------------------------------------------------------------
# --run-script, NOT --exec: the lane's --exec is not a shell (a redirect
# inside it returns rc=0 and writes nothing) and a blocked command there
# wedges the agent's command path until an owner reboot.
echo "== putting to RAM: =="
python3 "$SPIKE" submit --spool "$SPOOL" --wait "$RUN" \
    --put "$OUT/$RIAPP:RAM:$RIAPP" \
    --put "$OUT/$MSEND:RAM:$MSEND" \
    --put "$OUT/M4-LEVI.MID.TXT:RAM:M4-LEVI.MID.TXT" \
    --put "$OUT/R5-NOTEIN.MID.TXT:RAM:R5-NOTEIN.MID.TXT" \
    --put "$OUT/LANE-PROOF.RUN:RAM:LANE-PROOF.RUN"
rc=$?
if [ $rc -ne 0 ]; then
    echo "lane_proof: the PUT did not complete (rc=$rc)."
    echo "            An empty spool with jobs left in it is what an"
    echo "            unresponsive agent looks like. This needs the owner."
    exit $rc
fi

if [ "$STAGE_ONLY" = 1 ]; then
    echo "== staged only; --stage-only was given, so nothing was run =="
    exit 0
fi

# ---- 4. run -------------------------------------------------------------
# The env vars the proof needs, set INSIDE the run script's shell, because
# RIAPP_NOTECH is deliberately UNSET -- that is what makes the melodic output
# follow the Leviasynth channel, and setting it would test the other branch.
echo "== running the proof (this is the owner's listening window) =="
python3 "$SPIKE" submit --spool "$SPOOL" --wait "$RUN" \
    --run-script RAM:LANE-PROOF.RUN

# ---- 5. get the logs and lead with the counters -------------------------
# ---- 4b. IS THE AGENT STILL ALIVE, AND DID SOMETHING CRASH? -------------
# THIS CHECK EXISTS BECAUSE IT ALREADY WEDGED ONCE (2026-10-12).
#
# A modal `Software Failure!` requester blocks the agent's command path
# completely: jobs queue and never complete, `--exec` and `--get` all time
# out, and the screen capture times out too (Intuition waits for a screen the
# requester is holding). That cost two ten-minute timeouts before the shape of
# it was obvious. So: a SHORT wait, and if the agent does not answer, say so
# and name the remedy rather than trying again.
echo "== is the agent still answering? =="
if python3 "$SPIKE" submit --spool "$SPOOL" --wait 20 \
        --ui-windows > "$OUT/windows.txt" 2>&1; then
    if grep -qi "software failure" "$OUT/windows.txt"; then
        echo "  *** A 'Software Failure!' REQUESTER IS UP ON THE LANE."
        echo "  *** CAPTURING IT NOW, at scale 0."
        echo
        grep -iE "software failure|RIAPP" "$OUT/windows.txt" | sed 's/^/    /'
        echo
        # SCALE 0, and not 1 or 2. The guest picks the smallest factor that
        # fits its frame cap, which is what a 1366x768 screen needs -- scale 1
        # was refused as too large and scale 2 timed out. This capture is the
        # whole reason this branch exists: on 2026-12 the alert text was lost
        # because the wedge was discovered too late to photograph, and the
        # diagnosis stalled on the one piece of evidence nobody had.
        # Best effort: a wedged agent may not answer it either, and that is
        # itself the answer -- it confirms the requester is what is blocking.
        python3 "$SPIKE" submit --spool "$SPOOL" --wait 45 \
            --ui-capture "$OUT/failure.png",0 > "$OUT/capture.txt" 2>&1
        if [ -s "$OUT/failure.png" ]; then
            echo "  captured: $OUT/failure.png"
        else
            echo "  capture failed (expected if the agent is wedged):"
            tail -n 2 "$OUT/capture.txt" | sed 's/^/    /'
        fi
        echo "  The next step after a reboot is to capture at scale 0 BEFORE"
        echo "  anything else runs, because the requester does not survive it."
        echo
    fi
else
    echo "  *** THE AGENT DID NOT ANSWER IN 20s."
    echo "  *** Almost always a modal requester wedging its command path."
    echo "  *** Move the queued jobs out of the spool so a later agent does not"
    echo "  *** replay them, then reboot:"
    echo "      mkdir -p $RI_TMP/moved_jobs"
    echo "      mv $SPOOL/jobs/*.json $RI_TMP/moved_jobs/"
    echo "  *** Nothing below can be concluded from this run. Stopping."
    exit 6
fi

echo "== bringing the logs back =="
# RIAPP.LOG as well as the ev-log: the binding lines and the counters are
# `evlog` (RIAPP-EV.LOG) and the audio/transport lines are `rlog`
# (RIAPP.LOG), and a proof that fetched only one of them would be unable to
# tell "the app never started" from "the app started but is silent".
python3 "$SPIKE" submit --spool "$SPOOL" --wait 120 \
    --get "RAM:RIAPP-EV.LOG:$OUT/RIAPP-EV.LOG" \
    --get "RAM:RIAPP.LOG:$OUT/RIAPP.LOG" \
    --get "RAM:R6LISTEN.LOG:$OUT/R6LISTEN.LOG"

echo
echo "==================== READ THIS FIRST ===================="
EV="$OUT/RIAPP-EV.LOG"
if [ ! -f "$EV" ]; then
    echo "--- RIAPP-EV.LOG: NOT RETRIEVED -- the app did not log, or the get"
    echo "    failed. Nothing below can be concluded from this run."
elif ! grep -q 'NOTEIN' "$EV"; then
    echo "--- FAIL: NO NOTEIN LINE IN THE EV-LOG."
    echo "    This is NOT silence. The app emits a zero baseline at startup"
    echo "    (R5f), so a missing NOTEIN line means one of two things and"
    echo "    neither is a quiet wire:"
    echo "      * the binary predates the counters  -> WRONG BINARY; check the"
    echo "        staged sha256 against what this run built"
    echo "      * MIDI in was never enabled, so the binding block that emits"
    echo "        the baseline never ran -> check RIAPP_MIDI_IN"
    exit 5
else
    echo "--- NOTEIN (the last line is the final state)"
    grep 'NOTEIN' "$EV" | tail -n 6
    echo
    LAST=$(grep 'NOTEIN played=' "$EV" | tail -n 1)
    get() { echo "$LAST" | tr ' ' '\n' | grep "^$1=" | cut -d= -f2; }
    SEEN=$(get seen); UNCL=$(get unclaimed); REFU=$(get refused)
    DROP=$(get dropped); PLAY=$(get played)
    : "${SEEN:=?}" "${UNCL:=?}" "${REFU:=?}" "${DROP:=?}" "${PLAY:=?}"
    echo "--- DIAGNOSIS (first matching row wins)"
    if   [ "$PLAY" != "?" ] && [ "$PLAY" -gt 0 ] 2>/dev/null; then
        echo "    played>0            notes are queued -- the fault is DOWNSTREAM"
    elif [ "$DROP" != "?" ] && [ "$DROP" -gt 0 ] 2>/dev/null; then
        echo "    dropped>0           QUEUE FULL -- P-19 territory, not a mapping fault"
    elif [ "$REFU" != "?" ] && [ "$REFU" -gt 0 ] 2>/dev/null; then
        echo "    refused>0           the MAP is wrong (bound channel, unknown note)"
    elif [ "$UNCL" != "?" ] && [ "$UNCL" -gt 0 ] 2>/dev/null; then
        echo "    unclaimed>0         the CHANNEL BINDING is wrong -- notes are"
        echo "                        arriving on a channel no machine claims"
    elif [ "$SEEN" = "0" ]; then
        echo "    seen=0              nothing arrived at all: CLUSTER or RECEIVER"
        echo "                        fault. Not a mapping fault."
    else
        echo "    no counter moved    messages arrived but none were notes"
    fi
fi
if [ -f "$OUT/R6LISTEN.LOG" ]; then
    echo
    echo "--- R6LISTEN.LOG (R6's outbound side)"
    tail -n 6 "$OUT/R6LISTEN.LOG"
fi
cat <<'EOF'

  A MISSING NOTEIN LINE IS A FAIL, NOT SILENCE (R5f). The app emits a zero
  baseline at startup precisely so that absence means one thing: a binary that
  predates the counters -- the wrong-binary trap, which has happened here
  three times -- or a run where MIDI in was never enabled.

  THE FIVE COUNTERS, AND WHAT EACH ONE RULES OUT. First row that matches wins:

    played>0     notes are queued; the fault is downstream
    dropped>0    the ring was full -- P-19, not a mapping fault
    refused>0    bound channel, unknown note: the MAP is wrong
    unclaimed>0  notes are arriving on a channel no machine claims: BINDING
    seen=0       nothing arrived at all: cluster or receiver fault

  `unclaimed` is the one that was missing and it is what separates a dead wire
  from a wrong rack. With only played/refused, `played=0 refused=0` meant
  BOTH "nothing arrived" and "plenty arrived, on the wrong channel".

  DEV out=1 ch=1 prog=38   one program change, on the Leviasynth channel
  R6LISTEN.LOG             channel-2 notes, channel-10 drums, ONE prog change

Then the owner's ears, in the order the script played them:
  A  M4 inbound   channel 2 -- notes, CC 55 / CC 74, bend, and the two
                 "must NOT" lines that must stay silent
  B  R5 note-in   channel 10 808 drums, channel 2 the 303, and the
                 unmapped-note refusal and unbound-channel silences
  C  R6 outbound  melodic notes following the Leviasynth channel, and the
                 clock-out lamp -- lit while bytes flow, dark after
EOF
