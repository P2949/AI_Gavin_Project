"""UE 5.8.2 measured, source-preserving observer placement.

PROBE is read-only. APPLY requires that exact probe in a previous Editor process.
Changes only the physical Pedro observer actor's starting location and yaw in
one approved map, and only after independently proven nav and direct sight.
No data-source mutation, event injection, scoring, AI teleport, or hub writes.
"""
from __future__ import annotations
import hashlib
import json
import math
import os
from pathlib import Path
import sys
import traceback

sys.dont_write_bytecode = True
if os.environ.get('B2B6E_PYCACHE_PREFIX'):
    sys.pycache_prefix = os.environ['B2B6E_PYCACHE_PREFIX']
sys.path.insert(0,os.environ.get('B2B6E_SCRIPT_DIR',str(Path(__file__).resolve().parent)))

import unreal
import b2b6e_visibility_editor as original_probe
from b2b6e_editor_common import find, nav_path, nav_project, prop, save_map
from b2b6e_visibility_geometry import (angle_difference,geometry_status,plan_rotation,valid3)
from b2b6e_observer_placement_geometry import (MAX_PROJECTED,admissible_projection,
                                                candidate_rank,choose,dist3,
                                                offsets_from_sources)

MODE=os.environ.get('B2B6E_PLACE_MODE','')
REPORT=Path(os.environ.get('B2B6E_PLACE_REPORT',''))
PROBE=Path(os.environ['B2B6E_PLACE_PROBE']) if os.environ.get('B2B6E_PLACE_PROBE') else None
MODES={'probe-test','apply-test','probe-production','apply-production'}


def need(condition,why):
    if not condition:
        raise RuntimeError('PEDRO_PLACEMENT STOP: '+why)


def xyz(unreal_vector):
    return valid3((float(unreal_vector.x),float(unreal_vector.y),float(unreal_vector.z)))


def rounded3(values):
    return [round(v,4) for v in valid3(values)]


def read_probe():
    need(PROBE is not None and PROBE.is_file() and not PROBE.is_symlink(),
         'missing independent approved probe result')
    saved=(Path(os.environ['B2B6E_PROJECT'])/'Saved/PedroValidation').resolve()
    need(PROBE.resolve().is_relative_to(saved), 'probe must live in retained Saved reports')
    result=json.loads(PROBE.read_text(encoding='utf-8'))
    need(result.get('mode')=='probe-'+MODE.removeprefix('apply-') and
         result.get('status')=='PASS' and result.get('authoring')=='NONE_READ_ONLY' and
         isinstance(result.get('candidate'),dict),
         'previous placement probe is not approved')
    return result


def sample():
    need(MODE in MODES,'invalid mode')
    # Existing validated inventory, actor IDs, actual observer eye and project
    # channel mapping, including the exact preimage saved-map hash.
    base=original_probe.inspect()
    observer=base['observer']; root=xyz(base['root']); eye=xyz(base['eye'])
    eye_yaw=float(base['eye_rotation'].yaw); eye_pitch=float(base['eye_rotation'].pitch)
    root_yaw=float(base['root_rotation'].yaw)
    world=base['world']; targets=base['targets']
    start_nav=nav_project(world,unreal.Vector(*root),radius=170.)
    need(start_nav is not None,'saved physical observer not on Recast')
    start=xyz(start_nav)
    need(math.hypot(root[0]-start[0],root[1]-start[1])<=115. and
         abs(root[2]-start[2])<=190.,'physical observer does not agree with saved Recast')
    height=root[2]-start[2]
    bounds=[]
    for actor in find(base['world_actors'] if 'world_actors' in base else
                      unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors(),
                      'NavMeshBoundsVolume'):
        origin,extent=actor.get_actor_bounds(False)
        bounds.append({'center':xyz(origin),'extent':xyz(extent)})
    wishes=offsets_from_sources(targets,bounds)
    stats={'requested':len(wishes),'projected':0,'nav_rejected':0,'duplicate':0,
           'complete_paths':0,'no_rotation_solution':0,'qualified':0,
           'tested_yaws':0,'best_visible':0,'trace_protocol':'VisibilityIgnoreSourceConservative'}
    seen=set(); ranked=[]
    for anchor,wish in wishes:
        projection=nav_project(world,unreal.Vector(*wish),radius=130.)
        if projection is None: continue
        stats['projected']+=1
        projected=xyz(projection)
        if not admissible_projection(wish,projected,root,start):
            stats['nav_rejected']+=1;continue
        # World Z for the physical actor's root, not a prop origin nor a
        # fabricated floor height. Preserve the character's saved feet offset.
        candidate=(projected[0],projected[1],projected[2]+height)
        key=tuple(round(q/80.) for q in candidate)
        if key in seen: stats['duplicate']+=1;continue
        seen.add(key)
        if len(seen)>MAX_PROJECTED:
            raise RuntimeError('bounded Recast placement candidate count exceeded')
        route=nav_path(world,unreal.Vector(*start),unreal.Vector(*projected))
        if not route or not 150. <= route['length'] <= 2200.:
            stats['nav_rejected']+=1;continue
        stats['complete_paths']+=1
        # Translate the measured observer eye by the *physical root displacement*;
        # plan_rotation() then rotates its relative offset about the new root.
        eye_at_candidate=tuple(v+(b-a) for v,a,b in zip(eye,root,candidate))
        solution=plan_rotation(candidate,eye_at_candidate,eye_yaw,eye_pitch,
                               root_yaw,targets,float(prop(base['director'],'direct_observation_max_distance')),
                               float(prop(base['director'],'direct_observation_view_half_angle_degrees')),
                               base['visibility_ray'])
        stats['tested_yaws']+=solution['candidates_considered']
        stats['best_visible']=max(stats['best_visible'],solution['best_possible_visible'])
        chosen=solution['chosen']
        if chosen is None:
            stats['no_rotation_solution']+=1;continue
        stats['qualified']+=1
        ranked.append({'anchor':anchor,'root':rounded3(candidate),
                       'original_root':rounded3(root),
                       'movement_path_cm':round(route['length'],3),
                       'long_path_support':route['length']>=750.,
                       'sight':chosen})
    selected=choose(ranked)
    public={'map':base['package'],'map_sha256':base['map_sha256'],
            'observer':observer.get_path_name(),
            'original_root':rounded3(root), 'original_yaw':round(root_yaw,5),
            'original_eye':rounded3(eye),
            'source_positions':[{'id':t['id'],'actor':t['name'],'position':rounded3(t['position'])}
                                for t in targets],
            'original_sight':base['plan']['original'],'search':stats,'candidate':selected,
            'selection_rule':'maximum observed core, then nav path, then shortest relocation'}
    return base,public


def compare_plans(saved,now):
    keys=('map','map_sha256','observer','original_root','original_yaw',
          'original_eye','source_positions','original_sight','search','candidate','selection_rule')
    return all(saved.get(key)==json.loads(json.dumps(now.get(key),sort_keys=True)) for key in keys)


def verify_actual(base,candidate):
    actor=base['observer']
    target_root=valid3(candidate['root'])
    root_now=xyz(actor.get_actor_location())
    need(dist3(root_now,target_root)<=1.5,
         'actual Pedro root drifted from proposed Recast position')
    real_eye,real_rotation=actor.get_actor_eyes_view_point()
    expected_eye=candidate['sight']['eye']
    expected_yaw = (float(base['eye_rotation'].yaw) +
                    angle_difference(float(candidate['sight']['yaw']),
                                     float(base['root_rotation'].yaw)))
    actual_yaw = float(real_rotation.yaw)
    root_rotation = actor.get_actor_rotation()
    position_error = dist3(xyz(real_eye), expected_eye)
    heading_error = abs(angle_difference(actual_yaw, expected_yaw))
    root_heading_error = abs(angle_difference(float(root_rotation.yaw),
                                               float(candidate['sight']['yaw'])))
    # A simulated/attached eye may lag a non-teleporting Actor rotation. Do not
    # weaken these checks: report the actual Editor result and REFUSE to save.
    need(position_error <= 1.5 and heading_error <= 1.0 and root_heading_error <= 1.0,
         'physical observer eye offset changed unexpectedly: '
         + 'expected_eye=' + repr(rounded3(expected_eye))
         + ' actual_eye=' + repr(rounded3(xyz(real_eye)))
         + ' eye_distance_cm=' + repr(round(position_error, 4))
         + ' expected_eye_yaw=' + repr(round(expected_yaw, 5))
         + ' actual_eye_yaw=' + repr(round(actual_yaw, 5))
         + ' root_yaw=' + repr(round(float(root_rotation.yaw), 5))
         + ' proposed_root_yaw=' + repr(candidate['sight']['yaw']))
    sight=[]
    for t in base['targets']:
        status,distance=geometry_status(xyz(real_eye),float(real_rotation.yaw),
                                      float(real_rotation.pitch),t['position'],
                                      float(prop(base['director'],'direct_observation_max_distance')),
                                      float(prop(base['director'],'direct_observation_view_half_angle_degrees')))
        if status=='Visible':
            clear,blocker=base['visibility_ray'](xyz(real_eye),t['position'],t)
            if not clear: status='Occluded'
        sight.append((t['id'],status))
    wanted=[(s['id'],s['status']) for s in candidate['sight']['sources']]
    need(sight==wanted and 1 <= sum(x[1]=='Visible' for x in sight) <= 3,
         'actual saved-world sight does not match approved visible/hidden plan')
    # New root must still be on real nav after the Actor move.
    floor=nav_project(base['world'],unreal.Vector(*target_root),radius=140.)
    need(floor is not None and
         math.hypot(floor.x-target_root[0],floor.y-target_root[1])<=100.,
         'post-move physical observer position not on real Recast')
    return sight


def run():
    base,public=sample()
    candidate=public['candidate']
    if candidate is None:
        return {'status':'BLOCKED','mode':MODE,
                'reason':'NoNavReachableVisibleHiddenObserverPlacement',**public}
    if MODE.startswith('probe-'):
        return {'status':'PASS','mode':MODE,'authoring':'NONE_READ_ONLY',**public}
    approved=read_probe()
    need(compare_plans(approved,public),'read-only probe does not match current saved geometry or plan')
    old_root=xyz(base['root'])
    need(dist3(old_root,candidate['root'])>=250.,
         'no real relocation proposed; refusing redundant authoring')
    actor=base['observer']; previous=base['root_rotation']
    # Deliberate Editor-only authoring; runtime gameplay never teleports Pedro.
    with unreal.ScopedEditorTransaction('Pedro B2b6e measured observer placement'):
        # One atomic UE Actor transform. Teleport=True propagates the rotation
        # to attached/simulating children, including the physical ObserverEye.
        # This API can return None when there is no sweep hit: NEVER interpret
        # that as failure or success. Read back root + eye pose below.
        # Unreal *Python* Rotator is (roll, pitch, yaw), unlike the C++
        # FRotator(Pitch, Yaw, Roll) constructor. The old positional call
        # silently applied -123.52 degrees to pitch and flipped root yaw 180.
        # Use named axes and validate them BEFORE moving the actor.
        requested_yaw=float(candidate['sight']['yaw'])
        requested_rotation=unreal.Rotator(
            roll=float(previous.roll),
            pitch=float(previous.pitch),
            yaw=requested_yaw)
        need(abs(angle_difference(float(requested_rotation.yaw),requested_yaw))<=0.001
             and abs(angle_difference(float(requested_rotation.pitch),float(previous.pitch)))<=0.001
             and abs(angle_difference(float(requested_rotation.roll),float(previous.roll)))<=0.001,
             'Python Rotator axis mismatch before authoring; refusing actor move')
        actor.set_actor_location_and_rotation(
            unreal.Vector(*candidate['root']),
            requested_rotation,
            False, True)
        sight=verify_actual(base,candidate)
        # Ensure not a changed/overwritten cloned asset or an external level.
        need(base['world'].get_outermost().get_name()==base['package'],
             'loaded level identity drifted')
        save_map(base['package'])
    saved_hash=original_probe.sha_file(base['map_file'])
    need(saved_hash!=base['map_sha256'], 'authoring was no-op; refusing PASS')
    return {'status':'PASS','mode':MODE,'authoring':'OBSERVER_ROOT_AND_YAW_ONLY_SAVED',
            'saved_map_sha256':saved_hash,'actual_sight':sight,**public}


def main():
    need(REPORT.is_absolute(),'report must have absolute Saved path')
    try:
        result=run()
    except Exception as exc:
        result={'status':'FAIL','mode':MODE,'reason':str(exc),
                'traceback':traceback.format_exc()}
    REPORT.write_text(json.dumps(result,sort_keys=True,indent=2)+'\n',encoding='utf-8')
    if result['status']=='PASS':
        unreal.log('PEDRO_PLACEMENT status=PASS mode='+MODE)
    else:
        unreal.log_error('PEDRO_PLACEMENT status='+result['status']+
                         ' reason='+result.get('reason',''))
        raise RuntimeError(result.get('reason','placement failed'))

if __name__=='__main__':main()
