"""Offline regression tests: no Unreal runtime required."""
from __future__ import annotations
import hashlib
import json
from pathlib import Path
import os
import subprocess
import sys
import tempfile
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from b2b6e_integrity import (read_hashes, compare_manifests, run_snapshot_diff,
                              reconcile, CACHE_RE)

MODULES = ('b2b6e_contract', 'b2b6e_editor_common')


def sha(data):
    return hashlib.sha256(data).hexdigest()


def cmd(project, *args):
    return subprocess.check_output(['git', '-C', str(project), *args])


class SnapshotIntegrity(unittest.TestCase):
    def test_added_bytecode_is_explicit_not_ignored(self):
        added={f'Tools/PedroValidation/__pycache__/{m}.cpython-311.pyc':sha(m.encode())
               for m in MODULES}
        d=compare_manifests({'Source/A.cpp':sha(b'old')},
                            {'Source/A.cpp':sha(b'old'), **added})
        self.assertEqual(len(d['added']), 2)
        self.assertEqual(d['modified'], [])
        self.assertTrue(all(CACHE_RE.fullmatch(s) for s in d['added']))

    def test_modified_tracked_source_is_not_cache_only(self):
        d=compare_manifests({'Source/A.cpp':sha(b'old')}, {'Source/A.cpp':sha(b'new')})
        self.assertEqual(d['modified'], ['Source/A.cpp'])

    def test_malformed_or_unsafe_snapshot_rejected(self):
        with tempfile.TemporaryDirectory() as t:
            f=Path(t)/'bad'
            for value in ('not-a-hash  foo\n', f'{sha(b"x")}  ../outside\n',
                          f'{sha(b"x")}  /tmp/outside\n'):
                f.write_text(value)
                with self.assertRaises(ValueError):read_hashes(f)

    def test_no_implicit_classification_of_other_untracked_file(self):
        for name in ('b2b6e_author_level', 'other_module', 'b2b6e_saved_map_audit'):
            self.assertIsNone(CACHE_RE.fullmatch(
                'Tools/PedroValidation/__pycache__/'+name+'.cpython-311.pyc'))


class ReconcileFixture(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.addCleanup(self.tmp.cleanup)
        self.root=Path(self.tmp.name)
        self.repo=self.root/'project'
        self.repo.mkdir()
        subprocess.run(['git','init','-q',str(self.repo)],check=True)
        (self.repo/'AI_Gavin_Project.uproject').write_text('{}')
        (self.repo/'.gitignore').write_text('Saved/\n')
        cmd(self.repo, 'add','AI_Gavin_Project.uproject','.gitignore')
        subprocess.run(['git','-C',str(self.repo), '-c','user.name=Test',
                        '-c','user.email=test@example.invalid','commit','-qm','baseline'],check=True)
        self.report=self.repo/'Saved/PedroValidation/b2b6e-probe-FAKE0001'
        self.report.mkdir(parents=True)
        tracked=[p for p in ('.gitignore','AI_Gavin_Project.uproject')]
        self.before={name:sha((self.repo/name).read_bytes()) for name in tracked}
        self.before_status=cmd(self.repo,'status','--porcelain=v1','-z','--untracked-files=all')
        self.created=[]
        for name in MODULES:
            p=self.repo/'Tools/PedroValidation/__pycache__'/f'{name}.cpython-311.pyc'
            p.parent.mkdir(parents=True,exist_ok=True)
            p.write_bytes((name+'-fake-cache').encode())
            self.created.append(p)
        self.after={**self.before,**{str(p.relative_to(self.repo)):sha(p.read_bytes())
                                    for p in self.created}}
        self.after_status=cmd(self.repo,'status','--porcelain=v1','-z','--untracked-files=all')
        self._write_snapshots()

    def _write_snapshots(self):
        for key,vals in (('before',self.before),('after',self.after)):
            (self.report/f'files-{key}.sha256').write_text(''.join(
                f'{v}  {p}\n' for p,v in sorted(vals.items())))
        (self.report/'status-before.txt').write_bytes(self.before_status)
        (self.report/'status-after.txt').write_bytes(self.after_status)

    def test_preview_and_atomic_quarantine(self):
        preview=reconcile(self.repo,self.report,False)
        self.assertTrue(preview['recovery_permitted'])
        self.assertTrue(all(p.is_file() for p in self.created))
        result=reconcile(self.repo,self.report,True)
        self.assertTrue(result['reconciled_to_before_snapshot'])
        self.assertTrue(all(not p.exists() for p in self.created))
        self.assertEqual(cmd(self.repo,'status','--porcelain=v1','-z','--untracked-files=all'),
                         self.before_status)
        for p in self.created:
            q=self.report/'quarantined-bytecode'/p.relative_to(self.repo)
            self.assertTrue(q.is_file())
            self.assertEqual(sha(q.read_bytes()),self.after[str(p.relative_to(self.repo))])

    def test_report_unchanged_bytes_required(self):
        self.created[0].write_bytes(b'changed')
        with self.assertRaisesRegex(ValueError, 'changed since probe'):
            reconcile(self.repo,self.report,True)
        self.assertTrue(all(p.exists() for p in self.created))

    def test_refuses_unrelated_added_file(self):
        p=self.repo/'Content/Shared/SomeAsset.uasset'
        p.parent.mkdir(parents=True)
        p.write_bytes(b'asset')
        self.after[str(p.relative_to(self.repo))]=sha(p.read_bytes())
        self.after_status=cmd(self.repo,'status','--porcelain=v1','-z','--untracked-files=all')
        self._write_snapshots()
        with self.assertRaisesRegex(ValueError, 'not exactly two'):
            reconcile(self.repo,self.report,True)
        self.assertTrue(p.is_file())

    def test_refuses_nonmatching_status(self):
        (self.repo/'UNRELATED.txt').write_text('stop')
        with self.assertRaisesRegex(ValueError, 'status differs'):
            reconcile(self.repo,self.report,True)
        self.assertTrue(all(p.is_file() for p in self.created))

    def test_refuses_tracked_source_change(self):
        (self.repo/'AI_Gavin_Project.uproject').write_text('changed')
        with self.assertRaisesRegex(ValueError, 'unstaged tracked'):
            reconcile(self.repo,self.report,True)
        self.assertTrue(all(p.is_file() for p in self.created))


class NoEditorCache(unittest.TestCase):
    def test_guard_precedes_local_imports(self):
        base=Path(__file__).resolve().parents[1]
        for path in ('b2b6e_author_level.py','b2b6e_saved_map_audit.py'):
            with self.subTest(script=path):
                text=(base/path).read_text()
                self.assertLess(text.index('sys.dont_write_bytecode = True'),
                                text.index('from b2b6e_contract import'))
                self.assertLess(text.index('sys.pycache_prefix ='),
                                text.index('from b2b6e_contract import'))
                self.assertIn('B2B6E_PYCACHE_PREFIX',text)

    def test_editor_launcher_redirects_under_saved(self):
        text=(Path(__file__).resolve().parents[1]/'b2b6e_editor.sh').read_text()
        self.assertIn('PYTHONPYCACHEPREFIX="$RUN/python-cache"',text)
        self.assertIn('B2B6E_PYCACHE_PREFIX="$RUN/python-cache"',text)
        self.assertIn('b2b6e_integrity.py snapshots "$RUN"',text)


if __name__=='__main__':unittest.main()
