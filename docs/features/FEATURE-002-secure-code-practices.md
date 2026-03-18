# FEATURE-002: Refactor Codebase for Secure Code Practices

**GitHub Issue:** https://github.com/devinhedge/MPD-secure/issues/2
**EPIC:** #1 MPD-secure Zero-Trust Network Security
**Status:** Planned

## What the User Experiences

A technical music enthusiast running MPD-secure on their home network can trust
that the application itself does not introduce vulnerabilities. The codebase
handles all input defensively, stores no secrets in plaintext, and produces no
code paths that a hostile actor on the local network could exploit to escalate
privileges or exfiltrate data.

## Zero-Trust Alignment

Secure code practices eliminate the attack surface from within. Code that
handles input unsafely, manages memory incorrectly, or trusts implicit state
violates zero-trust at the implementation level — before a single network
packet is sent.

## User Stories

### US-002-01: Static Analysis Baseline
**As a** contributor to MPD-secure,
**I want** a SAST tool integrated and all existing findings triaged,
**so that** we have a documented baseline of the current security posture
and a clear list of issues to remediate.

**Acceptance Criteria:**
- SAST tool configured and running against the C++ codebase
- All findings catalogued by severity (Critical, High, Medium, Low)
- Critical and High findings have assigned remediation owners
- Baseline report committed to `docs/security/sast-baseline.md`

---

### US-002-02: Eliminate Hardcoded Secrets and Plaintext Credentials
**As a** user running MPD-secure,
**I want** the codebase to contain no hardcoded tokens, passwords, or API keys,
**so that** a repository compromise does not expose credentials.

**Acceptance Criteria:**
- Secret-detection scan passes with zero findings on the full commit history
- Any previously hardcoded values are rotated and replaced with runtime
  references to the credential store (see FEATURE-005)
- Secret-detection is enforced in CI (see FEATURE-006)

---

### US-002-03: Input Validation at All Trust Boundaries
**As a** user running MPD-secure,
**I want** all external input to be validated and rejected loudly on failure,
**so that** malformed or malicious input cannot cause undefined behavior or
  exploitation.

**Acceptance Criteria:**
- All client command inputs are validated before processing
- All file path inputs are validated and normalized (no path traversal)
- All plugin-supplied data is treated as untrusted and validated before use
- Validation failures return explicit error codes, not silent truncation or
  undefined behavior

---

### US-002-04: Memory Safety Audit
**As a** contributor to MPD-secure,
**I want** the codebase audited for unsafe memory patterns,
**so that** buffer overflows, use-after-free, and out-of-bounds reads cannot
  be exploited.

**Acceptance Criteria:**
- All raw pointer arithmetic reviewed and documented or replaced with
  safer alternatives (std::span, std::array, RAII wrappers)
- AddressSanitizer (ASAN) and UndefinedBehaviorSanitizer (UBSAN) run clean
  on the test suite
- Any suppressions are documented with rationale in `docs/security/asan-suppressions.md`

---

### US-002-05: Least-Privilege Access Patterns
**As a** user running MPD-secure,
**I want** each component to access only what it needs,
**so that** a compromised component cannot reach data or capabilities outside
  its intended scope.

**Acceptance Criteria:**
- Each module's file system access is audited and scoped to its minimum
  required paths
- No module holds open handles, connections, or capabilities beyond the
  scope of the current operation
- Privilege separation between the daemon core and plugins is documented

---

### US-002-06: Security Code Review Checklist
**As a** contributor to MPD-secure,
**I want** a security-focused code review checklist,
**so that** every PR is evaluated against a consistent set of security criteria
  before merge.

**Acceptance Criteria:**
- Checklist exists at `docs/standards/SECURITY-REVIEW-CHECKLIST.md`
- Checklist covers: input validation, memory safety, secret handling,
  error handling, and logging
- Checklist is referenced in the PR template (`.github/PULL_REQUEST_TEMPLATE.md`)

---

## Definition of Ready

- STRIDE threat model (FEATURE-007) completed or in progress, so input
  validation scope is informed by documented threat actors
- SAST tool selected and agreed upon

## Definition of Done

- All Critical and High SAST findings remediated
- ASAN and UBSAN run clean on CI
- No hardcoded secrets in codebase or commit history
- Input validation present at all client and plugin trust boundaries
- Security code review checklist in place and applied to all new PRs
- Baseline security report committed to `docs/security/`
