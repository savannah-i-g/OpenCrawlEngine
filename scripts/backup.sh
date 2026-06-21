#!/usr/bin/env bash
# Write a timestamped snapshot of the working tree to the backup directory,
# excluding build output, local reference material, and VCS metadata.
set -euo pipefail

project_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
project_name="$(basename "$project_root")"
parent_dir="$(dirname "$project_root")"
backup_dir="${HOME}/Documents/${project_name}_Backups"
stamp="$(date +%Y%m%d_%H%M%S)"
archive="${backup_dir}/${project_name}_${stamp}.tar.gz"

mkdir -p "$backup_dir"
tar -czf "$archive" -C "$parent_dir" \
    --exclude="${project_name}/build" \
    --exclude="${project_name}/build-*" \
    --exclude="${project_name}/Reference" \
    --exclude="${project_name}/.git" \
    "$project_name"

echo "Backup written: $archive"
