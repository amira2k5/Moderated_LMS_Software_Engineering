# Branching strategy and workflow

GitHub flow with protected `main`.

* `main` always builds and passes all tests. Direct pushes are not allowed.
* Work happens on short-lived branches named `feature/<topic>`, `fix/<topic>` or `docs/<topic>`.
* Each branch is merged by a pull request with a reviewer, after the CI pipeline passes (build, tests,
  static analysis, coverage of at least 70% for the business-logic layer). Merge commits keep each feature visible.
* Commit messages: a short imperative summary line, a blank line, then the reason for the change.
* Releases are annotated tags `vMAJOR.MINOR.PATCH` on `main` (for example `v1.0.0`). A change request that adds
  behaviour raises the minor number (`v1.1.0`); a bug fix only raises the patch number.
* A change request starts with an impact analysis (`docs/`), and ends with updated design documents, new tests and a
  full regression run before the tag.

Pull request checklist: tests added or updated; no compiler warnings; `cppcheck` clean; documents updated; requirement
IDs mentioned in the description.
