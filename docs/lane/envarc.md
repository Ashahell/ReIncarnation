# ENVARC reference — RIAPP

Every variable below is read once at startup. Unset means "the default shown",
so a run with an empty ENVARC is the documented configuration, not an accident.

| variable | default | effect |
|---|---|---|
| `RIAPP_LOG` | first mounted USB/stick volume, else `RAM:` | where `RIAPP.LOG` goes. **Set this to a volume a reboot cannot reach** — on `RAM:` the log died with the first reboot and 346925 B of evidence had to be reconstructed from a transcript. |
| `RIAPP_EVLOG` | same rule as `RIAPP_LOG` | where `RIAPP-EV.LOG` goes; truncated per run. Set it to `RAM:` for a throwaway diagnostic run — otherwise the app **blocks on a requester asking for the log name and never shows a window**. |
| `RIAPP_DIAG` | **off** | `1` enables the per-phase draw timing (six `ReadEClock` calls per repaint, ~13 µs on the Dell). Off by default so a release build pays nothing. When off, the `gap_avg` field reads 0 and that means **not measured**; the `draw:` line carries `diag=` so the two are never confused. |
| `RIAPP_AUDIO_PRI` | `AU_LIVE_PRI` (21) | pins the render task's working priority. |
| `RIAPP_AUDIO_NOGOVERNOR` | unset | set to any non-zero value to disable the load governor entirely. The governor **yields the render task to pri −1, below the GUI**, which is why `-O0` repaint timings look cheap: at `-O0` a buffer overruns and the guard hands the GUI free CPU. |

## Reading a log

Both files begin with a line carrying `build=<short-sha>`, and the heartbeat's
`draw:` line carries `diag=`. **Check the hash before believing any number** —
a stale binary is otherwise indistinguishable from a regression.
