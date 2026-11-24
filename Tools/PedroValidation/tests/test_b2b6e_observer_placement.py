"""Pure and source-contract regression for measured Pedro relocation."""
from __future__ import annotations
import importlib
from pathlib import Path
import sys
import tempfile
import unittest

ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT))
if not (ROOT/'b2b6e_visibility_geometry.py').exists():
    sys.path.insert(0,str(ROOT.parents[1]/'selftests/fixtures'))
from b2b6e_observer_placement_geometry import (RADII,ANGLES,MAX_RAW,MAX_PROJECTED,
    offsets_from_sources,admissible_projection,candidate_rank,choose,dist3)
from b2b6e_observer_placement_audit import audit,hashes,statuses

TARGETS=[{'id':x,'position':p} for x,p in [
    ('Inspect_Crate',(1000.,0.,0.)),('Inspect_Lantern',(0.,2000.,0.)),
    ('Inspect_Radio',(1800.,0.,0.)),('Inspect_Statue',(-1000.,0.,0.))]]

class CandidateTests(unittest.TestCase):
    def test_uses_only_saved_source_positions(self):
        got=offsets_from_sources(TARGETS)
        self.assertEqual(len(got),128)
        self.assertEqual(got[0][1],(1450.,0.,0.))
        self.assertEqual(got[-1][0],'Inspect_Statue:1050:315')
    def test_bound_samples_are_relative(self):
        got=offsets_from_sources(TARGETS,[{'center':(800.,900.,50.),'extent':(500.,600.,400.)}])
        self.assertEqual(len(got),137)
        self.assertEqual(got[128][1],(475.,510.,50.))
    def test_duplicate_core_rejected(self):
        with self.assertRaises(ValueError):offsets_from_sources(TARGETS[:3]+[TARGETS[2]])
    def test_missing_core_rejected(self):
        with self.assertRaises(ValueError):offsets_from_sources(TARGETS[:3])
    def test_nan_rejected(self):
        with self.assertRaises(ValueError):offsets_from_sources(TARGETS[:3]+[{'id':'X','position':(float('nan'),1,2)}])
    def test_nav_bounds_overflow_refused(self):
        with self.assertRaises(ValueError):offsets_from_sources(TARGETS,[{'center':(1,2,3),'extent':(100,100,100)}]*4)
    def test_zero_bounds_refused(self):
        with self.assertRaises(ValueError):offsets_from_sources(TARGETS,[{'center':(1,2,3),'extent':(0,100,100)}])
    def test_projection_is_distance_and_floor_bounded(self):
        self.assertTrue(admissible_projection((1000,0,0),(980,0,-50),(0,0,0),(0,0,0)))
        self.assertFalse(admissible_projection((1000,0,0),(840,0,0),(0,0,0),(0,0,0)))
        self.assertFalse(admissible_projection((1000,0,0),(1000,0,-260),(0,0,0),(0,0,0)))
    def test_refuse_stationary_and_far(self):
        self.assertFalse(admissible_projection((100,0,0),(100,0,0),(0,0,0),(0,0,0)))
        self.assertFalse(admissible_projection((2500,0,0),(2500,0,0),(0,0,0),(0,0,0)))
    def test_distance(self):
        self.assertAlmostEqual(dist3((0,0,0),(3,4,12)),13)
    def test_maximize_visible_over_shorter_move(self):
        a={'sight':{'visible':1,'hidden':3,'changed_degrees':0,'yaw':0},'movement_path_cm':200.,'long_path_support':False,'root':(1,2,3)}
        b={'sight':{'visible':2,'hidden':2,'changed_degrees':90,'yaw':90},'movement_path_cm':700.,'long_path_support':False,'root':(1,2,3)}
        self.assertEqual(choose([a,b]),b)
    def test_reject_fabricated_four_visible(self):
        a={'sight':{'visible':4,'hidden':0,'changed_degrees':0,'yaw':0},'movement_path_cm':200.,'long_path_support':False,'root':(1,2,3)}
        self.assertIsNone(choose([a]))
    def test_long_path_wins_tie(self):
        a={'sight':{'visible':2,'hidden':2,'changed_degrees':0,'yaw':0},'movement_path_cm':250.,'long_path_support':False,'root':(1,2,3)}
        b={'sight':{'visible':2,'hidden':2,'changed_degrees':10,'yaw':10},'movement_path_cm':950.,'long_path_support':True,'root':(2,3,4)}
        self.assertEqual(choose([a,b]),b)
    def test_reject_zero_path(self):
        a={'sight':{'visible':1,'hidden':3,'changed_degrees':0,'yaw':0},'movement_path_cm':0.,'long_path_support':False,'root':(1,2,3)}
        self.assertIsNone(choose([a]))

class AuditTests(unittest.TestCase):
    def fixture(self,kind='probe'):
        from b2b6e_observer_placement_audit import MAPS
        d=tempfile.TemporaryDirectory();self.addCleanup(d.cleanup)
        p=Path(d.name);old={'file%03d.txt'%i:'0'*64 for i in range(625)}
        target=MAPS['test'];old[target]='a'*64
        new=old.copy()
        if kind=='apply':new[target]='b'*64
        for name,dct in (('before',old),('after',new)):
            (p/f'files-{name}.sha256').write_text(''.join(f'{h}  {s}\n' for s,h in sorted(dct.items())))
        codes={target:b'A ', 'abc.txt':b'M '}
        before=b''.join(k+ b' '+n.encode()+b'\0' for n,k in codes.items())
        after=(b'AM '+target.encode()+b'\0M  abc.txt\0') if kind=='apply' else before
        (p/'status-before.txt').write_bytes(before)
        (p/'status-after.txt').write_bytes(after)
        candidate={'movement_path_cm':900,'root':[800,700,10],
            'sight':{'visible':1,'hidden':3,'sources':[{'id':'A','status':'Visible'}]*4}}
        result={'status':'PASS','mode':kind+'-test','candidate':candidate,'map_sha256':'a'*64,
            'authoring':'NONE_READ_ONLY' if kind=='probe' else 'OBSERVER_ROOT_AND_YAW_ONLY_SAVED',
            'saved_map_sha256':'b'*64}
        import json
        (p/'results.json').write_text(json.dumps(result))
        return p
    def test_valid_readonly(self):
        self.assertEqual(audit(self.fixture(), 'probe-test')['status'],'PASS')
    def test_probe_write_refused(self):
        p=self.fixture('apply')
        with self.assertRaises(ValueError):audit(p,'probe-test')
    def test_valid_one_map_apply(self):
        self.assertEqual(audit(self.fixture('apply'),'apply-test')['changed_assets'],
            ['Content/AI_Gavin/Encounters/Pedro/Validation/B2b6e/L_Pedro_AutonomousTest.umap'])
    def test_unrelated_file_refused(self):
        p=self.fixture('apply');f=p/'files-after.sha256'
        f.write_text(f.read_text().replace('0'*64+'  file005.txt','c'*64+'  file005.txt'))
        with self.assertRaises(ValueError):audit(p,'apply-test')
    def test_false_visible_refused(self):
        p=self.fixture()
        import json
        f=p/'results.json';o=json.loads(f.read_text());o['candidate']['sight']['visible']=0;o['candidate']['sight']['hidden']=4;f.write_text(json.dumps(o))
        with self.assertRaises(ValueError):audit(p,'probe-test')
    def test_unsaved_report_refused(self):
        p=self.fixture('apply')
        import json
        f=p/'results.json';o=json.loads(f.read_text());o['saved_map_sha256']='c'*64;f.write_text(json.dumps(o))
        with self.assertRaises(ValueError):audit(p,'apply-test')
    def test_unexpected_status_refused(self):
        p=self.fixture('apply');(p/'status-after.txt').write_bytes(b'AM unrelated.txt\0')
        with self.assertRaises(ValueError):audit(p,'apply-test')
    def test_bad_snapshot_path_refused(self):
        p=self.fixture();f=p/'files-before.sha256';f.write_text(f.read_text()+'a'*64+'  ../evil\n')
        with self.assertRaises(ValueError):audit(p,'probe-test')

class SourceScope(unittest.TestCase):
    def test_no_synthetic_events_or_director_completion(self):
        src=(ROOT/'b2b6e_observer_placement_editor.py').read_text()
        for bad in ('ReportConfirmedAction(', 'RecordEvidence(', 'ResolveEncounter(', 'SetAttentionEnabled(', 'SetNavigationEnabled('):
            self.assertNotIn(bad,src)
    def test_nav_and_live_sight_required(self):
        src=(ROOT/'b2b6e_observer_placement_editor.py').read_text()
        for key in ('nav_project(', 'nav_path(', 'base[\'visibility_ray\']',
                    'verify_actual(', 'compare_plans(', 'ScopedEditorTransaction', 'save_map('):
            self.assertIn(key,src)
    def test_no_binary_mutation_in_runner_before_audit(self):
        src=(ROOT/'b2b6e_observer_placement_run.sh').read_text()
        self.assertNotIn('git clean',src)
        self.assertNotIn('git reset',src)
        self.assertIn('b2b6e_observer_placement_audit.py',src)
        self.assertIn('git add -- "$MAPFILE"',src)
    def test_only_approved_binary_staged(self):
        runner=(ROOT/'b2b6e_observer_placement_run.sh').read_text()
        self.assertIn('git add -- "$MAPFILE"',runner)
        self.assertIn('pedro_lock',runner)
        self.assertIn('b2b6a.lock',(ROOT/'runner_common.sh').read_text())

if __name__=='__main__':unittest.main()
