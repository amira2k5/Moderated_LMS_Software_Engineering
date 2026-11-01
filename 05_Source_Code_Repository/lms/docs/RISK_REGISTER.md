# Risk register (updated at completion)

Likelihood (L) and impact (I) are scored 1 to 5; exposure = L x I. "Initial" is the Phase 1 score; "Final" is the residual score at completion.

| ID | Risk | Type | Initial L x I | Mitigation | Owner | Final L x I | Status and evidence |
|---|---|---|---|---|---|---|---|
| R1 | Schedule overruns (7 weeks fixed) | Project | 4 x 5 = 20 | MoSCoW; Must items first; weekly check | Developer | 2 x 3 = 6 | Closed. All Must and Should requirements built; FR-23 (Could) descoped |
| R2 | Stakeholder conflicts unresolved | People | 4 x 4 = 16 | Written resolutions baselined at Gate 1 | Developer | 1 x 3 = 3 | Closed. Eleven resolutions in the SRS; assumptions listed for confirmation |
| R3 | Skill gaps in C++, SQLite, CI | Technical | 3 x 4 = 12 | Small interfaces; early tests | Developer | 2 x 3 = 6 | Mitigated. 104 tests, 98.6% coverage of business logic |
| R4 | Database lost or corrupted | Technical | 2 x 5 = 10 | Transactions; backup; restore guide | IT officer | 2 x 3 = 6 | Mitigated. Backup function (FR-22), all-or-nothing tests, install guide. Needs a rehearsed restore on site |
| R5 | Stakeholders unavailable for review | People | 3 x 3 = 9 | Early bookings; supervisor as proxy | Developer | 3 x 2 = 6 | Open. NFR-08 usability test with real lecturers still to do |
| R6 | Scope creep | Project | 3 x 3 = 9 | Change control; out-of-scope list | Developer | 1 x 2 = 2 | Closed. Only CR1 accepted, through impact analysis |
| R7 | Hashing library does not build on old machines | Technical | 2 x 4 = 8 | Early CI build; interface | Developer | 2 x 3 = 6 | Open. Built on Ubuntu 24.04 (libsodium 1.0.18) only; try the school's computers before go-live |
| R8 | Slow with 800 students | Technical | 2 x 3 = 6 | Indexes; performance test | Developer | 1 x 2 = 2 | Closed. Login 78 ms; listing and saving a few milliseconds |
| R9 | New: students who join after a class extension miss it | Project | n/a | Documented; lecturer repeats the action | Developer | 3 x 2 = 6 | Open (accepted limitation of CR1, see `CHANGE_REQUEST_CR1.md`) |
| R10 | New: SQLite on a network folder can lock badly | Technical | n/a | Single computer is the supported set-up; shared folder only for light use; busy timeout and test | IT officer | 2 x 4 = 8 | Open. Concurrency test passes on one computer, not on a real school network |
| R11 | New: Windows 10 users cannot run the Linux build | Project | n/a | Supported through WSL2; document in install guide | Developer | 3 x 3 = 9 | Open. Native Windows build untested |
