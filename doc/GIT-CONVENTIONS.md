# Git conventions and installation

`AGENTS.md` defines the full policy. `tools/git_conventions.py` is the shared
Python 3.9+ standard-library implementation for hooks and pull request CI.

## Install in each clone

From the repository root, with Python and Git available in PATH:

```powershell
python --version
git config --local core.hooksPath .githooks
git config --local --get core.hooksPath
python tools/git_conventions.py branch
python -m unittest discover -s test -p test_git_conventions.py
```

The hook scripts are executable POSIX shell files with LF endings. Git for
Windows supplies the shell; Linux and macOS can run the same hooks. The runner
tries `python3` and `python` and requires version 3.9 or newer. No pip packages
are required. A missing interpreter fails with an installation hint.

`pre-commit` validates the current branch. `commit-msg` validates the subject.
`pre-push` validates both source and destination branch names and outgoing
commits. Pushing tags and deleting refs adds no new branch/commit to validate.
For a new branch, commits already reachable through local remote-tracking refs
are excluded. Fetch before pushing if an existing remote commit is unavailable.

Manual checks:

```powershell
python tools/git_conventions.py branch Chore/update-build-system
python tools/git_conventions.py commit --message-file .git/COMMIT_EDITMSG
python tools/git_conventions.py range --base origin/main --head HEAD
python tools/git_conventions.py range --head HEAD
```

Without `--base`, the range command checks every ancestor, including the root.
Merge commits are checked too; use a compliant merge subject or a compliant
squash/rebase strategy. When squashing, preserve a compliant final subject.

## Exact checks and exceptions

Commit types: `feature`, `bugfix`, `chore`, `refactor`, `style`, `assets`.
Optional scopes are lowercase kebab-case. Subjects must contain a nonblank
description and be at most 72 characters. Trailing whitespace, empty scopes,
double/trailing scope hyphens, and the vague descriptions "update files" and
"fix stuff" are rejected. English, imperative wording, descriptive intent and
logical commit boundaries remain review responsibilities.

Working prefixes: `feature/`, `bugfix/`, **`Chore/`**, `refactor/`, `style/`,
`assets/`, followed by lowercase kebab-case. `main`, `master` and `develop` are
permanent branch exceptions. The only safety branch exception is
`backup/history-reconstruction-YYYYMMDDTHHMMSSZ-<7-to-40-hex-sha>`; pushing one
may preserve original noncompliant subjects for recovery. PRs never receive
this safety exception. No environment switch bypasses checks.

## Pull request workflow and security boundaries

`.github/workflows/git-conventions.yml` provides the stable job name
**Git conventions validation**. It uses `pull_request_target`, checks out only
the trusted base SHA and runs the validator from that base. It never checks out,
builds or executes files from the PR head. The token has only `contents: read`;
checkout credentials permit read-only fetching, including private repositories.
PR numbers and SHAs are validated and all Git arguments use subprocess arrays.

The source name comes from `pull_request.head.ref`, not `HEAD` or a temporary
`refs/pull/123/merge`. The PR title is also checked for compliant squash defaults.
The validator fetches GitHub's PR head ref, verifies it matches event metadata,
then checks every commit in `base.sha..head.sha`. If the PR changed meanwhile,
rerun validation. Editing the title reruns the check. The trusted base must
already contain these standards; the initial introduction is a bootstrap step.

This file does not enable GitHub Actions or configure branch protection.
Actions were previously disabled at repository level and are not re-enabled by
this task. After explicit approval, Actions must be enabled and the exact job
must be made a required check in native protection settings to block merges.
Verify those settings separately rather than assuming a saved workflow is active.

Hooks are opt-in, local and bypassable (`--no-verify`, configuration changes).
CI alone cannot prevent invalid branch creation, direct pushes, branch deletion,
force pushes, administrator bypasses or manual edits to squash subjects. Native
rulesets/branch protection and commit-metadata restrictions, where supported by
the GitHub plan, are needed for those guarantees. Merge queues are not configured
by this workflow. Do not change repository security settings without explicit
approval.
