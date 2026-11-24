"""Exercise the shared Bash helpers without starting Unreal or using Nix."""
import json
import os
from pathlib import Path
import subprocess
import tempfile
import unittest

COMMON = Path(__file__).resolve().parents[1] / 'runner_common.sh'


class RunnerCommonTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix='pedro runner ')
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        self.project = self.root / 'project'
        self.project.mkdir()
        self.engine = self.root / 'engine'
        self.editor = self.engine / 'Engine/Binaries/Linux/UnrealEditor'
        self.editor.parent.mkdir(parents=True)
        self.editor.write_text('#!/bin/bash\nexit 0\n')
        self.editor.chmod(0o755)
        self.descriptor = self.project / 'AI_Gavin_Project.uproject'
        self.descriptor.write_text('{"Plugins": [{"Name":"PythonScriptPlugin","Enabled":false}]}\n')
        self.env = dict(os.environ, PROJECT=str(self.project), ENGINE=str(self.engine),
                        COMMON=str(COMMON))

    def shell(self, code):
        return subprocess.run(['bash', '-c', 'set -euo pipefail\nsource "$COMMON"\n' + code],
                              env=self.env, cwd=self.root, capture_output=True, text=True)

    def ok(self, code):
        result = self.shell(code)
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)

    def test_paths_with_spaces_and_required_build(self):
        self.ok('pedro_init; [[ "$PWD" == "$PROJECT" && -x "$UE" ]]')
        self.assertNotEqual(self.shell('pedro_init build').returncode, 0)
        self.editor.unlink()
        self.assertNotEqual(self.shell('pedro_init').returncode, 0)

    def test_fresh_directory_and_exclusive_lock(self):
        self.ok('''pedro_init
pgrep() { return 1; }
pedro_lock
[[ -f Saved/PedroValidation/b2b6a.lock ]]
if flock -n Saved/PedroValidation/b2b6a.lock true 9>&-; then exit 42; fi
NIX=(env)
run_nix bash -c '[[ ! -e /proc/self/fd/9 ]]'
''')

    def test_active_or_unknown_process_refused(self):
        for rc in (0, 2, 3):
            with self.subTest(rc=rc):
                result = self.shell(f'pedro_init; pgrep() {{ return {rc}; }}; pedro_lock')
                self.assertNotEqual(result.returncode, 0)

    def test_descriptor_enable_preserves_other_fields(self):
        self.ok('pedro_init; NIX=(env); enable_editor_plugins; enable_editor_plugins')
        plugins = json.loads(self.descriptor.read_text())['Plugins']
        self.assertEqual(plugins, [{'Name': 'PythonScriptPlugin', 'Enabled': True},
                                  {'Name': 'EditorScriptingUtilities', 'Enabled': True,
                                   'TargetAllowList': ['Editor']}])

    def test_duplicate_descriptor_refused_without_write(self):
        original = b'{"Plugins":[{"Name":"EditorScriptingUtilities"},{"Name":"EditorScriptingUtilities"}]}\n'
        self.descriptor.write_bytes(original)
        self.assertNotEqual(self.shell('pedro_init; NIX=(env); enable_editor_plugins').returncode, 0)
        self.assertEqual(self.descriptor.read_bytes(), original)

    def test_fatal_and_unreadable_logs_fail(self):
        log = self.root / 'editor.log'
        log.write_text('Normal exit\n')
        self.ok('check_fatal editor.log')
        log.write_text('Normal exit\nFatal error: late crash\n')
        self.assertNotEqual(self.shell('check_fatal editor.log').returncode, 0)
        log.unlink()
        self.assertNotEqual(self.shell('check_fatal editor.log').returncode, 0)

    def test_snapshot_comparison_requires_both_files(self):
        for name in ('status-before.txt', 'status-after.txt',
                     'files-before.sha256', 'files-after.sha256'):
            (self.root / name).write_text('same\n')
        self.ok('verify_readonly_snapshot .')
        (self.root / 'files-after.sha256').write_text('different\n')
        self.assertNotEqual(self.shell('verify_readonly_snapshot .').returncode, 0)
        self.assertIn('different', (self.root / 'file-differences.txt').read_text())
        (self.root / 'files-after.sha256').write_text('same\n')
        (self.root / 'status-after.txt').unlink()
        self.assertNotEqual(self.shell('verify_readonly_snapshot .').returncode, 0)

    def test_dispatch_preserves_modes_and_excludes_authoring(self):
        # Run the real entry point against harmless stand-ins for its children.
        # No UE, Nix or actual repository is involved in these assertions.
        import shutil
        entry = self.root / 'validate.sh'
        shutil.copyfile(COMMON.with_name('validate.sh'), entry)
        for name in ('b2b6a_run.sh', 'b2b6f_flow_run.sh', 'b2b6e_interaction_run.sh',
                     'b2b6e_editor.sh', 'b2b5b2_run.sh'):
            (self.root / name).write_text('printf "%s\\n" "${0##*/}" "$@"\n')
        cases = [(('native',), ['b2b6a_run.sh', 'unit']),
                 (('world', 'all'), ['b2b6a_run.sh', 'all']),
                 (('flow', 'production-completion-rendered'),
                  ['b2b6f_flow_run.sh', 'production-completion-rendered']),
                 (('interaction', 'test'), ['b2b6e_interaction_run.sh', 'test']),
                 (('maps', 'verify-production'), ['b2b6e_editor.sh', 'verify-production']),
                 (('legacy-map', 'verify'), ['b2b5b2_run.sh', 'verify'])]
        for args, expected in cases:
            with self.subTest(args=args):
                result = subprocess.run(['bash', str(entry), *args], capture_output=True, text=True)
                self.assertEqual(result.returncode, 0, result.stderr)
                self.assertEqual(result.stdout.splitlines(), expected)
        for args in ((), ('unknown',), ('maps',), ('maps', 'author-production')):
            with self.subTest(args=args):
                result = subprocess.run(['bash', str(entry), *args], capture_output=True, text=True)
                self.assertEqual(result.returncode, 2)
                self.assertEqual(result.stdout, '')
