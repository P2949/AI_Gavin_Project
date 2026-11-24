"""Fail-closed saved-geometry planning and authoring-scope fixtures."""
import importlib.util
import json
import math
from pathlib import Path
import tempfile
import unittest

ROOT=Path(__file__).resolve().parents[1]
def load(name):
    spec=importlib.util.spec_from_file_location(name,ROOT/(name+'.py'))
    obj=importlib.util.module_from_spec(spec)
    spec.loader.exec_module(obj)
    return obj
G=load('b2b6e_visibility_geometry')
A=load('b2b6e_visibility_audit')


def targets(angles=(-90,-20,25,110),radius=800):
    return [{'id':f'Core_{i}','name':f'Prop_{i}',
             'position':(radius*math.cos(math.radians(x)),
                         radius*math.sin(math.radians(x)),0.0)}
            for i,x in enumerate(angles)]

class Geometry(unittest.TestCase):
    def plan(self,items=None,ray=None,**kw):
        return G.plan_rotation((0,0,0),(0,0,64),180,0,180,
                               items or targets(),1500,75,
                               ray or (lambda eye,point,target:(True,'')),**kw)
    def test_real_rotation_finds_visible_and_hidden(self):
        p=self.plan()
        self.assertIsNotNone(p['chosen'])
        self.assertGreaterEqual(p['chosen']['visible'],1)
        self.assertGreaterEqual(p['chosen']['hidden'],1)
        self.assertTrue(p['needs_authoring'])
        self.assertLessEqual(p['candidates_considered'],40)
    def test_no_fabricated_sight_through_cover(self):
        p=self.plan(ray=lambda eye,point,target:(False,'ActualWall'))
        self.assertIsNone(p['chosen'])
        self.assertEqual(p['best_possible_visible'],0)
    def test_no_artificial_hidden_when_all_sources_coincident(self):
        p=self.plan(items=targets((0,0,0,0)))
        self.assertIsNone(p['chosen'])
    def test_a_legitimate_occluded_source_remains_hidden(self):
        p=self.plan(ray=lambda eye,point,t:(t['id']!='Core_3','Wall' if t['id']=='Core_3' else ''))
        self.assertIsNotNone(p['chosen'])
        self.assertNotEqual(
            next(r['status'] for r in p['chosen']['sources'] if r['id']=='Core_3'),
            'Visible')
    def test_hypothetical_eye_rotates_about_physical_root(self):
        pos=G.hypot_eye((100,200,300),(130,200,364),90)
        self.assertAlmostEqual(pos[0],100)
        self.assertAlmostEqual(pos[1],230)
        self.assertEqual(pos[2],364)
    def test_range_and_viewcone_same_geometry(self):
        self.assertEqual(G.geometry_status((0,0,0),0,0,(2000,0,0),1500,75)[0],'TooFar')
        self.assertEqual(G.geometry_status((0,0,0),0,0,(-500,0,0),1500,75)[0],'OutsideViewCone')
        self.assertEqual(G.geometry_status((0,0,0),0,0,(500,0,0),1500,75)[0],'Visible')
    def test_nonfinite_invalid_and_duplicate_source_ids_rejected(self):
        with self.assertRaises(ValueError):G.hypot_eye((0,0,0),(float('nan'),0,0),90)
        wrong=targets();wrong[0]['id']=wrong[1]['id']
        with self.assertRaises(ValueError):self.plan(items=wrong)
    def test_no_candidate_beyond_distance(self):
        p=self.plan(items=targets(radius=2200))
        self.assertIsNone(p['chosen'])
        self.assertEqual(p['best_possible_visible'],0)

class Audit(unittest.TestCase):
    def setUp(self):
        self.tmp=tempfile.TemporaryDirectory()
        self.root=Path(self.tmp.name)
        self.root.mkdir(exist_ok=True)
        self.before={f'Tools/Existing/f{i}':f'{i:064x}' for i in range(620)}
        self.map=A.MAPS['test']
        self.before[self.map]='a'*64
        self.after=dict(self.before)
        self.info={'status':'PASS','mode':'probe-test','authoring':'NONE_READ_ONLY',
                   'rotation_needed':True,'candidate':{'visible':2,'hidden':2}}
    def tearDown(self):self.tmp.cleanup()
    def write(self,mode,changed=False,extra=False):
        v=dict(self.info);v['mode']=mode
        if changed:
            self.after[self.map]='b'*64
            v['authoring']='ROTATION_ONLY_SAVED'
            v['map_sha256']='a'*64
            v['saved_map_sha256']='b'*64
        if extra:
            self.after['Content/Shared/DontTouch.umap']='c'*64
        def manifest(d):return ''.join(h+'  '+p+'\n' for p,h in sorted(d.items()))
        (self.root/'files-before.sha256').write_text(manifest(self.before))
        (self.root/'files-after.sha256').write_text(manifest(self.after))
        old=b'A  '+self.map.encode()+b'\0'
        new=(b'AM ' if changed else b'A  ')+self.map.encode()+b'\0'
        (self.root/'status-before.txt').write_bytes(old)
        (self.root/'status-after.txt').write_bytes(new)
        (self.root/'results.json').write_text(json.dumps(v))
    def test_read_only_probe_exact_status_and_hashes(self):
        self.write('probe-test')
        self.assertEqual(A.compare(self.root,'probe-test')['binary_changes'],[])
    def test_new_staged_test_map_may_become_AM_before_exact_stage(self):
        self.write('align-test',changed=True)
        self.assertEqual(A.compare(self.root,'align-test')['binary_changes'],[self.map])
    def test_existing_modified_production_may_become_MM(self):
        self.map=A.MAPS['production']
        self.before[self.map]='a'*64
        self.write('align-production',changed=True)
        (self.root/'status-before.txt').write_bytes(b'M  '+self.map.encode()+b'\0')
        (self.root/'status-after.txt').write_bytes(b'MM '+self.map.encode()+b'\0')
        self.assertEqual(A.compare(self.root,'align-production')['binary_changes'],[self.map])
    def test_other_binary_change_rejected(self):
        self.write('align-test',changed=True,extra=True)
        with self.assertRaises(RuntimeError):A.compare(self.root,'align-test')
    def test_probe_writes_rejected(self):
        self.write('probe-test',changed=True)
        with self.assertRaises(RuntimeError):A.compare(self.root,'probe-test')
    def test_forged_zero_visible_rejected(self):
        self.write('probe-test')
        p=self.root/'results.json';d=json.loads(p.read_text());d['candidate']['visible']=0;p.write_text(json.dumps(d))
        with self.assertRaises(RuntimeError):A.compare(self.root,'probe-test')
    def test_changed_unrelated_git_status_rejected(self):
        self.write('align-test',changed=True)
        p=self.root/'status-after.txt';p.write_bytes(p.read_bytes()+b'?? unrelated.txt\0')
        with self.assertRaises(RuntimeError):A.compare(self.root,'align-test')

class Source(unittest.TestCase):
    def test_editor_does_not_modify_props_or_inject_evidence(self):
        text=(ROOT/'b2b6e_visibility_editor.py').read_text()
        self.assertIn('get_actor_eyes_view_point()',text)
        self.assertIn('line_trace_single(',text)
        self.assertIn('set_actor_rotation(',text)
        self.assertIn('save_map(',text)
        self.assertIn("post_visible==candidate['visible']",text)
        for forbidden in ('ReportConfirmedAction(', 'RecordEvidence(', 'ResolveEncounter(',
                          'set_actor_location(', 'spawn_actor_from_class(', 'set_editor_property('):
            self.assertNotIn(forbidden,text)
    def test_runner_only_stages_exact_map(self):
        text=(ROOT/'b2b6e_visibility_run.sh').read_text()
        self.assertIn('git add -- "$MAPFILE"',text)
        self.assertIn('pedro_lock',text)
        self.assertIn('b2b6a.lock',(ROOT/'runner_common.sh').read_text())
        self.assertIn('b2b6e_visibility_audit.py compare',text)
        self.assertNotIn('git clean',text)
        self.assertNotIn('git reset',text)
        self.assertNotIn('git add .',text)

if __name__=='__main__':unittest.main()
