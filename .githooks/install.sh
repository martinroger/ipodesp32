#!/usr/bin/env bash
# ==============================================================================
# Installer script for standard workspace git hooks
# ==============================================================================
set -e

REPO_ROOT="$(git rev-parse --show-toplevel 2>/dev/null || pwd)"
cd "$REPO_ROOT"

echo "Configuring git hooks for repository: $(basename "$REPO_ROOT")..."

if [ ! -d ".githooks" ]; then
    echo "❌ Error: .githooks directory not found in repository root."
    exit 1
fi

# Make hooks executable
chmod +x .githooks/pre-commit 2>/dev/null || true
chmod +x .githooks/pre-push 2>/dev/null || true

# Configure git to use tracked .githooks directory
git config core.hooksPath .githooks

# Also symlink into .git/hooks for tool fallback compatibility
if [ -d ".git/hooks" ]; then
    ln -sf ../../.githooks/pre-commit .git/hooks/pre-commit 2>/dev/null || true
    ln -sf ../../.githooks/pre-push .git/hooks/pre-push 2>/dev/null || true
fi

echo "✔ Git hooks successfully configured (.githooks/pre-commit, .githooks/pre-push)."
echo "  Privacy guards, doc hygiene, and regression tests are now active."
