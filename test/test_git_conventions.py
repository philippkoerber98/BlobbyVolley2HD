"""Naming policy, trusted PR metadata, and real Git hook integration tests."""

import importlib.util
import os
import shutil
import subprocess
import tempfile
import unittest
from pathlib import Path
from unittest.mock import patch

ROOT = Path(__file__).resolve().parents[1]
SPEC = importlib.util.spec_from_file_location("git_conventions", ROOT / "tools/git_conventions.py")
rules = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(rules)


class NamingTests(unittest.TestCase):
    def test_valid_subjects(self):
        for subject in (
            "feature(rendering): implement HD resolution scaling",
            "feature(gameplay): add improved player controls",
            "bugfix(physics): correct ball collision behavior",
            "bugfix(audio): prevent overlapping sound effects",
            "chore(build): update compiler configuration",
            "refactor(game): simplify game state management",
            "style(menu): improve button alignment",
            "assets(sprites): replace player textures",
            "assets(audio): update sound effects",
            "assets(fonts): replace legacy font files",
            "chore: maintain tooling",
            "style: " + "a" * 65,
        ):
            with self.subTest(subject=subject):
                rules.validate_subject(subject)

    def test_invalid_subjects(self):
        for subject in (
            "", "feat: add gameplay", "fix: repair audio", "docs: add readme",
            "perf: optimize rendering", "Feature: add rendering", "feature(): add rendering",
            "feature(HD): add rendering", "feature(two_words): add rendering",
            "feature(two--words): add rendering", "feature(scope-): add rendering",
            "feature: ", "feature:   ", "feature: add rendering ",
            "feature:add rendering", "chore: update files", "bugfix: fix stuff",
            "Merge branch 'feature/example'", "style: " + "a" * 66,
            "feature: add rendering\nsecond line",
        ):
            with self.subTest(subject=subject), self.assertRaises(rules.ValidationError):
                rules.validate_subject(subject)

    def test_valid_branches_and_exceptions(self):
        for branch in (
            "feature/hd-rendering", "feature/multiplayer-controls", "bugfix/ball-collision",
            "bugfix/audio-crash", "Chore/update-build-system", "refactor/game-state",
            "style/main-menu", "assets/hd-sprites", "assets/new-sound-effects",
            "main", "master", "develop",
            "backup/history-reconstruction-20261016T135037Z-270b72f",
        ):
            with self.subTest(branch=branch):
                rules.validate_branch(branch)

    def test_invalid_branches(self):
        for branch in (
            "chore/build", "Chore/Build", "feature/changes", "feature/",
            "feature/two_words", "feature/two words", "feature/two--words",
            "feature/nested/name", "feature/trailing-", "feat/name", "codex/name",
            "refs/pull/42/merge", "backup/anything", "--help",
        ):
            with self.subTest(branch=branch), self.assertRaises(rules.ValidationError):
                rules.validate_branch(branch)

    def test_backup_is_not_a_pr_exception(self):
        with self.assertRaises(rules.ValidationError):
            rules.validate_branch("backup/history-reconstruction-20261016T135037Z-270b72f", False)

    def pr_event(self):
        return {
            "number": 42,
            "pull_request": {
                "head": {"ref": "feature/hd-rendering", "sha": "a" * 40},
                "base": {"sha": "b" * 40},
                "title": "feature(rendering): improve HD rendering",
            },
        }

    def test_pr_uses_metadata_and_fetches_only_head_objects(self):
        event = self.pr_event()
        with patch.object(rules, "git", return_value="a" * 40) as git, patch.object(
            rules, "validate_commits"
        ) as commits:
            rules.validate_pull_request(event)
        git.assert_any_call("fetch", "--no-tags", "origin", "refs/pull/42/head")
        commits.assert_called_once_with(["b" * 40 + ".." + "a" * 40])
        self.assertFalse(any("checkout" in call.args or "switch" in call.args for call in git.call_args_list))

    def test_invalid_pr_branch_title_number_and_race(self):
        for field, value in (("ref", "feature/bad_name"), ("title", "bad title"),
                             ("number", "42; echo injected"), ("sha", "--help")):
            event = self.pr_event()
            if field == "number":
                event[field] = value
            elif field == "title":
                event["pull_request"][field] = value
            else:
                event["pull_request"]["head"][field] = value
            with self.subTest(field=field), patch.object(rules, "git") as git:
                with self.assertRaises(rules.ValidationError):
                    rules.validate_pull_request(event)
                git.assert_not_called()
        with patch.object(rules, "git", return_value="c" * 40):
            with self.assertRaisesRegex(rules.ValidationError, "changed during"):
                rules.validate_pull_request(self.pr_event())

    def test_push_destination_and_maintenance_exception(self):
        with self.assertRaises(rules.ValidationError):
            rules.validate_push([f"refs/heads/feature/good {'a'*40} refs/heads/invalid {'0'*40}"])
        with patch.object(rules, "validate_commits") as commits:
            rules.validate_push([
                f"refs/heads/main {'a'*40} refs/heads/feature/good {'0'*40}",
                f"refs/tags/v1 {'a'*40} refs/tags/v1 {'0'*40}",
                f"refs/heads/main {'0'*40} refs/heads/main {'b'*40}",
                f"refs/heads/main {'a'*40} refs/heads/backup/history-reconstruction-20261016T135037Z-270b72f {'0'*40}",
            ])
            commits.assert_called_once_with(["a" * 40, "--not", "--remotes"])


class RealGitTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.directory = Path(self.temp.name)
        self.env = os.environ.copy()
        self.env.update({"GIT_CONFIG_NOSYSTEM": "1", "GIT_CONFIG_GLOBAL": os.devnull})
        for name in ("GIT_DIR", "GIT_WORK_TREE", "GIT_INDEX_FILE", "GIT_AUTHOR_DATE", "GIT_COMMITTER_DATE"):
            self.env.pop(name, None)
        self.run_git("init", "-b", "main")
        self.run_git("config", "user.name", "Convention Test")
        self.run_git("config", "user.email", "test@example.invalid")
        shutil.copytree(ROOT / ".githooks", self.directory / ".githooks")
        (self.directory / "tools").mkdir()
        shutil.copy2(ROOT / "tools/git_conventions.py", self.directory / "tools/git_conventions.py")
        for hook in (self.directory / ".githooks").iterdir():
            hook.chmod(0o755)
        self.run_git("config", "core.hooksPath", ".githooks")

    def run_git(self, *args, success=True):
        result = subprocess.run(
            ["git", *args], cwd=self.directory, env=self.env,
            capture_output=True, text=True, encoding="utf-8", errors="replace",
        )
        if success:
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        else:
            self.assertNotEqual(result.returncode, 0, result.stdout + result.stderr)
        return result

    def test_real_hooks_accept_and_reject_commits_and_branches(self):
        self.run_git("commit", "--allow-empty", "-m", "chore(test): add valid initial commit")
        before = self.run_git("rev-parse", "HEAD").stdout.strip()
        rejected = self.run_git("commit", "--allow-empty", "-m", "fix: invalid type", success=False)
        self.assertIn("Git conventions:", rejected.stderr)
        self.assertEqual(before, self.run_git("rev-parse", "HEAD").stdout.strip())
        self.run_git("switch", "-c", "chore/incorrect-case")
        self.run_git("commit", "--allow-empty", "-m", "chore(test): validate branch", success=False)
        self.run_git("switch", "-c", "Chore/correct-case")
        self.run_git("commit", "--allow-empty", "-m", "chore(test): validate branch")
        # Demonstrate the documented boundary: local hooks are bypassable.
        self.run_git("-c", "core.hooksPath=", "commit", "--allow-empty", "-m", "bad subject")
        with patch.object(rules, "git", side_effect=lambda *args: self.run_git(*args).stdout.strip()):
            with self.assertRaisesRegex(rules.ValidationError, "bad subject"):
                rules.validate_commits(["HEAD"])
            rules.validate_commits(["HEAD~2..HEAD~1"])

    def test_real_pre_push_checks_remote_names_and_commit_ranges(self):
        self.run_git("commit", "--allow-empty", "-m", "chore(test): add valid initial commit")
        remote = self.directory / "remote.git"
        self.run_git("init", "--bare", str(remote))
        self.run_git("remote", "add", "origin", str(remote))
        result = self.run_git("push", "origin", "main:bad-name", success=False)
        self.assertIn("Invalid branch", result.stderr)
        self.run_git("push", "origin", "main:main")
        self.run_git("switch", "-c", "feature/check-commits")
        self.run_git("commit", "--allow-empty", "-m", "feature(test): add valid branch commit")
        self.run_git("push", "origin", "HEAD:refs/heads/feature/check-commits")
        previous = self.run_git("rev-parse", "HEAD").stdout.strip()
        self.run_git("-c", "core.hooksPath=", "commit", "--allow-empty", "-m", "invalid subject")
        result = self.run_git("push", "origin", "HEAD:refs/heads/feature/check-commits", success=False)
        self.assertIn("Invalid commit subject", result.stderr)
        self.assertEqual(previous, self.run_git("ls-remote", "origin", "refs/heads/feature/check-commits").stdout.split()[0])


if __name__ == "__main__":
    unittest.main()
