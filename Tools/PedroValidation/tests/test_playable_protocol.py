"""B2b-6e source and report inventory regression tests (pure Python)."""
import sys
import unittest
from pathlib import Path
sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
import verify_awareness_report as verifier

class PlayableNativeInventoryTests(unittest.TestCase):
    def test_exact_new_names(self):
        self.assertEqual(len(verifier.PLAYABLE_NAMES), 7)
        self.assertEqual(len(set(verifier.PLAYABLE_NAMES)), 7)
        self.assertEqual(len(verifier.NATIVE_NAMES), 172)
        self.assertEqual(len(verifier.NATIVE_NAMES & {
            'AI_Gavin_Project.Pedro.Playable.' + name
            for name in verifier.PLAYABLE_NAMES
        }), 7)

    def test_fail_on_missing_new_test(self):
        names = sorted(verifier.NATIVE_NAMES)
        bad = [dict(fullTestPath=n, state='Success', warnings=0, errors=0) for n in names[:-1]]
        report = dict(tests=bad, succeeded=len(bad), succeededWithWarnings=0,
                      failed=0, notRun=0, inProcess=0)
        with self.assertRaises(ValueError):
            verifier.native(report)

    def test_fail_on_extra_test(self):
        names = sorted(verifier.NATIVE_NAMES) + ['AI_Gavin_Project.Pedro.FakeTest']
        bad = [dict(fullTestPath=n, state='Success', warnings=0, errors=0) for n in names]
        report = dict(tests=bad, succeeded=len(bad), succeededWithWarnings=0,
                      failed=0, notRun=0, inProcess=0)
        with self.assertRaises(ValueError):
            verifier.native(report)

if __name__ == '__main__':
    unittest.main()
