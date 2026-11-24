"""One source of truth for the Pedro playable-map and runner acceptance contract.

This module deliberately imports no Unreal APIs. A test map must never be
silently substituted by production, and a verification mode must not be
weakened to compensate for absent saved assets.
"""
from __future__ import annotations

from pathlib import Path

PRODUCTION = '/Game/AI_Gavin/Encounters/Pedro/Maps/L_Pedro_Prototype'
TEST = '/Game/AI_Gavin/Encounters/Pedro/Validation/B2b6e/L_Pedro_AutonomousTest'
TEST_HUB = '/Game/AI_Gavin/Encounters/Pedro/Validation/B2b6e/L_Hub_Pedro_AutonomousTest'
TEST_DATA = '/Game/AI_Gavin/Encounters/Pedro/Validation/B2b6e/DA_Encounter_Pedro_AutonomousTest'
BASELINE = '/Game/AI_Gavin/Encounters/Pedro/Validation/L_Pedro_ObserverActorTest'
BASELINE_HUB = '/Game/AI_Gavin/Encounters/Pedro/Validation/L_Hub_Pedro_ObserverActorTest'
BASELINE_DATA = '/Game/AI_Gavin/Encounters/Pedro/Validation/DA_Encounter_Pedro_ObserverActorTest'
TEST_ID = 'PedroAutonomousTest'

BASE_RUNTIME = (
    'world', 'integration', 'attention', 'attention-integration',
    'navigation', 'navigation-integration', 'context', 'context-integration',
)
COMMON = ('preflight', 'engine-inspection', 'offline-tests', 'snapshot-before')


def asset_disk_path(project: Path, package: str, suffix: str) -> Path:
    if not package.startswith('/Game/') or '..' in package or '.' in package:
        raise ValueError(f'Non-canonical asset package: {package}')
    return project / 'Content' / (package.removeprefix('/Game/') + suffix)


def expected_playable_map(mode: str, override: str | None = None) -> str:
    if mode not in ('playable', 'playable-rendered', 'playable-test', 'playable-test-rendered', 'all', 'rendered'):
        raise ValueError(f'Unsupported playable mode: {mode}')
    required = TEST if 'test' in mode else PRODUCTION
    if override is not None and override != required:
        raise ValueError(f'{mode}: only exact map {required} is accepted (got {override!r})')
    return required


def expected_stages(mode: str) -> list[str]:
    c = list(COMMON)
    r = list(BASE_RUNTIME)
    rendered = ['rendered-' + n for n in BASE_RUNTIME]
    individual = {
        'probe': [],
        'unit': ['non-unity-build', 'native'],
        'world': ['ordinary-build', 'world'],
        'integration': ['ordinary-build', 'integration'],
        'attention': ['ordinary-build', 'attention'],
        'attention-integration': ['ordinary-build', 'attention-integration'],
        'navigation-probe': ['non-unity-build', 'navigation-probe'],
        'navigation': ['ordinary-build', 'navigation'],
        'navigation-integration': ['ordinary-build', 'navigation-integration'],
        'context-probe': ['non-unity-build', 'context-probe'],
        'context': ['ordinary-build', 'context'],
        'context-integration': ['ordinary-build', 'context-integration'],
        'playable': ['ordinary-build', 'playable'],
        'playable-rendered': ['ordinary-build', 'rendered-playable'],
        'playable-test': ['ordinary-build', 'playable-test'],
        'playable-test-rendered': ['ordinary-build', 'rendered-playable-test'],
        'rendered': ['ordinary-build', *rendered, 'rendered-playable'],
        'unity': ['forced-unity-build', 'native-unity'],
        'all': ['non-unity-build', 'native', *r, 'playable', *rendered,
                'rendered-playable', 'forced-unity-build', 'native-unity'],
    }
    if mode not in individual:
        raise ValueError(f'Unknown Pedro runner mode: {mode}')
    return c + individual[mode] + ['repository-audit']


def expected_reports(mode: str) -> set[str]:
    if mode != 'all':
        raise ValueError('Exact complete report inventory is defined for all only')
    return ({'non-unity', 'unity', 'native', 'native-unity', 'playable', 'rendered-playable'}
            | set(BASE_RUNTIME)
            | {'rendered-' + x for x in BASE_RUNTIME})


def check_full_acceptance(mode: str, rc: int, completed: list[str], reports: set[str]) -> bool:
    if rc != 0 or completed != expected_stages(mode):
        return False
    return mode != 'all' or reports == expected_reports('all')
