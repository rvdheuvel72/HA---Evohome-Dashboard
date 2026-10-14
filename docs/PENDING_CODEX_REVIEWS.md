# Codex review debt — pending

Recorded 2026-10-14 at the user's request. Review quota messages were observed in GitHub on the two active PRs. A verified quota exception allows other successful validation to continue; it does NOT mean a completed Codex review.

| PR | Scope | Status |
| --- | --- | --- |
| [#23](https://github.com/rvdheuvel72/HA---Evohome-Dashboard/pull/23) | Release-date checking, CI gating, safe tags and release policy | Await next available Codex review of final SHA |
| [#21](https://github.com/rvdheuvel72/HA---Evohome-Dashboard/pull/21) | ESP32 RAMSES II/OpenTherm v0.4 firmware; NVS settings, CET/CEST, web UI | Await next available Codex review of final SHA |
| [#24](https://github.com/rvdheuvel72/HA---Evohome-Dashboard/pull/24) | v0.4.0-beta.2 release build, source packaging, current firmware version marking | Await next available Codex review of final SHA |

## Next run checklist

1. Before starting new work, read this file and the latest PR/CI statuses.
2. When Codex reviews are available, request a fresh review of BOTH final diffs.
3. If these PRs are already merged and Codex will not review closed PRs, open a retrospective audit PR referencing them; include this outstanding review in that run.
4. Fix/test any new P1/P2 findings. Close this debt only after a completed review.
5. If quota is still exhausted, keep these entries open and proceed with unrelated green work.

Do not mistake reviewed earlier commits for final-head review. Known P1/P2 findings and failing CI always block publication. Firmware RF transmit stays disabled by default pending physical testing.
