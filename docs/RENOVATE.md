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

## Python 3.15 upgrade hold

Home Assistant 2026.10 runtime imports and the pinned Resideo Python transport
are currently validated on **Python 3.14**. Renovate PR #8 proposed Python 3.15
for the `actions/setup-python` steps, but the HA runtime CI failed installing
dependencies on the self-hosted runner (`orjson`, `lru-dict`, `pycares`
attempted native source builds and required compiler/build tools).

Until the full Home Assistant 2026.10 dependency set can be installed and
imported reliably on 3.15, `renovate.json` restricts the `python` dependency
managed by `github-actions` to `>=3.14 <3.15`. This **does not disable**
Renovate updates for GitHub Actions, other dependency families, or patches
within the 3.14 line. Removing the restriction requires an intentional code
change with passing pinned transport/runtime tests.
