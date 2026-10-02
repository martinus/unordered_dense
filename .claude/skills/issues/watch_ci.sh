#!/usr/bin/env bash
# Waits until every workflow run on PR N's current head commit has finished, then prints the
# check tally and the head. A loop over `gh pr checks` alone exits at once right after a push:
# there are no checks yet, or it still lists the previous head's (on 2026-10-02 one exited
# before any check existed).
#
#   .claude/skills/issues/watch_ci.sh N      # run_in_background; 60 minutes at most
set -u
n=$1
head=$(gh pr view "$n" --json headRefOid -q .headRefOid)
for _ in $(seq 1 60); do
    total=$(gh run list --commit "$head" --json status -q 'length')
    open=$(gh run list --commit "$head" --json status -q '[.[] | select(.status != "completed")] | length')
    if [ "${total:-0}" -gt 0 ] && [ "${open:-1}" = 0 ]; then
        gh pr checks "$n" | awk -F'\t' '{print $2}' | sort | uniq -c | awk '{printf "%s=%s ", $2, $1}'
        echo "head ${head:0:7}"
        exit 0
    fi
    sleep 60
done
echo "timeout, head ${head:0:7}"
exit 1
