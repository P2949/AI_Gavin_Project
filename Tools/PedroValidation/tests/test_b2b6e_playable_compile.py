"""Type-safe playable smoke call contract; portable without Unreal installed.

The real Clang/UnrealBuildTool build is still the acceptance gate. These
tests catch the exact base-to-derived pointer regression before that build.
"""
from pathlib import Path
import re
import unittest

PROJECT = Path(__file__).resolve().parents[3]
SOURCE = PROJECT / 'Source/AI_Gavin_Project/Encounters/Pedro/Tests/PedroPlayableSmoke.cpp'

class SmokeObserverType(unittest.TestCase):
    def test_actual_derived_observer_pointer_passed(self):
        src = SOURCE.read_text(encoding='utf-8')
        self.assertEqual(src.count('APedroObserverCharacter* O=S.Observer.Get();'), 1)
        self.assertNotIn('ACharacter* O=S.Observer.Get();', src)
        self.assertRegex(src, r'bool FindPlayerSetupLocation\(UWorld\* W,\s*APedroObserverCharacter\* Observer,')
        self.assertRegex(src, r'FindPlayerSetupLocation\(W,O,S\.Controller\.Get\(\),S\.Player\.Get\(\),Nav,Data,')

    def test_real_gameplay_acceptance_not_bypassed(self):
        src = SOURCE.read_text(encoding='utf-8')
        for proof in (
            'Nav->FindPathSync(',
            'LineTraceSingleByChannel(',
            'OverlapBlockingTestByChannel(',
            'PedroDirectObservation::CheckGeometry(',
            'GetPedroMoveSubmissions()',
            'ObserverAttackCount',
            'Melee->TryStartAttack()',
            'PEDRO_PLAYABLE_TEST status=SUMMARY passed=6 expected=6 result=PASS',
        ):
            with self.subTest(proof=proof):
                self.assertIn(proof, src)
        self.assertNotIn('S.Observer->SetActorLocation(', src)
        self.assertNotIn('S.Observer->SetActorRotation(', src)

    def test_only_one_typed_change_can_reproduce_old_source(self):
        src = SOURCE.read_text(encoding='utf-8')
        fixed = 'APedroObserverCharacter* O=S.Observer.Get();'
        old = 'ACharacter* O=S.Observer.Get();'
        self.assertEqual(src.count(fixed), 1)
        # Structural regression check: no second observer declaration which
        # silently shadows the properly typed observer.
        self.assertEqual(len(re.findall(r'\b(?:ACharacter|APedroObserverCharacter)\s*\*\s*O\s*=',src)),1)
        self.assertEqual(src.replace(fixed, old, 1).count(old), 1)

if __name__ == '__main__':
    unittest.main()
