# FEATURE-007: Produce Formal STRIDE Threat Model for MPD-secure

**GitHub Issue:** https://github.com/devinhedge/MPD-secure/issues/7
**EPIC:** #1 MPD-secure Zero-Trust Network Security
**Blocked By:** *(nothing — this is the starting point)*
**Status:** Planned

## What the User Experiences

A technical music enthusiast and contributor to MPD-secure can open a single
living document and understand exactly what the attack surface looks like, what
threats exist, which are currently unmitigated, and which feature is
responsible for mitigating each one. Security decisions across all other
features are traceable back to this document.

## Zero-Trust Alignment

Zero-trust without a threat model is security theater. This document is the
map every other security feature navigates by. It establishes the shared
vocabulary of adversaries, assets, and acceptable risk for the project.

## Scope

The threat model covers all components of MPD-secure:

- Network listener and client protocol
- Permission and authentication system
- Input plugins (including service credentials: Qobuz, SHOUTcast, proxy)
- Decoder and output plugins
- Credential storage (config file, in-memory)
- IPC (daemon fork, inter-thread communication)
- Filesystem access (playlists, music library, state file)

The document has two parts:

1. **Forward-looking STRIDE analysis** — what threats exist by design against
   each component, regardless of current implementation
2. **Current vulnerability catalog** — specific weaknesses found in the
   existing codebase that the STRIDE analysis predicts, documented with file
   and line references so FEATURE-002 (secure code practices) has a concrete
   remediation backlog

### Protected Assets

The following assets are in scope. Every threat is evaluated in terms of its
impact on one or more of these assets.

| Asset | Sensitivity | Authorized Accessors |
|---|---|---|
| Service account credentials (Qobuz, SHOUTcast) | Critical | Daemon process only |
| MPD client passwords (config) | High | Daemon process only |
| Music library filesystem paths | Medium | Authenticated MPD clients |
| Playlist files | Medium | Authenticated MPD clients (with ADD/CONTROL permission) |
| State file (playback state, queue) | Medium | Daemon process only |
| Daemon availability (playback) | Medium | Authorized clients |
| Network position (routing, lateral movement) | High | Not an asset to share |
| Config file contents | High | System administrator only |

### Severity Rubric

Severity ratings in the vulnerability catalog use the following definitions:

| Severity | Exploitability | Impact | Example |
|---|---|---|---|
| Critical | Remotely exploitable with no auth, no user interaction | Full confidentiality, integrity, or availability loss | Unauthenticated RCE via network |
| High | Remotely exploitable or requires minimal access | Significant data exposure, privilege escalation, or DoS | Credentials leaked to attacker on same LAN |
| Medium | Requires local access or specific conditions | Partial exposure, limited escalation, degraded availability | Path traversal limited to readable paths |
| Low | Requires privileged access or is difficult to exploit | Minimal impact, information leakage only | URL-encoding omission in internal request |

## User Stories

### US-007-00: Define Adversary Profiles
**As a** contributor to MPD-secure,
**I want** the threat actors documented as concrete adversary profiles,
**so that** likelihood ratings in the STRIDE analysis are anchored to realistic
attacker scenarios rather than abstract judgment calls.

**Acceptance Criteria:**

- At least 4 adversary profiles defined, each with: name, access level,
  motivation, and capability
- Profiles cover: network-adjacent attacker, compromised/malicious plugin,
  insider with config file access, and supply chain attacker
- Likelihood ratings in US-007-02 reference these profiles explicitly

**Initial adversary profiles:**

| Profile | Access | Motivation | Capability |
|---|---|---|---|
| Network-adjacent attacker | TCP port 6600 reachable (same LAN or misconfigured firewall) | Control playback, exfiltrate credentials, pivot to other LAN hosts | Can send arbitrary MPD protocol commands; cannot read memory or files directly |
| Compromised plugin | Runs inside daemon process at plugin init/stream time | Exfiltrate credentials, tamper with audio, persist via config | Access to daemon process memory, may call internal APIs |
| Insider (config access) | Read/write access to mpd.conf and daemon config directory | Extract service credentials, escalate to daemon permissions | Can modify config before daemon starts; may have shell access |
| Supply chain attacker | Controls a third-party library or upstream MPD commit | Introduce backdoor, exfiltrate credentials at scale | Can modify code compiled into the daemon binary |

---

### US-007-01: Map All Trust Boundaries
**As a** contributor to MPD-secure,
**I want** every trust boundary in the system documented,
**so that** I know exactly where untrusted input enters the system and what
validation or authentication is expected at each boundary.

**Acceptance Criteria:**

- All trust boundaries documented in a table with: location (file:line),
  what crosses the boundary, and current protection (if any)
- Boundaries include: TCP accept, Unix socket accept, command line input,
  file path arguments, plugin credential loading, external API calls,
  the daemonization fork pipe, and the state file
- Diagram shows trust zones and the boundaries between them

**Known boundaries from codebase analysis:**

| Boundary | Location | What Crosses It | Current Protection |
|---|---|---|---|
| TCP socket accept | `src/client/Listener.cxx:32` | Any TCP client | None |
| Unix socket accept | `src/client/Listener.cxx:32` | Local process | File permissions (0600 XDG socket) |
| Line input parsing | `src/client/Read.cxx:13` | Arbitrary bytes | Line framing, lowercase-first check |
| Command dispatch | `src/command/AllCommands.cxx:382` | Command + args | Permission bitmask check |
| Password command | `src/command/ClientCommands.cxx:45` | Client password string | Map lookup, no rate limiting |
| File path arguments | Various command handlers | Unvalidated string paths | None |
| Qobuz credential load | `src/input/plugins/QobuzInputPlugin.cxx:113` | API credentials from config | None |
| Qobuz login request | `src/input/plugins/QobuzLoginRequest.cxx:19` | Password in HTTPS query string | HTTPS transport only |
| Proxy DB password | `src/db/plugins/ProxyDatabasePlugin.cxx:79` | Remote MPD password | Cleartext unless external TLS |
| State file read (startup) | `src/StateFile.cxx` | Daemon state from disk | Filesystem permissions only |
| State file write (runtime) | `src/StateFile.cxx` | Daemon state to disk | Filesystem permissions only |

---

### US-007-02: Perform STRIDE Analysis on Each Component
**As a** contributor to MPD-secure,
**I want** a STRIDE analysis applied to every major component,
**so that** no threat category is overlooked and every threat is explicitly
accepted, mitigated, or deferred.

**Acceptance Criteria:**

- STRIDE analysis covers all six threat categories for each component:
  Spoofing, Tampering, Repudiation, Information Disclosure,
  Denial of Service, Elevation of Privilege
- Each threat has: component, threat description, likelihood (H/M/L),
  impact (H/M/L), adversary profile reference, current status
  (Unmitigated / Partially mitigated / Mitigated), and mitigating feature
- No component is omitted

**Components to analyze:**

- Network listener (`src/Listen.cxx`, `src/client/Listener.cxx`)
- Client protocol parser (`src/client/Read.cxx`, `src/client/Process.cxx`)
- Permission and authentication system (`src/Permission.cxx`)
- Input plugin system (`src/input/`)
- Decoder plugin system (`src/decoder/`)
- Output plugin system (`src/output/`)
- Credential storage (in-memory, config file)
- Filesystem access (playlist, library, state file)
- Daemonization and process lifecycle (`src/unix/Daemon.cxx`)

---

### US-007-03: Catalog Current Known Vulnerabilities
**As a** contributor to MPD-secure,
**I want** a catalog of specific vulnerabilities already present in the
codebase,
**so that** FEATURE-002 (secure code practices) has a concrete, traceable
remediation backlog grounded in the STRIDE analysis.

**Acceptance Criteria:**

- Each vulnerability entry includes: STRIDE category, severity (per rubric
  above), affected file and line, description, and linked mitigating feature
- All vulnerabilities discovered during codebase analysis are included
- Catalog is structured so entries can be checked off as FEATURE-002
  remediates them

**Known vulnerabilities from codebase analysis:**

| # | STRIDE | Severity | Location | Description |
|---|---|---|---|---|
| V-001 | Spoofing | High | `Permission.cxx:36` | Client passwords stored in plaintext in memory; no constant-time comparison; no rate limiting on `password` command |
| V-002 | Information Disclosure | High | `QobuzLoginRequest.cxx:19-36` | Qobuz password sent as plaintext query parameter in HTTPS URL; appears in server logs, proxy histories, and curl debug output |
| V-003 | Tampering | Medium | `QobuzClient.cxx:192` | MD5 used for Qobuz request signing (`request_sig`); MD5 is cryptographically broken and trivially forgeable |
| V-004 | Information Disclosure | High | `QobuzInputPlugin.cxx:113` | Qobuz `app_id`, `app_secret`, `username`, and `password` held as raw `const char *` for daemon lifetime; never zeroed; may appear in core dumps |
| V-005 | Information Disclosure | High | `ShoutOutputPlugin.cxx:22-46` | Icecast/SHOUTcast password held as `const char *` for daemon lifetime; never zeroed |
| V-006 | Information Disclosure | High | `ProxyDatabasePlugin.cxx:79` | Remote MPD password sent over cleartext TCP via MPD protocol unless external TLS is configured separately |
| V-007 | Spoofing | Critical | `Permission.cxx:76` | Default permission when no `password` config exists is full admin (READ\|ADD\|PLAYER\|CONTROL\|ADMIN); any TCP client gets admin |
| V-008 | Elevation of Privilege | Medium | `client/Listener.cxx:15` | Peer UID available via `SO_PEERCRED` on Unix sockets but explicitly unused (`(void)cred; // TODO`); UID-based authorization not implemented |
| V-009 | Denial of Service | Medium | `ClientCommands.cxx:45` | No rate limiting or lockout on failed `password` attempts; brute force is possible |
| V-010 | Tampering | Medium | Various command handlers | File path arguments (`add`, `load`, `save`, `readcomments`, `readpicture`) pass unvalidated strings to backend handlers; path traversal risk |
| V-011 | Information Disclosure | Low | `QobuzLoginRequest.cxx:28` | `// TODO: escape` comment; credential parameters appended to URL without URL-encoding |
| V-012 | Tampering / DoS | Low | `src/decoder/Control.hxx:48` | DecoderControl shared-memory IPC between player and decoder threads uses mutex/cond; a compromised decoder plugin that corrupts the command field or holds the mutex could stall playback indefinitely |
| V-013 | Information Disclosure | Low | `src/output/Control.hxx:31` | AudioOutputControl runs the output on its own thread; a compromised output plugin has access to the raw PCM audio buffer (MusicPipe), enabling audio stream interception |

---

### US-007-04: Map Each Threat to a Mitigating Feature
**As a** contributor to MPD-secure,
**I want** every identified threat mapped to the feature responsible for
mitigating it,
**so that** each feature's Definition of Done can reference the specific
threats it must close.

**Acceptance Criteria:**

- Traceability matrix produced: threat ID to mitigating feature(s)
- Every threat from the STRIDE analysis is assigned to exactly one of:
  (a) an existing feature (FEATURE-002 through FEATURE-010),
  (b) a new GitHub issue created at time of discovery, or
  (c) an accepted-risk entry with explicit rationale
- No threat may remain in an untracked state
- Accepted-risk entries require: description of residual risk, rationale for
  acceptance, and the name of the contributor who accepted it

**Traceability matrix (initial — covers known vulnerabilities only):**

| Threat / Vuln | Mitigating Feature |
|---|---|
| V-001 (plaintext passwords, no rate limit) | FEATURE-002, FEATURE-004 (OAuth replaces password command) |
| V-002 (Qobuz password in URL) | FEATURE-002, FEATURE-005 (secure credential storage) |
| V-003 (MD5 signing) | FEATURE-002 |
| V-004 (Qobuz credentials in memory) | FEATURE-002, FEATURE-005 |
| V-005 (Shout password in memory) | FEATURE-002, FEATURE-005 |
| V-006 (ProxyDB cleartext) | FEATURE-004 (TLS/mTLS) |
| V-007 (default full admin) | FEATURE-004 (OAuth replaces permission system) |
| V-008 (UID-based authz unused) | FEATURE-004 |
| V-009 (no brute-force protection) | FEATURE-004 (OAuth replaces password command) |
| V-010 (unvalidated file paths) | FEATURE-002 |
| V-011 (unencoded URL params) | FEATURE-002 |
| V-012 (decoder thread IPC) | FEATURE-003 (plugin sandboxing) |
| V-013 (output plugin audio access) | FEATURE-003 (plugin sandboxing) |

---

### US-007-05: Establish Threat Model as a Living Document
**As a** contributor to MPD-secure,
**I want** the threat model updated as part of every security feature's
Definition of Done,
**so that** the document stays current as mitigations are implemented.

**Acceptance Criteria:**

- Threat model stored at `docs/security/THREAT-MODEL.md`
- Each security feature (FEATURE-002 through FEATURE-010) references this
  document in its Definition of Done
- A threat model review is added to the security code review checklist
  (FEATURE-002, US-002-06)
- Closed threats are marked `[MITIGATED by #X]` with the PR number that
  closed them
- Any new threat discovered during implementation of another feature is
  added to the catalog before that feature's PR is merged

---

## Definition of Ready

- No prerequisites — this feature is the unblocked starting point for the
  entire EPIC

## Definition of Done

- `docs/security/THREAT-MODEL.md` exists and contains:
  - Protected asset inventory with sensitivity ratings
  - Severity rubric (4 levels defined with exploitability and impact)
  - At least 4 adversary profiles with access, motivation, and capability
  - Trust boundary table with all 11 boundaries identified in US-007-01
  - STRIDE analysis table for all 9 components in US-007-02
  - Vulnerability catalog with all entries V-001 through V-013
  - Traceability matrix where every threat maps to a feature or accepted-risk entry
  - Architecture diagram showing trust zones and boundaries
- All 13 known vulnerabilities (V-001 through V-013) are present in the
  catalog with severity, location, and feature mapping
- No threat in the STRIDE analysis is left in an untracked state
- Document reviewed by at least one contributor before merging
- Threat model review added to security code review checklist
