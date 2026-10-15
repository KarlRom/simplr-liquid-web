#!/usr/bin/env bash
# Publishes a *new*, separate GitHub Pages repository without overwriting an existing site.
# Prerequisites: GitHub CLI (gh), git, and `gh auth login`.
set -euo pipefail
repo="${1:-simplr-liquid-web}"
if [[ ! "$repo" =~ ^[a-zA-Z0-9._-]+$ ]]; then
  printf 'ERROR: pass a repository name (without owner or URL).\n' >&2; exit 1
fi
for program in gh git; do
  command -v "$program" >/dev/null || { printf 'Missing %s. On Mac, run: brew install gh git\n' "$program" >&2; exit 1; }
done
gh auth status >/dev/null || { printf 'Connect GitHub first using: gh auth login\n' >&2; exit 1; }
owner="$(gh api user --jq .login)"
if gh repo view "$owner/$repo" --json name >/dev/null 2>&1; then
  printf 'ABORTED: %s/%s already exists; this script never overwrites existing repositories.\n' "$owner" "$repo" >&2
  exit 1
fi
project="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
temporary="$(mktemp -d)"
trap 'rm -rf "$temporary"' EXIT
mkdir -p "$temporary/.github/workflows"
cp -R "$project/web" "$temporary/web"
cp "$project/.github/workflows/pages.yml" "$temporary/.github/workflows/pages.yml"
cp "$project/README_WEB_EDITION.md" "$temporary/README.md"
git -C "$temporary" init -b main -q
git -C "$temporary" add web .github README.md
git -C "$temporary" -c "user.name=$owner" -c "user.email=$owner@users.noreply.github.com" commit -qm 'Publish SIMPL_R Liquid v0.23.13 Web Edition'
gh repo create "$owner/$repo" --public --source="$temporary" --remote=origin --push
# A GitHub Pages site using custom Actions workflows can be activated via this REST endpoint.
if ! gh api --method POST "repos/$owner/$repo/pages" -f build_type=workflow >/dev/null; then
  printf 'Automatic Pages activation was not completed.\n' >&2
  printf 'Open https://github.com/%s/%s/settings/pages and select GitHub Actions as the build source.\n' "$owner" "$repo" >&2
fi
if ! gh workflow run -R "$owner/$repo" 'Deploy SIMPL_R Liquid Web Edition' --ref main; then
  printf 'Check Actions in the new repository and run the Pages workflow manually if needed.\n' >&2
fi
printf '\nRepository: https://github.com/%s/%s\n' "$owner" "$repo"
printf 'Expected Pages address after deployment: https://%s.github.io/%s/\n' "$owner" "$repo"
printf 'Confirm deployment success in the GitHub Actions / Pages UI before sharing.\n'
