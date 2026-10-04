# Host bench and the overload guard — verbatim, 2026-10-04

**Ingested:** 2026-10-04 into ReIncarnation `llm-wiki`
**Source:** host bench (`tests/unit/levi_bench.c`) + four Dell runs, ABIv11.
**Provenance:** verbatim tool output and log lines. Derived values show their
components.
**Recorded in:** [the songs library lives at songs/local, and the tab-cycle "cost of -O2" was the overload guard](../articles/2026-10-04-host-bench-and-the-overload-guard.md)

## 1. The song library, listed from the Dell

```
[exec] 'dir Vk4aros:ReIncarnation/songs' -> rc=0 (63 ms)
            local (dir)
            demo (dir)

[exec] 'dir Vk4aros:ReIncarnation/songs/local' -> rc=0 (58 ms)
            zombie-nation (dir)
            the-knife (dir)
         demos.rbpl                       knife.rbpl
         riapp-demo.rbng

[exec] 'dir Vk4aros:ReIncarnation/songs/local/zombie-nation' -> rc=0 (63 ms)
         zombie-nation.rbng
```

And on riqemu1, flat, no nesting:

```
[exec] 'dir SYS:Classes/ReIncarnation/Songs' -> rc=0 (9754 ms)
         zombie-nation.rbng
```

## 2. The three probe shapes, as logged on the Dell

```
RIAPP song Vk4aros:ReIncarnation/songs/zombie-nation.rbng: open failed (probe; requester suppressed)
RIAPP song Vk4aros:ReIncarnation/songs/local/zombie-nation.rbng: open failed (probe; requester suppressed)
RIAPP song Vk4aros:ReIncarnation/songs/local/zombie-nation/zombie-nation.rbng: 151 bars at 140 BPM
```

## 3. Host bench, -O2

```
levi voice-render host bench
  build        : optimised (-O2 or -O1; NDEBUG unset)
  block        : 64 samples, 400 blocks timed

=== held voices (marginal cost = the step between adjacent rows) ===
  voices |   ns/block |  us/block | us/sample | us/voice-sample
       0 |      972.9 |     0.973 |     0.0152 |              -
       1 |    20423.4 |    20.423 |     0.3191 |         0.3039
       2 |    40537.8 |    40.538 |     0.6334 |         0.3143
       3 |    60172.5 |    60.172 |     0.9402 |         0.3068
       4 |    79793.0 |    79.793 |     1.2468 |         0.3066
       5 |    99420.8 |    99.421 |     1.5535 |         0.3067
       6 |   119937.9 |   119.938 |     1.8740 |         0.3206
       7 |   139219.0 |   139.219 |     2.1753 |         0.3013
       8 |   159040.2 |   159.040 |     2.4850 |         0.3097

=== active operators (ONE voice held, so voice count is fixed) ===
  ops |   ns/block |  us/block | ns/op | what it prices
    8 |    20799.4 |    20.799 | 2599.9 | full stack (baseline)
    7 |    20820.2 |    20.820 | 2974.3 | skips the top ops
    6 |    20783.6 |    20.784 | 3463.9 | skips the top ops
    5 |    20782.9 |    20.783 | 4156.6 | skips the top ops
    4 |    20766.7 |    20.767 | 5191.7 | skips the top ops
    3 |    20771.7 |    20.772 | 6923.9 | skips the top ops
    2 |    20772.0 |    20.772 | 10386.0 | skips the top ops
    1 |    20779.8 |    20.780 | 20779.8 | one op sounding

=== morph banks ===
  morph |   ns/block |  us/block
      0 |    79729.8 |    79.730
     50 |    79938.4 |    79.938
    100 |    79847.9 |    79.848

  midpoint minus the mean of the two extremes: -149.5 ns/block (-0.2%)
  => a bank BOTH banks render but only ONE uses is already being computed and thrown away.

  signal check: 4 voices give peak 2.08351 (non-silent, so the
  numbers above are work, not an early-out)
PASS levi_bench
```

**The operator line is flat and that is the finding:** the operator skip already
exists in `voice_pass`:

```c
        if (i >= RI_LEVI_NOPS || !live[i]) {
            opout[k & (RI_LEVI_NOPS - 1u)] = 0.0f;
            continue;
        }
```

**The morph line is flat**, so the "skip the unused bank" cut has no prize.

## 4. Host bench, -O0, same harness

```
  build        : UNOPTIMISED (-O0)
  voices |   ns/block |  us/block | us/sample | us/voice-sample
       0 |     1762.1 |     1.762 |     0.0275 |              -
       1 |    67187.0 |    67.187 |     1.0498 |         1.0223
       2 |   133006.9 |   133.007 |     2.0782 |         1.0284
       3 |   198372.7 |   198.373 |     3.0996 |         1.0213
       4 |   263081.7 |   263.082 |     4.1107 |         1.0111
       5 |   328061.3 |   328.061 |     5.1260 |         1.0153
       6 |   394535.1 |   394.535 |     6.1646 |         1.0387
       7 |   459891.0 |   459.891 |     7.1858 |         1.0212
       8 |   525323.6 |   525.324 |     8.2082 |         1.0224
```

## 5. Dell, -O2, paired counter regression

```
  vsamples |  n | mean vus | us/vsample | sounding
         0 | 13 |     10.0 |     0.0000 | idle
        64 |  4 |    130.5 |     2.0391 | 1.00
       128 |  4 |    278.0 |     2.1719 | 2.00
       256 | 14 |    513.0 |     2.0039 | 4.00

  fit: vus = 13.0 + 1.96987 * voice_active     (r = 0.9983)
    fixed overhead  :   13.0 us/block = 0.203 us/sample
    per voice-sample: 1.96987 us

  Dell -O0 / -O2 per voice-sample : 3.07x   (host: 3.33x)
  Dell -O2 / host -O2              : 6.42x
  Dell -O0 / host -O0              : 5.91x
```

(The `-O0` Dell fit is noisier, `r = 0.9117`, and its intercept is meaningless at
−300 µs; only its slope is used, and it is quoted as such.)

## 6. The overload guard, four runs

Heartbeat line shape:
`RIAPP hb: buffers=.. xruns=.. render_max=.. us wake_max=.. us wake_n=.. prio=.. arm_us=.. load=../1000 overloads=..`

| run | build | governor | last rows |
|---|---|---|---|
| FIX5 | `-O0` | on | `xruns 5420`, `render_max 20405`, `overloads 37`, `load 3/1000` |
| O2A | `-O2` | on | `xruns 0`, `render_max 4095`, `overloads 0`, `load 696/1000` |
| O2B | `-O2` | NOGOVERNOR | `xruns 0`, `render_max 4129`, `overloads 0`, `prio 21` |
| O0NG | `-O0` | NOGOVERNOR | `xruns 2505`, `render_max 8446`, `overloads 17`, `arm_us 1487907` |

`prio` distribution — this is the whole finding:

```
-O0 guard ON  (earlier) prio seen: {'21': 117, '4294967295': 7}     <- 4294967295 = -1
-O0 NOGOVERNOR              : {'21': 7}
-O2 guard ON                : {'21': 18}
```

And the 1.36 s stall in the `-O0` NOGOVERNOR run:

```
  draw: full_max/full_avg/n  part_max/part_avg/n
    ('0', '0', '0', '1364359', '74648', '25')
    ('0', '0', '0', '204', '202', '8')
```

## 7. Build identities

```
AROS RIAPP v11 BUILD OK (/tmp/opencode/RIAPP-O2.v11, 884176 bytes, build=f86b52d, r12moves=291)
AROS RIAPP v11 BUILD OK (/tmp/opencode/RIAPP-O0.v11, 1104032 bytes, build=3c826bb, r12moves=41)
```

## 8. Close and cleanup, verified

```
[ui  ] close '#0' [closerequest] ok (26 ms)
       Process 8 Loaded as command: RAM:RIPP-O2A      -> after close, count 0

[exec] 'Unsetenv RIAPP_AUDIO_NOGOVERNOR' -> rc=0 (11787 ms)
[exec] 'Getenv RIAPP_AUDIO_NOGOVERNOR' -> rc=5 (1650 ms)
       Getenv: object not found
[ui  ] close '#0' [closerequest] ok (28 ms)
```

`rc=5 object not found` on the follow-up `Getenv` is the unset taking effect.