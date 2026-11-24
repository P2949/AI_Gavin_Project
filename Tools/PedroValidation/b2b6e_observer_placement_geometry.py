"""Pure placement candidates from saved Pedro world anchors, never absolute guesses.

All locations subsequently require actual Recast projection/path and ECC_Visibility.
This module may be imported outside UE for independent deterministic regression tests.
"""
from __future__ import annotations
import math

from b2b6e_visibility_geometry import valid3

RADII = (450., 650., 850., 1050.)
ANGLES = tuple(range(0, 360, 45))
MAX_RAW = 160
MAX_PROJECTED = 160


def dist3(a, b):
    aa, bb = valid3(a), valid3(b)
    return math.sqrt(sum((x-y)**2 for x,y in zip(aa,bb)))


def offsets_from_sources(targets, bounds=()):
    """Never use world origin or manually chosen absolute map coordinates.

    Four source locations produce 128 radial wishes. Nav bounds supply at most
    27 additional wishes; *every* wish must still be projected and pathed by UE.
    """
    if len(targets) != 4 or len({t['id'] for t in targets}) != 4:
        raise ValueError('exactly four distinct real core source IDs required')
    result = []
    for target in sorted(targets, key=lambda t:t['id']):
        x, y, z = valid3(target['position'])
        for radius in RADII:
            for deg in ANGLES:
                a = math.radians(deg)
                result.append((f"{target['id']}:{int(radius)}:{deg}",
                               (x + radius*math.cos(a), y+radius*math.sin(a), z)))
    if len(bounds) > 3:
        raise ValueError('unexpected number of Pedro nav bounds')
    for idx, bound in enumerate(bounds):
        center = valid3(bound['center']); extent = valid3(bound['extent'])
        if not (extent[0] > 0 and extent[1] > 0):
            raise ValueError('invalid navigation bound')
        for fx in (-.65, 0., .65):
            for fy in (-.65, 0., .65):
                result.append((f'NavBounds{idx}:{fx}:{fy}',
                               (center[0]+fx*extent[0],center[1]+fy*extent[1],center[2])))
    if len(result) > MAX_RAW:
        raise ValueError('unexpectedly large placement search')
    return result


def admissible_projection(wish, projected, original, projected_start, min_move=250., max_move=2200.):
    """Projection must not silently jump to another floor or a distant polygon."""
    wish, projected, original, projected_start = map(valid3,(wish,projected,original,projected_start))
    snap_xy = math.hypot(wish[0]-projected[0], wish[1]-projected[1])
    snap_z = abs(wish[2]-projected[2])
    moved = math.hypot(projected[0]-projected_start[0],projected[1]-projected_start[1])
    return snap_xy <= 115. and snap_z <= 240. and min_move <= moved <= max_move


def candidate_rank(record):
    """Maximize witnessed core opportunities, then favor intact long-path proof."""
    chosen = record['sight']
    return (-chosen['visible'], -int(record['long_path_support']),
            round(record['movement_path_cm'],3),
            chosen['changed_degrees'],
            tuple(round(v,3) for v in record['root']), chosen['yaw'])


def choose(candidates):
    plausible = [x for x in candidates if 1 <= x['sight']['visible'] <= 3 and
                 x['sight']['visible']+x['sight']['hidden']==4 and
                 x['movement_path_cm']>=150.]
    return min(plausible,key=candidate_rank) if plausible else None
