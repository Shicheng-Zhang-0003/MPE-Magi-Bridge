# 472-MGB — S2→MPE display bridge (second kernel module ecosystem)

Standalone home of the bridge core. It spawns a live `s2tui`, configures an
S2 world through the shell S2 already has, pulls machine state via `sync`
(S2SAVE1 frames), and maps frames into an abstract sink (bodies + bonds in
display millimetres).

Developed here; copied into 475 (`v15S/src/ecosystem/mgb/`) for the MPE sink,
the MPI module wrapper, and the suite — exactly like MFS was. **The 475 copy
is the live one** and has moved ahead of this tree; see
[Status](#status) for the split.

## Status

The bridge core is complete and gated; the MPE-side integration is live in
475. Motion is deliberately not implemented yet.

### Core (here)
- `mgb_spawn/cmd/snapshot/close` own the child with handshake, prompt
  framing, timeouts, and fail-closed teardown (no half frames, ever).
- `mgb_frame_parse` reads S2SAVE1 strictly: bad magic, truncation,
  out-of-bounds bonds, trailing garbage, and non-physical parameters
  (dt<=0, cutoff<=0, temp<0, tau<=0, nu<0) are all rejected.
- `mgb_map` owns the **only** scale (`mm_per_a`, default 1.0: 1 Å → 1 unit —
  large on purpose; a 48 Å dish becomes 48 units). Display-only, labeled on
  every artifact, inverts exactly.
- Sinks: `file` (standalone verification + goldens) and `mgb_sink_mpe_stub.c`,
  which documents the copy-time contract (static bodies, stable ids,
  zero-force joint lines, no physics writes, health section,
  `deterministic=false`).
- **Physical validation**: the frame parser enforces hard physical
  constraints — timestep must be positive, cutoff must be positive,
  temperature must be non-negative (Kelvin), coupling time must be
  positive, collision frequency must be non-negative. A frame violating
  any of these is rejected, not repaired.

### Failure reporting (this tree and the 475 copy)
The bridge never reports a bare "it didn't work". Each rejection names
itself:

- **`mgb_spawn` returns a status plus the OS errno.** Previously every
  failure — empty path, missing file, missing exec bit, bad exec format,
  resource exhaustion, fork failure, allocation failure, and a child that
  ran but stayed silent — collapsed into one `NULL`, so callers had to
  *guess* between them. Use `mgb_spawn_reason (int *why_errno)`;
  `mgb_spawn_strerror` gives the text. All eight failure modes are now
  named: `mgb_spawn_e_no_path`, `mgb_spawn_e_pipe`, `mgb_spawn_e_fork`,
  `mgb_spawn_e_notfound`, `mgb_spawn_e_noexec`, `mgb_spawn_e_noexecbit`,
  `mgb_spawn_e_handshake`, `mgb_spawn_e_alloc`.
- **`exec` failures carry the real errno.** The child writes errno through a
  `CLOEXEC` status pipe read with `poll()`. A non-blocking read races the
  child and turns every exec failure into a bogus "handshake timeout".
- **Path resolution is validated, never assumed.** `s2tui` is located from
  `$MGB_S2TUI_BIN`, else by a bounded upward search from the module's own
  directory, else from the working directory. Every candidate is checked
  with `access(X_OK)` and normalised with `realpath`, so a hit is always an
  absolute, canonical, existing executable. A miss stays a miss.
- **Gated here, not only in the copy.** `make test` (39 checks) pins the
  parser and mapper contracts, the spawn-failure naming, and the physical
  validation: each mode must report its own status and errno,
  `mgb_spawn_strerror` must never collapse two causes onto one string, and
  non-physical parameters (dt<=0, cutoff<=0, temp<0, tau<=0, nu<0) must be
  rejected. These gates are POSIX-only, which is also the only platform this
  core builds for.

> Historical trap, kept as a gate: an earlier revision stripped a *fixed*
> number of path components, which assumed one exact install depth. Reached
> through a symlinked working directory it produced
> `<root>/4179-MPE/../4179-Magi/.../s2tui`, which resolves only while that
> symlink exists — otherwise the kernel returns `ENOENT` on the symlink
> before it can even apply `..`, and the operator is shown "no such file"
> for a path that looks entirely plausible. **The plausible-looking path was
> the bug.** The gate now pins: a hit is an existing executable, is
> absolute, is canonical, and contains no unresolved `..`.

### 475 copy — what it adds
- `src/mgb_sink_mpe.c`: the real MPE sink. Static spheres, zero-force joints.
- `modules/s2/s2_bridge.c`: the MPI tick module, registry name `s2-bridge`.
- **Staging placement.** The renderer frustum-culls *silently*, so a
  successful attach can still place every body off-screen. Two rules, both
  gated:
  - the stage offset must be **on the default camera axis** — default
    `(0,20,-60)`; the camera starts at `(0,20,50)` looking down −Z through a
    45° vertical FOV, so anything more than 22.5° off that axis is dropped;
  - the offset targets the frame's **bounding-box centre**, not raw S2
    coordinates. Adding it to raw coordinates only means anything for a scene
    sitting at the origin, and the petri dish does not — with the offset
    added raw, only 2 of the 8 bounding-box corners were on screen.

  Attach logs `STAGED_BOUNDS` with the real centre, extent and radius range.
  Read it first when the screen is empty. Override with `$MGB_OX`/`$MGB_OY`/
  `$MGB_OZ`.

## Layout

```
472-MGB/
  readme.md            this file
  PROTOCOL.md          wire protocol (handshake/commands/frame grammar)
  makefile             all / probe / test / clean (C11, libm only)
  include/mgb.h        public API
  src/                 link, frame, map, file sink, MPE stub
  tools/mgb_probe.c    spawn+configure+snapshot+map exerciser
  tests/test_mgb.c     parser strictness + mapper policy gates
  scenes/petri.setup   configure transcript (documentation)
```

## Build, probe, test

```bash
make              # bridge lib + probe + tests
make test         # parser + mapper + spawn-naming gates -> "mgb: PASS 33 FAIL 0"
make probe        # live petri dish through ../s2tui -> probe_petri.mgb
```

`make probe` needs the `s2tui` binary one level up (build it from the S2
tree root first). It never touches the batch record (`./carbonsim`,
`output.txt`): the bridge only drives the interactive shell.

The 475 copy carries the MPE-side gates too (`v15S/src/ecosystem/mgb`,
`make test` → `mgb-mpe: PASS 43 FAIL 0`): spawn naming, path resolution, and
the static/zero-force staging contract.

## Roadmap

- **Phase 0 (done):** static frames on demand; file-sink goldens; named
  failures throughout.
- **Phase 1 (open):** follow mode — the dish renders but does not move or
  vibrate. Needs a move-don't-re-add refresh path (the current staging
  *adds* bodies, so a naive cadence would duplicate the dish every tick), a
  joint-graph refresh, and curated scenes (KcsA filter first).
- **Phase 2:** only if Phase 1 demands it — a `dump` verb on the s2tui side.
- **Never (until renamed):** physics coupling. Positions flow one way.

Tracked in `475-MPE/v15S/REMAINING_WORK.md`.