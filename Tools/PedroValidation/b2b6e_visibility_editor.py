"""UE 5.8.2 Editor-only, evidence-grounded rotation of Pedro's saved observer.

PROBE is read-only. ALIGN edits precisely the saved PedroObserverCharacter
rotation through the Unreal Actor and LevelEditorSubsystem APIs. If the map
cannot offer a real visible + hidden core at the same pose, stop without saving.
No prop/event/observation system, source ID, nav bounds or binary bytes are edited.
"""
from __future__ import annotations
import hashlib
import json
import os
from pathlib import Path
import sys
import traceback

sys.dont_write_bytecode = True
if os.environ.get('B2B6E_PYCACHE_PREFIX'):
    sys.pycache_prefix=os.environ['B2B6E_PYCACHE_PREFIX']
sys.path.insert(0,os.environ.get('B2B6E_SCRIPT_DIR',str(Path(__file__).resolve().parent)))
import unreal
from b2b6e_contract import TEST, PRODUCTION
from b2b6e_editor_common import ACTORS, prop, load_map, save_map
from b2b6e_visibility_geometry import plan_rotation, valid3, angle_difference, geometry_status

MODE=os.environ.get('B2B6E_VIS_MODE','')
REPORT=Path(os.environ.get('B2B6E_VIS_REPORT',''))
PROBE=Path(os.environ.get('B2B6E_VIS_PROBE','')) if os.environ.get('B2B6E_VIS_PROBE') else None
MAPS={'probe-test':TEST,'align-test':TEST,
      'probe-production':PRODUCTION,'align-production':PRODUCTION}


def require(ok,reason):
    if not ok: raise RuntimeError('PEDRO_VISIBILITY STOP: '+reason)


def vec(v):
    return valid3((float(v.x),float(v.y),float(v.z)))


def sha_file(path):
    h=hashlib.sha256()
    with path.open('rb') as f:
        for block in iter(lambda:f.read(1024*1024),b''):
            h.update(block)
    return h.hexdigest()


def map_disk_path(pkg):
    return Path(os.environ['B2B6E_PROJECT'])/'Content'/(pkg.removeprefix('/Game/')+'.umap')


def unique(actors,typename):
    clazz=getattr(unreal,typename,None)
    require(clazz is not None,'Unreal reflected class absent: '+typename)
    found=[a for a in actors if isinstance(a,clazz)]
    require(len(found)==1,f'exactly one saved {typename} required, found {len(found)}')
    return found[0]


def inspect():
    require(MODE in MAPS,'unsupported mode')
    package=MAPS[MODE]
    map_file=map_disk_path(package)
    require(map_file.is_file() and not map_file.is_symlink(),'missing or symlinked saved map')
    initial_map_hash=sha_file(map_file)
    world,actors=load_map(package)
    observer=unique(actors,'PedroObserverCharacter')
    director=unique(actors,'PedroEncounterDirector')
    integration=unique(actors,'PedroPlayableIntegrationActor')
    require(prop(integration,'observer')==observer and
            prop(integration,'director')==director,'not the saved playable integration')
    require(prop(director,'direct_observation_actor')==observer and
            bool(prop(director,'use_direct_observation_acquisition','b_use_direct_observation_acquisition')) and
            not bool(prop(director,'use_prototype_assumed_acquisition','b_use_prototype_assumed_acquisition')),
            'must have one physical, direct-only observer')
    require('EVIDENCE' in str(prop(director,'completion_policy')).upper(),
            'not evidence-driven completion')
    defs=list(prop(director,'opportunity_definitions'))
    ids=[str(prop(x,'opportunity_id')) for x in defs
         if 'CORE' in str(prop(x,'tier')).upper()]
    require(len(ids)==4 and len(set(ids))==4,'require exactly four unique core IDs')
    refcls=getattr(unreal,'PedroPrototypeInteractableActor',None)
    sourcecls=getattr(unreal,'PedroObservationSourceComponent',None)
    require(refcls is not None and sourcecls is not None,'native Pedro source types missing')
    targets=[]
    for a in actors:
        if not isinstance(a,refcls):continue
        components=list(a.get_components_by_class(sourcecls))
        require(len(components)==1,'wrong source count on real Pedro prop')
        src=components[0]
        opportunity=str(prop(src,'opportunity_id'))
        if opportunity in ids:
            require(bool(prop(src,'evidence_enabled','b_evidence_enabled')),
                    'core source has evidence disabled: '+opportunity)
            targets.append({'id':opportunity,'name':a.get_name(),
                            'position':vec(a.get_actor_location()),'actor':a})
    require(len(targets)==4 and set(t['id'] for t in targets)==set(ids),
            'saved core source actors do not match Director')
    targets.sort(key=lambda x:(x['id'],x['name']))
    eye, eye_rotation=observer.get_actor_eyes_view_point()
    root_rot=observer.get_actor_rotation()
    root=observer.get_actor_location()
    max_distance=float(prop(director,'direct_observation_max_distance'))
    half_angle=float(prop(director,'direct_observation_view_half_angle_degrees'))
    require(0<max_distance<=100000 and 0<half_angle<=180,'invalid saved visibility tuning')
    require(hasattr(unreal.SystemLibrary,'line_trace_single') and
            hasattr(unreal.TraceTypeQuery,'ECC_VISIBILITY'),
            'installed Unreal Python does not support ECC_Visibility traces')
    require(hasattr(unreal.DrawDebugTrace,'NONE'),'Unreal draw debug enum missing')

    def visibility_ray(origin,target_pos,target):
        # UE 5.8 Python returns HitResult OR None for a single blocking trace;
        # it does not promise get_editor_property('blocking_hit') (nor hit_actor).
        # Ignore the intended source actor as well as the physical observer:
        # the production C++ accepts a first hit on the source, while any OTHER
        # blocking collider must reject the ray. Ignoring the source is
        # conservative if something behind its surface blocks before its origin.
        result=unreal.SystemLibrary.line_trace_single(
            world,unreal.Vector(*origin),unreal.Vector(*target_pos),
            unreal.TraceTypeQuery.ECC_VISIBILITY,False,
            [observer,target['actor']],unreal.DrawDebugTrace.NONE,False)
        if result is None:
            return True,''
        require(isinstance(result,unreal.HitResult),
                'UE line_trace_single returned neither HitResult nor None')
        return False,'InterveningVisibilityBlocker'

    result=plan_rotation(vec(root),vec(eye),float(eye_rotation.yaw),float(eye_rotation.pitch),
                         float(root_rot.yaw),targets,max_distance,half_angle,visibility_ray)
    return {
        'package':package,'map_sha256':initial_map_hash,'observer':observer,
        'director':director,
        'root':root,'root_rotation':root_rot,'eye':eye,'eye_rotation':eye_rotation,
        'targets':targets,'plan':result,'map_file':map_file,'world':world,
        'visibility_ray':visibility_ray,
    }


def plan_public(info):
    return {'map':info['package'],'map_sha256':info['map_sha256'],
            'observer':info['observer'].get_name(),
            'observer_root':vec(info['root']),
            'original_yaw':round(float(info['root_rotation'].yaw),5),
            'core_sources':[{'opportunity':t['id'],'actor':t['name'],
                             'event_location':t['position']} for t in info['targets']],
            'candidate':info['plan']['chosen'],
            'original_sight':info['plan']['original'],
            'evaluated_yaws':info['plan']['candidates_considered'],
            'best_visible_count':info['plan']['best_possible_visible'],
            'rotation_needed':info['plan']['needs_authoring']}


def run():
    info=inspect()
    candidate=info['plan']['chosen']
    public=plan_public(info)
    if candidate is None:
        return {'status':'BLOCKED','mode':MODE,'reason':'NoRotationOnlyVisibleAndHiddenCore',**public}
    if not info['plan']['needs_authoring']:
        return {'status':'BLOCKED','mode':MODE,'reason':'AlreadyAlignedYetRuntimeFailed',**public}
    if MODE.startswith('probe-'):
        return {'status':'PASS','mode':MODE,'authoring':'NONE_READ_ONLY',**public}
    require(PROBE is not None and PROBE.is_file() and not PROBE.is_symlink(),
            'align requires the independent retained probe')
    require(PROBE.resolve().is_relative_to((Path(os.environ['B2B6E_PROJECT'])/'Saved/PedroValidation').resolve()),
            'probe must be a retained project report')
    old=json.loads(PROBE.read_text(encoding='utf-8'))
    expect='probe-'+MODE.removeprefix('align-')
    require(old.get('status')=='PASS' and old.get('mode')==expect and
            old.get('authoring')=='NONE_READ_ONLY' and
            all(old.get(k)==json.loads(json.dumps(public.get(k),sort_keys=True)) for k in public),
            'probe does not match the unchanged saved map, source geometry or chosen yaw')
    observer=info['observer']
    r=info['root_rotation']
    require(observer.set_actor_rotation(
        unreal.Rotator(float(r.pitch),float(candidate['yaw']),float(r.roll)),False),
        'Unreal rejected observer-only rotation')
    require(sum((a-b)**2 for a,b in zip(vec(observer.get_actor_location()),vec(info['root'])))<0.01,
            'observer position changed during rotation')
    # Re-evaluate actual Unreal eye pose and actual collision after mutation,
    # before saving anything. This detects unexpected component-relative eyes.
    eye_now, r_now=observer.get_actor_eyes_view_point()
    if sum((a-b)**2 for a,b in zip(vec(eye_now),candidate['eye']))>1.0 or \
       abs(angle_difference(float(r_now.yaw),float(info['eye_rotation'].yaw)+
                            angle_difference(candidate['yaw'],float(r.yaw))))>1.0:
        raise RuntimeError('actual observer eye drifted from read-only sight plan; refusing save')
    # The event source's exact world position must remain visible through the
    # real Editor Visibility ray AFTER the actual Actor rotation, not merely
    # from simulated math. Never infer sight from a player setup position.
    post_visible=0
    for target in info['targets']:
        status,_=geometry_status(vec(eye_now),float(r_now.yaw),float(r_now.pitch),
                                 target['position'],float(prop(info['director'],'direct_observation_max_distance')),
                                 float(prop(info['director'],'direct_observation_view_half_angle_degrees')))
        if status=='Visible':
            clear,_=info['visibility_ray'](vec(eye_now),target['position'],target)
            if clear:post_visible+=1
    require(post_visible==candidate['visible'] and 0<post_visible<4,
            'real post-rotation Visibility rays differ from the plan; refusing save')
    # No direct observation knowledge is synthesized.
    require(sha_file(info['map_file'])==info['map_sha256'],
            'saved map bytes changed while evaluating; refusing save')
    save_map(info['package'])
    return {'status':'PASS','mode':MODE,'authoring':'ROTATION_ONLY_SAVED',
            'saved_map_sha256':sha_file(info['map_file']),**public}


def main():
    require(REPORT.is_absolute(),'absolute Saved report required')
    try:
        result=run()
    except Exception as exc:
        result={'status':'FAIL','mode':MODE,'reason':str(exc),
                'traceback':traceback.format_exc()}
    REPORT.write_text(json.dumps(result,indent=2,sort_keys=True)+'\n',encoding='utf-8')
    if result.get('status')=='PASS':
        unreal.log('PEDRO_VISIBILITY status=PASS mode='+MODE)
    else:
        unreal.log_error('PEDRO_VISIBILITY status='+str(result.get('status'))+
                         ' reason='+str(result.get('reason')))
        raise RuntimeError(result['reason'])

if __name__=='__main__':main()
