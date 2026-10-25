"""CI policy: review-first, second final review and exact-head merge safety."""
import sys
import unittest
from datetime import datetime, timezone
from urllib.error import HTTPError
from pathlib import Path
from unittest.mock import patch, MagicMock

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "scripts"))
import check_codex_review as gate
import manage_codex_request as requests
import auto_merge_codex as merge


class CodexFirstTests(unittest.TestCase):
    SHA = "a" * 40
    REPO = "example/evohome"
    PR = {
        "state": "open", "draft": False,
        "head": {"sha": SHA, "repo": {"full_name": REPO}},
        "base": {"ref": "main"},
    }

    def summary(self, updated="2026-10-24T10:20:00Z"):
        return {
            "user": {"login": "chatgpt-codex-connector[bot]"},
            "body": ("<!-- codex-pull-request-review-summary -->\n"
                     "✅ **Completed** `aaaaaaa`"),
            "updated_at": updated,
        }

    def test_expected_codex_blocker_requests_non_green_cancellation(self):
        """Never exit-success or continue to self-hosted builds for P1/P2."""
        response = MagicMock()
        response.__enter__.return_value.status = 202
        with patch.object(gate, "urlopen", return_value=response) as api, \
             patch.object(gate.time, "sleep", side_effect=KeyboardInterrupt):
            with self.assertRaises(KeyboardInterrupt):
                gate.cancel_expected_blocker(
                    "token", self.REPO, "P1/P2 must be fixed before CI", "12345")
        sent = api.call_args.args[0]
        self.assertEqual(sent.full_url,
                         "https://api.github.com/repos/example/evohome/actions/runs/12345/cancel")
        self.assertEqual(sent.get_method(), "POST")

    def test_cancellation_never_succeeds_without_accepted_api_result(self):
        response = MagicMock()
        response.__enter__.return_value.status = 403
        with patch.object(gate, "urlopen", return_value=response), \
             patch.object(gate.time, "sleep") as sleep:
            with self.assertRaisesRegex(RuntimeError, "did not accept"):
                gate.cancel_expected_blocker("token", self.REPO, "P2 found", "12345")
            sleep.assert_not_called()

    def test_main_routes_real_codex_blocked_state_to_cancellation(self):
        """Integration regression: producer state MUST reach cancel path."""
        import json
        from tempfile import NamedTemporaryFile
        with NamedTemporaryFile("w+", encoding="utf-8") as event:
            json.dump({"pull_request":{"number":12,
                      "head":{"sha":self.SHA}}},event)
            event.flush()
            environ = {"GITHUB_EVENT_PATH":event.name,
                       "GITHUB_TOKEN":"token",
                       "GITHUB_REPOSITORY":self.REPO,
                       "CODEX_EXPECTED_BLOCK_POLICY":"cancel"}
            with patch.dict(gate.os.environ,environ), \
                 patch.object(sys,"argv",["check_codex_review.py","--review-only"]), \
                 patch.object(gate,"check_review_only",
                              return_value=("codex_blocked","found P2")), \
                 patch.object(gate,"cancel_expected_blocker",
                              side_effect=KeyboardInterrupt) as cancel:
                with self.assertRaises(KeyboardInterrupt):
                    gate.main()
                cancel.assert_called_once_with("token",self.REPO,"found P2")

    def test_draft_blocker_does_not_cancel(self):
        """Only a verified Codex finding, never a draft, may cancel."""
        import json
        from tempfile import NamedTemporaryFile
        with NamedTemporaryFile("w+", encoding="utf-8") as event:
            json.dump({"pull_request":{"number":12,
                      "head":{"sha":self.SHA}}},event)
            event.flush()
            environ = {"GITHUB_EVENT_PATH":event.name,
                       "GITHUB_TOKEN":"token",
                       "GITHUB_REPOSITORY":self.REPO,
                       "CODEX_EXPECTED_BLOCK_POLICY":"cancel"}
            with patch.dict(gate.os.environ,environ), \
                 patch.object(sys,"argv",["check_codex_review.py","--review-only"]), \
                 patch.object(gate,"check_review_only",
                              return_value=("blocked","Draft PR")), \
                 patch.object(gate,"cancel_expected_blocker") as cancel:
                self.assertEqual(gate.main(),1)
                cancel.assert_not_called()

    def test_cancellation_without_valid_run_id_fails_closed(self):
        with self.assertRaisesRegex(RuntimeError, "workflow-run ID"):
            gate.cancel_expected_blocker("token", self.REPO, "P1 found", "bad")

    def test_ha_ci_does_not_run_for_codex_or_firmware_changes(self):
        self.assertFalse(gate.ha_validation_required([
            "scripts/check_codex_review.py",
            "tests/test_codex_first_pipeline.py",
            ".github/workflows/trusted-ramses-compile.yml",
            "firmware/arduino-wroom32e/test.cpp",
        ]))
        self.assertTrue(gate.ha_validation_required([
            "custom_components/evohome_dashboard/climate.py",
        ]))
        self.assertTrue(gate.ha_validation_required(["tests/test_schedules.py"]))

    def test_review_first_prevents_build_when_pending(self):
        with patch.object(gate, "request", return_value=self.PR), \
             patch.object(gate, "blockers", return_value=[]), \
             patch.object(gate, "pages", return_value=[]):
            state, _ = gate.check_review_only("token", self.REPO, 12, self.SHA)
            self.assertEqual(state, "pending")

    def test_review_first_blocks_p1(self):
        with patch.object(gate, "request", return_value=self.PR), \
             patch.object(gate, "blockers", return_value=["P1: safety"]):
            state, _ = gate.check_review_only("token", self.REPO, 12, self.SHA)
            self.assertEqual(state, "codex_blocked")

    def test_review_first_requires_real_completion_no_quota_bypass(self):
        with patch.object(gate, "request", return_value=self.PR), \
             patch.object(gate, "blockers", return_value=[]), \
             patch.object(gate, "pages", side_effect=[[], [self.summary()]]):
            state, _ = gate.check_review_only("token", self.REPO, 12, self.SHA)
            self.assertEqual(state, "passed")

    def test_skipped_approved_build_is_pending_not_a_pass_or_failure(self):
        # A previous pre-review run intentionally skipped its expensive job.
        # A new reviewed attempt may be queued, but the old skipped check
        # must NEVER be treated as success or a permanent CI error.
        with patch.object(gate, "pages",
                          return_value=[{"filename": "firmware/arduino-wroom32e/test.h"}]), \
             patch.object(gate, "request", return_value={
                 "check_runs": [{"name": "Trusted Arduino compile",
                                 "status": "completed", "conclusion": "skipped"}]}):
            state, _ = gate.required_ci_state("token", self.REPO, self.SHA,
                                               pr_number=12)
        self.assertEqual(state, "pending")

    def test_post_ci_review_must_be_after_request(self):
        marker = "<!-- codex-final-review:" + self.SHA + " -->"
        final = {
            "user": {"login": "github-actions[bot]"},
            "created_at": "2026-10-24T10:30:00Z",
            "body": marker,
        }
        old = self.summary(updated="2026-10-24T10:20:00Z")
        with patch.object(gate, "check_ci", return_value=("passed", "OK")), \
             patch.object(gate, "check_review_only", return_value=("passed", "OK")), \
             patch.object(gate, "final_review_requested_at",
                          return_value=datetime(2026, 10, 24, 10, 30, tzinfo=timezone.utc)), \
             patch.object(gate, "pages", return_value=[final, old]):
            state, _ = gate.check_final_review("token", self.REPO, 12, self.SHA)
            self.assertEqual(state, "pending")
        new = self.summary(updated="2026-10-24T10:31:00Z")
        with patch.object(gate, "check_ci", return_value=("passed", "OK")), \
             patch.object(gate, "check_review_only", return_value=("passed", "OK")), \
             patch.object(gate, "final_review_requested_at",
                          return_value=datetime(2026, 10, 24, 10, 30, tzinfo=timezone.utc)), \
             patch.object(gate, "pages", return_value=[final, new]):
            state, _ = gate.check_final_review("token", self.REPO, 12, self.SHA)
            self.assertEqual(state, "passed")

    def test_forged_final_marker_is_rejected(self):
        marker = "<!-- codex-final-review:" + self.SHA + ":run=1234 -->"
        comment = {"id": 321, "user": {"login": "github-actions[bot]"},
                   "created_at": "2026-10-24T10:30:00Z",
                   "body": marker}
        fake_run = {"path": ".github/workflows/pr-owned.yml",
                    "event": "pull_request_target",
                    "repository": {"full_name": self.REPO}}
        with patch.object(gate, "request", return_value=fake_run):
            self.assertIsNone(gate.final_review_requested_at(
                "token", self.REPO, 12, [comment], self.SHA))

    def test_real_protected_final_job_is_authenticated(self):
        marker = "<!-- codex-final-review:" + self.SHA + ":run=1234 -->"
        comment = {"id": 321, "user": {"login": "github-actions[bot]"},
                   "created_at": "2026-10-24T10:30:00Z",
                   "body": marker}
        trusted_run = {"path": ".github/workflows/codex-p1-p2-review.yml",
                       "event": "pull_request_target",
                       "head_sha": "b"*40,  # protected base SHA, not PR head
                       "pull_requests":[{"number":12,"head":{"sha":self.SHA}}],
                       "repository": {"full_name": self.REPO}}
        trusted_jobs = {"total_count": 1,
            "jobs": [{"id":99,"name": "Request second Codex review AFTER CI",
                      "conclusion": "success",
                      "started_at": "2026-10-24T10:29:55Z",
                      "completed_at": "2026-10-24T10:30:01Z"}]}
        log = f"CODEX_FINAL_REQUEST pr=12 sha={self.SHA} comment_id=321"
        with patch.object(gate, "request",
                          side_effect=[trusted_run, trusted_jobs]), \
             patch.object(gate, "protected_job_log", return_value=log):
            self.assertEqual(
                gate.final_review_requested_at("token", self.REPO, 12,
                                               [comment], self.SHA),
                datetime(2026, 10, 24, 10, 30, tzinfo=timezone.utc))

    def test_other_pr_protected_final_job_cannot_authorize_merge(self):
        marker = "<!-- codex-final-review:" + self.SHA + ":run=1234 -->"
        comment = {"id": 321, "user": {"login": "github-actions[bot]"},
                   "created_at": "2026-10-24T10:30:00Z", "body": marker}
        run = {"path": ".github/workflows/codex-p1-p2-review.yml",
               "event": "pull_request_target", "head_sha": self.SHA,
               "pull_requests": [{"number":777,"head":{"sha":self.SHA}}],
               "repository":{"full_name":self.REPO}}
        with patch.object(gate,"request",return_value=run), \
             patch.object(gate,"protected_job_log") as logs:
            self.assertIsNone(gate.final_review_requested_at(
                "token",self.REPO,12,[comment],self.SHA))
            logs.assert_not_called()

    def test_same_pr_forged_comment_cannot_reuse_protected_log(self):
        marker = "<!-- codex-final-review:" + self.SHA + ":run=1234 -->"
        comment = {"id": 998, "user": {"login": "github-actions[bot]"},
                   "created_at": "2026-10-24T10:30:00Z", "body": marker}
        run = {"path": ".github/workflows/codex-p1-p2-review.yml",
               "event": "issue_comment",
               "actor":{"login":"chatgpt-codex-connector[bot]"},
               "repository":{"full_name":self.REPO}}
        jobs = {"total_count":1,
                "jobs":[{"id":99,"name":"Request second Codex review AFTER CI",
                         "conclusion":"success",
                         "started_at":"2026-10-24T10:29:55Z",
                         "completed_at":"2026-10-24T10:30:01Z"}]}
        log = f"CODEX_FINAL_REQUEST pr=12 sha={self.SHA} comment_id=321"
        with patch.object(gate,"request",side_effect=[run,jobs]), \
             patch.object(gate,"protected_job_log",return_value=log):
            self.assertIsNone(gate.final_review_requested_at(
                "token",self.REPO,12,[comment],self.SHA))

    def test_recover_protected_firmware_run_with_base_sha(self):
        import resume_after_codex as recovery
        protected_run = {
            "id":901, "status":"completed", "run_attempt":1,
            "head_sha":"b"*40,  # pull_request_target => protected base
            "pull_requests":[{"number":12,"head":{"sha":self.SHA}}],
        }
        jobs = {"total_count":2,"jobs":[
            {"name":"Codex BEFORE CI - protected Arduino",
             "conclusion":"success"},
            {"name":"Trusted Arduino compile", "conclusion":"skipped"},
        ]}
        with patch.object(recovery,"check_review_only",
                          return_value=("passed","Exact SHA green")), \
             patch.object(recovery,"request",side_effect=[
                  {"workflow_runs":[]},
                  {"workflow_runs":[protected_run]},
                  jobs]), \
             patch.object(recovery,"post_retry") as retried:
            recovery.resume("token",self.REPO,12,self.SHA)
            retried.assert_called_once_with(
                "token",f"/repos/{self.REPO}/actions/runs/901/rerun")

    def test_late_codex_completion_waits_for_inflight_precheck(self):
        import resume_after_codex as recovery
        pending = {"id": 731, "status":"in_progress", "run_attempt":1,
                   "head_sha":self.SHA,
                   "pull_requests":[{"number":12,"head":{"sha":self.SHA}}]}
        completed = dict(pending, status="completed")
        jobs = {"total_count":2,"jobs":[
            {"name":"Codex BEFORE CI - Home Assistant",
             "conclusion":"success"},
            {"name":"Python contract and dashboard syntax",
             "conclusion":"skipped"},
        ]}
        with patch.object(recovery,"check_review_only",
                          return_value=("passed","green")), \
             patch.object(recovery,"request",side_effect=[
                 {"workflow_runs":[pending]},
                 {"workflow_runs":[completed]},
                 jobs,
                 {"workflow_runs":[]}
             ]), \
             patch.object(recovery.time,"sleep") as sleep, \
             patch.object(recovery,"post_retry") as retry:
            recovery.resume("token",self.REPO,12,self.SHA)
            sleep.assert_called_once_with(5)
            retry.assert_called_once_with(
                "token",f"/repos/{self.REPO}/actions/runs/731/rerun")

    def test_ha_filter_matches_required_script_and_test_paths(self):
        workflow = (Path(__file__).resolve().parents[1] /
                    ".github/workflows/phase1-self-hosted.yml").read_text()
        for path in ("scripts/check_codex_review.py",
                     "scripts/auto_merge_codex.py",
                     "tests/test_codex_first_pipeline.py"):
            self.assertTrue(gate.ha_validation_required([path]))
        self.assertIn("'scripts/**'", workflow)
        self.assertIn("'tests/**'", workflow)

    def test_auto_merge_rejects_stale_head(self):
        changed = dict(self.PR)
        changed["head"] = {"sha": "b" * 40,
                           "repo": {"full_name": self.REPO}}
        with patch.object(merge, "request", return_value=changed):
            with self.assertRaisesRegex(RuntimeError, "head"):
                merge.validate("token", self.REPO, 12, self.SHA)

    def test_auto_merge_rejects_incomplete_final(self):
        with patch.object(merge, "request", return_value=self.PR), \
             patch.object(merge, "check_final_review", return_value=("pending", "Waiting")):
            with self.assertRaisesRegex(RuntimeError, "not green"):
                merge.validate("token", self.REPO, 12, self.SHA)

    def test_nonexistent_run_marker_is_ignored_not_fatal(self):
        marker = "<!-- codex-final-review:" + self.SHA + ":run=0 -->"
        invalid = {"id": 1, "user": {"login": "github-actions[bot]"},
                   "created_at": "2026-10-24T10:30:00Z", "body": marker}
        error = HTTPError("https://api.github.com", 404, "Not Found", {}, None)
        with patch.object(gate, "request", side_effect=error):
            self.assertIsNone(gate.final_review_requested_at(
                "token", self.REPO, 12, [invalid], self.SHA))

    def test_recovery_skips_unsafe_ref(self):
        import resume_after_codex as recovery
        with patch.object(recovery, "check_review_only",
                          return_value=("blocked", "P1 found")), \
             patch.object(recovery, "post_retry") as retry:
            recovery.resume("token", self.REPO, 12, self.SHA)
            retry.assert_not_called()

    def test_pre_request_idempotent(self):
        marker = "<!-- codex-pre-review:" + self.SHA + ":run=1234 -->"
        comments = [{
            "user": {"login": "github-actions[bot]"},
            "body": marker,
        }]
        with patch.object(requests, "request", return_value=self.PR) as post, \
             patch.object(requests, "pages", side_effect=[comments, []]):
            requests.ensure_request("token", self.REPO, 12, self.SHA, "pre")
            self.assertEqual(post.call_count, 1)


if __name__ == "__main__":
    unittest.main()
