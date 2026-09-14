#!/usr/bin/env python3
"""Require solver/audit WDL identity, in encoded side-to-move order.

Conservation alone cannot detect swapped force roles. Compare the admitted
counts before trivial filtering, separately for each side and each outcome.
Never try an alternative W/L permutation to make this gate pass.
"""
import re

SOLVER = re.compile(
    r'^information_summary side ([01]) win (\d+) loss (\d+) draw (\d+) '
    r'unreachable_win \d+ unreachable_loss \d+ unreachable_draw \d+ '
    r'sets \d+ concrete \d+ bellman_residual 0 rank_residual 0 '
    r'belief_cap none exhaustive 1$', re.M)
AUDIT = re.compile(
    r'^information_reachability_admitted side ([01]) unknown 0 '
    r'win (\d+) loss (\d+) draw (\d+)$', re.M)


def side_counts(pattern, text, label):
    rows = pattern.findall(text)
    if len(rows) != 2 or {row[0] for row in rows} != {'0', '1'}:
        raise ValueError(f'{label}: missing, duplicate, or uncertified WDL side counts')
    return {side: dict(zip(('win', 'loss', 'draw'), map(int, values)))
            for side, *values in rows}


def validate_information_outcomes(solver_log, audit_log, filename):
    solver = side_counts(SOLVER, solver_log, filename + ' solver')
    audited = side_counts(AUDIT, audit_log, filename + ' audit')
    for side in ('0', '1'):
        if solver[side] != audited[side]:
            raise ValueError(
                f'{filename}: solver/audit WDL orientation residual side {side}: '
                f'solver={solver[side]} audit={audited[side]}')
    return dict(semantics='encoded-side-to-move-admitted-before-trivial-v1',
                solver=solver, audit=audited, orientation_residual=0)
