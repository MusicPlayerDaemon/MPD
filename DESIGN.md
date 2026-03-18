# Design System — mpd-secure-ctl

## Product Context

- **What this is:** A security management CLI for MPD-secure — the tool administrators
  use to rotate TLS certificates, manage plugin credentials, and inspect audit logs.
- **Who it's for:** Technical self-hosters running MPD-secure on home networks who care
  about explicit, verifiable security operations.
- **Space/industry:** Self-hosted media / home server security tooling. Peers: `vault`
  (HashiCorp), `step-cli`, `certbot`, `gpg`, `gh`.
- **Project type:** CLI tool (not a web app or TUI).

## Aesthetic Direction

- **Direction:** Security-explicit / audit-grade
- **Decoration level:** None — output serves a functional purpose only
- **Mood:** Every action produces a structured, parseable log line. The CLI output IS
  the audit trail. Operators should be able to pipe output to a log aggregator without
  any post-processing. No surprises, no ambiguity, no decoration.
- **Core principle:** If a line of output cannot be explained in terms of what the
  system did and whether it succeeded, it should not be printed.

## Output Format

The standard line format is:

```
[2026-03-18T14:22:01Z] LEVEL  message
```

- **Timestamp:** ISO 8601 UTC, always present, always in brackets.
- **Level label:** Fixed-width 5 characters, left-aligned:
  - `OK   ` — operation succeeded
  - `INFO ` — informational step (not an outcome)
  - `WARN ` — non-fatal condition requiring attention
  - `ERROR` — operation failed; exit code will be non-zero
- **Two spaces** between level and message — creates visual separation without a separator character.
- **Message:** Plain English, lowercase, no trailing period.

### Password / secret input

Sensitive values are always prompted interactively with echo disabled:

```
password: ****
```

Never read from positional arguments. Never echoed. Never logged.

### Structured data output

When outputting tables or status summaries, use aligned columns with a blank line before
and after:

```
component           status    detail
TLS certificate     OK        expires 2027-03-18
Qobuz credential    OK        stored in keychain
Shout password      WARN      not yet configured
```

Machine-readable output is available via `--json` on all commands. JSON output omits
timestamps and ANSI color.

## Color

- **Approach:** Status-only. Color encodes outcome, never decoration.
- **OK (green):** ANSI 32 — operation succeeded
- **WARN (yellow):** ANSI 33 — warning, non-fatal
- **ERROR (red):** ANSI 31 — operation failed
- **INFO (default):** No color applied — informational lines use the terminal default
- **Timestamps:** Dim (ANSI 2) — present but visually subordinate
- **`--no-color` flag:** First-class, not an afterthought. When set, no ANSI codes are
  emitted. Auto-detected when stdout is not a TTY (pipes, redirects, log files).
- **Dark mode:** Not applicable — color is status-semantic, not decorative.

## Typography

All output is monospace (terminal default). No font selection.

- **Label column:** 5 characters, fixed-width, left-aligned. Enables `grep OK` and
  `grep ERROR` to filter by outcome.
- **Indentation:** 2 spaces for continuation lines or sub-items under a log line.
- **No sentence case:** Messages are lowercase. This keeps output grep-friendly and
  avoids visual weight that suggests narrative prose.

## Spacing

- **Density:** Compact. No blank lines within a single command's output unless
  separating a header from data rows.
- **Between commands:** One blank line when multiple commands are chained in a script.
- **Separators:** No box-drawing characters, no horizontal rules. Blank lines are the
  only separator.

## Exit Codes

| Code | Meaning |
|------|---------|
| 0 | Success — all operations completed |
| 1 | Operational error — see ERROR line in output |
| 2 | Usage error — wrong arguments or flags |
| 3 | Authentication failure — credentials rejected |
| 4 | Daemon unreachable — cannot connect to mpd-secure |

Exit codes are documented in the man page. Scripts should check exit codes, not parse output.

## Command Naming Conventions

- **Verb-noun:** `cert rotate`, `credential set`, `credential list`, `log tail`
- **No abbreviations** in command names — full words only (`certificate` is `cert`,
  but `list` is not `ls`)
- **Subcommand groups:** One noun per domain (`cert`, `credential`, `log`, `config`,
  `status`)
- **Flags:** Long form preferred (`--no-color`, `--json`, `--force`). Short forms only
  for the most common flags (`-n`, `-q`, `-v`).
- **Destructive operations:** Require `--confirm` or an interactive confirmation
  prompt. Never destructive by default.

## Motion

- **Approach:** None. No spinners, progress bars, or animations.
- **Rationale:** Sequential log lines provide implicit progress. If an operation takes
  more than 2 seconds, it prints an `INFO` line explaining what it is waiting for.
  If it fails, the last printed line is the last step attempted — full traceability
  without animation state.

## Decisions Log

| Date | Decision | Rationale |
|------|----------|-----------|
| 2026-03-18 | Security-explicit / audit-grade aesthetic | Output must be usable as an audit trail without post-processing |
| 2026-03-18 | No spinners or animations | Sequential log lines are fully grep-parseable and failure-traceable |
| 2026-03-18 | `--no-color` as first-class flag | Color encodes status semantics; must degrade cleanly for log aggregators |
| 2026-03-18 | ISO 8601 UTC timestamps on every line | Timestamps must be sortable, timezone-unambiguous, and machine-parseable |
| 2026-03-18 | Exit codes 0-4 defined explicitly | Scripts should gate on exit codes, not output parsing |
| 2026-03-18 | Passwords never accepted as positional args | Prevents credentials appearing in shell history or process lists |
