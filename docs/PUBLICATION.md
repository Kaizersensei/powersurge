# Private work and public previews

Signed: President, 2026-11-22.

Private work includes unfinished source experiments, source-prototype inventories, raw logs/screenshots, edited disposable workspaces, machine-local paths, collaboration ledgers, saves and recovery files. Keep these in ignored out/private (or owned temporary storage). Existing historical reports and research have been preserved locally and removed from the GitHub-facing working tree; Git history has not been rewritten. CLAIMS.md and HANDOFF.md remain local ignored collaboration documents.

Public work includes reviewed reusable engine source, curated redistributable fixtures, dependency/asset notices, native debug/test scenarios and concise acceptance summaries. Passing a new test requires deliberate promotion of its reusable public version; no automatic copying of private runs. Do not imply full gameplay acceptance from a focused test. Keep incomplete capabilities identified as experimental.

Build locally with powersurge_portable. Use powersurge_candidate to assemble a separate distribution candidate without private debug symbols, run its native check, then record acceptance. Public/accepted.json pins that exact executable, complete approved runtime file set, source set and content. Compiler file-prefix maps keep diagnostics independent of the developer workspace; external static dependencies need the same treatment. The debug build remains local for diagnosis. Build powersurge_public to verify these pins and produce a separate preview under out/public, including runtime and matching source ZIPs. Public source assembly uses an explicit allowlist. Public runtime assembly refuses personal paths and excludes reports, user progress and recovery files. A changed implementation or fixture requires a new accepted record. The target does not run Python, launch external test frameworks, commit, push or upload a release.

Git ignores do not remove files already tracked. Existing tracked internal records are explicitly removed from the index while their private copies remain local. Do not force-add ignored ledgers or research. Preserve history and obtain a separate decision before any history purge.

Public assembly requires CMake 3.31 or newer. Current SurgeScript dependency pin: a0e8719d833000715c98eb34c129c56d6321d735 (6.9.0). Build its static library with source/build file-prefix maps before linking a public candidate.
