"""Regression: report diagnostics must expose real failures without editing a map."""
from __future__ import annotations
import contextlib
import hashlib
import importlib.util
import io
import json
from pathlib import Path
import sys
import tempfile
import unittest

TOOLS = Path(__file__).resolve().parents[1]
def module(name):
    spec=importlib.util.spec_from_file_location(name,TOOLS/(name+'.py'))
    m=importlib.util.module_from_spec(spec)
    spec.loader.exec_module(m)
    return m
DIAG=module('b2b6e_visibility_diagnose')
AUDIT=module('b2b6e_visibility_audit')

class ReportFixture(unittest.TestCase):
    def setUp(self):
        self.temp=tempfile.TemporaryDirectory(prefix='pedro-vis-diag-')
        self.addCleanup(self.temp.cleanup)
        self.project=Path(self.temp.name)/'project'
        self.project.mkdir()
        (self.project/'AI_Gavin_Project.uproject').write_text('{}\n')
        self.base=self.project/'Saved/PedroValidation'
        self.base.mkdir(parents=True)
        self.run=self.base/'b2b6e-visibility-probe-test-abc'
        self.run.mkdir()
        self.payload={'status':'BLOCKED','mode':'probe-test',
            'reason':'NoRotationOnlyVisibleAndHiddenCore','best_visible_count':0,
            'evaluated_yaws':23,'original_sight':{'sources':[
                {'id':'Inspect_Crate','status':'Occluded','blocker':'Wall_01'},
                {'id':'Inspect_Radio','status':'OutsideViewCone'},
            ]},'candidate':None}
        self.update()
        (self.run/'editor-exit.txt').write_text('0\n')
        (self.run/'editor.log').write_text('LogPython: Error: PEDRO_VISIBILITY status=BLOCKED reason=NoRotationOnlyVisibleAndHiddenCore\n')
        (self.run/'terminal.log').write_text('LogPython: Error: RuntimeError: blocked\n')
        data=hashlib.sha256(b'actor').hexdigest()+'  Content/Map.umap\n'
        (self.run/'files-before.sha256').write_text(data)
        (self.run/'files-after.sha256').write_text(data)
        (self.run/'status-before.txt').write_bytes(b'A  Tools/a\0')
        (self.run/'status-after.txt').write_bytes(b'A  Tools/a\0')
        (self.base/'b2b6e-visibility-probe-test-latest.txt').write_text(str(self.run)+'\n')
    def update(self):
        (self.run/'results.json').write_text(json.dumps(self.payload))
    def describe(self):
        return DIAG.describe(self.run,self.base,'probe-test')
    def test_blocked_reports_clear_reason_and_geometry(self):
        result=self.describe()
        self.assertEqual(result['unreal_status'],'BLOCKED')
        self.assertEqual(result['best_possible_visible'],0)
        self.assertEqual(result['candidate_count'],23)
        self.assertEqual(result['original_sight'][0]['blocker'],'Wall_01')
        self.assertEqual(result['next_gate'],'NO_MAP_EDIT__REVIEW_MEASURED_GEOMETRY')
        self.assertEqual(result['saved_file_changes'],[])
        self.assertTrue(result['git_status_unchanged'])
        self.assertEqual(result['editor_exit_code'],0)
    def test_script_exception_is_distinct_from_blocked_geometry(self):
        self.payload.update(status='FAIL',reason='Object of type X has no get_editor_property',
                            traceback='Traceback\nAttributeError: test')
        self.update()
        result=self.describe()
        self.assertEqual(result['next_gate'],'NO_MAP_EDIT__DEBUG_EDITOR_OR_SCRIPT')
        self.assertIn('AttributeError',result['error_excerpt'])
    def test_missing_result_is_not_reported_as_success(self):
        (self.run/'results.json').unlink()
        result=self.describe()
        self.assertEqual(result['unreal_status'],'MISSING_RESULTS')
        self.assertEqual(result['next_gate'],'NO_MAP_EDIT__DEBUG_EDITOR_OR_SCRIPT')
    def test_modified_map_is_reported(self):
        (self.run/'files-after.sha256').write_text(hashlib.sha256(b'changed').hexdigest()+'  Content/Map.umap\n')
        self.assertEqual(self.describe()['saved_file_changes'],['Content/Map.umap'])
    def test_changed_index_is_reported(self):
        (self.run/'status-after.txt').write_bytes(b'AM Tools/a\0')
        self.assertFalse(self.describe()['git_status_unchanged'])
    def test_refuse_outside_report(self):
        outside=Path(self.temp.name)/'elsewhere'
        outside.mkdir()
        with self.assertRaises(ValueError):DIAG.resolve_child(self.base,outside)
    def test_refuse_symlinked_results(self):
        p=self.run/'results.json';p.unlink();p.symlink_to(self.run/'editor.log')
        with self.assertRaises(ValueError):self.describe()
    def test_wrong_mode_refused(self):
        with self.assertRaises(ValueError):DIAG.describe(self.run,self.base,'probe-production')
    def test_unknown_output_does_not_grant_authoring(self):
        self.payload.update(status='PASS',reason='',authoring='NONE_READ_ONLY',candidate={'yaw':30},rotation_needed=True)
        self.update()
        self.assertIn('RUN_INDEPENDENT_AUDIT',self.describe()['next_gate'])
    def test_audit_prints_block_reason(self):
        # It must explain a failed Unreal outcome instead of the old generic error.
        for fname in ['files-before.sha256','files-after.sha256']:
            (self.run/fname).write_text(''.join(hashlib.sha256(str(i).encode()).hexdigest()+'  Tools/file_%03d\n'%i for i in range(614)))
        output=io.StringIO()
        with contextlib.redirect_stdout(output),self.assertRaises(RuntimeError):
            AUDIT.compare(self.run,'probe-test')
        self.assertIn('NoRotationOnlyVisibleAndHiddenCore',output.getvalue())
    def test_latest_pointer_must_resolve_inside_saved(self):
        self.assertEqual(DIAG.report_dirs(self.base,['probe-test']),[(self.run,'probe-test')])
        (self.base/'b2b6e-visibility-probe-test-latest.txt').write_text(str(Path(self.temp.name))+'\n')
        with self.assertRaises(ValueError):DIAG.report_dirs(self.base,['probe-test'])
    def test_malformed_manifest_fails_closed(self):
        (self.run/'files-after.sha256').write_text('garbage\n')
        with self.assertRaises(ValueError):self.describe()

if __name__=='__main__':unittest.main()
