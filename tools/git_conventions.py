#!/usr/bin/env python3
"""Shared, dependency-free Git naming checks for local hooks and trusted CI."""

import argparse
import json
import re
import subprocess
import sys
from pathlib import Path

SUBJECT = re.compile(
    r"^(feature|bugfix|chore|refactor|style|assets)"
    r"(?:\([a-z0-9]+(?:-[a-z0-9]+)*\))?: (\S(?:.*\S)?)$"
)
BRANCH = re.compile(
    r"^(feature|bugfix|Chore|refactor|style|assets)/"
    r"[a-z0-9]+(?:-[a-z0-9]+)*$"
)
BACKUP = re.compile(r"^backup/history-reconstruction-\d{8}T\d{6}Z-[0-9a-f]{7,40}$")
PERMANENT = {"main", "master", "develop"}
SHA = re.compile(r"^[0-9a-f]{40}$")
ZERO = "0" * 40


class ValidationError(ValueError):
    """A naming rule or a required Git precondition failed."""


def git(*args):
    result = subprocess.run(
        ["git", *args], capture_output=True, text=True, encoding="utf-8"
    )
    if result.returncode:
        raise ValidationError(result.stderr.strip() or "Git command failed")
    return result.stdout.strip()


def validate_subject(subject):
    match = SUBJECT.fullmatch(subject)
    if not match or len(subject) > 72:
        raise ValidationError(
            f"Invalid commit subject {subject!r}: use type(scope): description, "
            "at most 72 characters; types: feature, bugfix, chore, refactor, style, assets. "
            "Scope is optional and must be lowercase kebab-case."
        )
    if match.group(2).casefold() in {"update files", "fix stuff"}:
        raise ValidationError("Describe the concrete change, not 'update files' or 'fix stuff'.")


def validate_branch(name, allow_maintenance=True):
    if name in PERMANENT or (allow_maintenance and BACKUP.fullmatch(name)):
        return
    if not BRANCH.fullmatch(name) or name.split("/")[-1] == "changes":
        raise ValidationError(
            f"Invalid branch {name!r}: use feature/, bugfix/, Chore/, refactor/, "
            "style/ or assets/ followed by a descriptive lowercase-kebab-case name."
        )


def checked_sha(value):
    if not isinstance(value, str) or not SHA.fullmatch(value):
        raise ValidationError("Expected a full 40-character Git commit SHA.")
    return value


def validate_commits(revisions):
    commits = git("rev-list", "--reverse", *revisions).splitlines()
    errors = []
    for sha in commits:
        try:
            validate_subject(git("show", "-s", "--format=%s", sha))
        except ValidationError as error:
            errors.append(f"{sha[:12]}: {error}")
    if errors:
        raise ValidationError("\n".join(errors))
    print(f"Validated {len(commits)} commit subject(s).")


def validate_pull_request(event):
    """Run trusted base code; fetch objects only, never execute PR head content."""
    try:
        pr = event["pull_request"]
        number = event["number"]
        head = checked_sha(pr["head"]["sha"])
        base = checked_sha(pr["base"]["sha"])
        branch = pr["head"]["ref"]
        title = pr["title"]
    except (KeyError, TypeError) as error:
        raise ValidationError("Missing pull request metadata.") from error
    if type(number) is not int or number <= 0:
        raise ValidationError("Invalid pull request number.")
    validate_branch(branch, allow_maintenance=False)
    validate_subject(title)
    git("fetch", "--no-tags", "origin", f"refs/pull/{number}/head")
    if git("rev-parse", "FETCH_HEAD") != head:
        raise ValidationError("Pull request changed during validation; rerun the check.")
    validate_commits([f"{base}..{head}"])


def validate_push(lines):
    """Check outgoing branches and commits, including alternate destination names."""
    for line in lines:
        parts = line.split()
        if len(parts) != 4:
            raise ValidationError("Invalid pre-push ref record.")
        local_ref, local_sha, remote_ref, remote_sha = parts
        checked_sha(local_sha)
        checked_sha(remote_sha)
        if not remote_ref.startswith("refs/heads/") or local_sha == ZERO:
            continue  # Tags/deletions have no new branch name or commit subjects.
        destination = remote_ref.removeprefix("refs/heads/")
        validate_branch(destination)
        if local_ref.startswith("refs/heads/"):
            validate_branch(local_ref.removeprefix("refs/heads/"))
        if BACKUP.fullmatch(destination):
            continue  # Explicit recovery refs must preserve original, unvalidated history.
        if remote_sha == ZERO:
            validate_commits([local_sha, "--not", "--remotes"])
        else:
            git("cat-file", "-e", f"{remote_sha}^{{commit}}")
            validate_commits([f"{remote_sha}..{local_sha}"])


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    commands = parser.add_subparsers(dest="command", required=True)
    commit = commands.add_parser("commit")
    commit.add_argument("--message-file", required=True, type=Path)
    branch = commands.add_parser("branch")
    branch.add_argument("name", nargs="?")
    commits = commands.add_parser("range")
    commits.add_argument("--base")
    commits.add_argument("--head", default="HEAD")
    pr = commands.add_parser("pull-request")
    pr.add_argument("--event", required=True, type=Path)
    commands.add_parser("pre-push")
    args = parser.parse_args(argv)
    try:
        if args.command == "commit":
            lines = args.message_file.read_text(encoding="utf-8-sig").splitlines()
            validate_subject(lines[0] if lines else "")
        elif args.command == "branch":
            validate_branch(args.name or git("symbolic-ref", "--quiet", "--short", "HEAD"))
        elif args.command == "range":
            head = git("rev-parse", "--verify", f"{args.head}^{{commit}}")
            base = git("rev-parse", "--verify", f"{args.base}^{{commit}}") if args.base else None
            validate_commits([f"{base}..{head}" if base else head])
        elif args.command == "pull-request":
            validate_pull_request(json.loads(args.event.read_text(encoding="utf-8")))
        else:
            validate_push(sys.stdin)
    except (ValidationError, OSError, UnicodeError, json.JSONDecodeError) as error:
        print(f"Git conventions: {error}", file=sys.stderr)
        return 1
    print("Git conventions: OK")
    return 0


if __name__ == "__main__":
    sys.exit(main())
