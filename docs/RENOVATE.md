# Renovate dependency maintenance

`renovate.json` configures Renovate for GitHub Actions and the pinned
`evohome-async` requirement inside the HA integration's `manifest.json`.

## Enable the bot

1. Install the **Mend Renovate GitHub App** at https://github.com/apps/renovate
   and grant access to **rvdheuvel72/HA---Evohome-Dashboard**.
2. The bot will detect `renovate.json` on `main` and open dependency PRs.
3. Major changes to the transport library require an API-contract update
   and Codex P1/P2 validation. Renovate never automatically merges.
4. Keep `main` branch protection enabled: required pull request,
   Codex gate, Python/JS build/unit tests and version-pinned API contract.
5. Inspect Renovate Dependency Dashboard PRs after installation.

The GitHub App installation is controlled by the repository owner. Committing
this configuration alone **does not** activate the app or schedule its runs.

No plaintext Resideo tokens or credentials are used by Renovate.
