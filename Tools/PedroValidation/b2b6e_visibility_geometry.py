"""Pure, deterministic sightline planning for saved Pedro actors.

This is an authoring aid, NOT a replacement for runtime DirectObservation.
Every proposed sightline must also pass Unreal's actual ECC_Visibility ray
and independent fresh-game interaction tests before acceptance.
"""
from __future__ import annotations
import math


def valid3(value):
    if len(value) != 3 or any(isinstance(v, bool) or not isinstance(v, (float, int))
                                  or not math.isfinite(v) for v in value):
        raise ValueError('non-finite / malformed world position')
    return tuple(float(v) for v in value)


def norm_yaw(yaw):
    if not math.isfinite(yaw):
        raise ValueError('non-finite yaw')
    return (yaw + 180.0) % 360.0 - 180.0


def angle_difference(a, b):
    return norm_yaw(a-b)


def hypot_eye(pivot, eye, yaw_delta):
    pivot, eye = valid3(pivot), valid3(eye)
    a = math.radians(yaw_delta)
    dx, dy = eye[0]-pivot[0], eye[1]-pivot[1]
    return (pivot[0] + dx*math.cos(a)-dy*math.sin(a),
            pivot[1] + dx*math.sin(a)+dy*math.cos(a), eye[2])


def geometry_status(eye, facing_yaw, facing_pitch, point, max_distance, half_angle):
    eye, point = valid3(eye), valid3(point)
    if not (0.0 < max_distance < 100000.0 and 0.0 <= half_angle <= 180.0 and
            all(math.isfinite(v) for v in (facing_yaw,facing_pitch,max_distance,half_angle))):
        raise ValueError('invalid observer settings')
    delta = tuple(b-a for a,b in zip(eye,point))
    distance = math.sqrt(sum(v*v for v in delta))
    if distance > max_distance:
        return 'TooFar', distance
    if distance < 1e-6:
        return 'Visible', distance
    yaw, pitch = math.radians(facing_yaw), math.radians(facing_pitch)
    front = (math.cos(pitch)*math.cos(yaw), math.cos(pitch)*math.sin(yaw), math.sin(pitch))
    dot = sum(a*b for a,b in zip(front,delta)) / distance
    if dot < math.cos(math.radians(half_angle))-1e-6:
        return 'OutsideViewCone', distance
    return 'Visible', distance


def candidate_yaws(pivot, eye, eye_yaw, root_yaw, targets):
    """Small, bounded angular search derived only from actual source locations."""
    pivot, eye = valid3(pivot), valid3(eye)
    result = [norm_yaw(root_yaw)]
    bearings = []
    for t in targets:
        p = valid3(t['position'])
        bearing = math.degrees(math.atan2(p[1]-eye[1],p[0]-eye[0]))
        desired = norm_yaw(root_yaw + angle_difference(bearing,eye_yaw))
        bearings.append(desired)
        for offset in (0.0, -35.0,35.0, -60.0,60.0):
            result.append(norm_yaw(desired+offset))
    for i,a in enumerate(bearings):
        for b in bearings[i+1:]:
            result.append(norm_yaw(a+angle_difference(b,a)/2.0))
    dedup = []
    for y in result:
        if not any(abs(angle_difference(y,old))<0.1 for old in dedup):
            dedup.append(y)
    if len(dedup)>40:
        raise ValueError('search bound exceeded')
    return dedup


def plan_rotation(pivot, eye, eye_yaw, eye_pitch, root_yaw, targets,
                  max_distance, half_angle, visibility_ray):
    """Plan the one rotation that exposes the most real source actors.

    visibility_ray(eye, point, target) returns (clear:bool, blocker:str).
    A blocked ray never becomes visible by assumption. A hidden core is retained
    so the already-authored failed/hidden/visible game tests remain meaningful.
    """
    if len(targets)!=4 or len({t['id'] for t in targets})!=4:
        raise ValueError('require exactly four distinct saved core actors')
    ranked=[]
    for yaw in candidate_yaws(pivot,eye,eye_yaw,root_yaw,targets):
        delta=angle_difference(yaw,root_yaw)
        hypothetical_eye=hypot_eye(pivot,eye,delta)
        items=[]
        for t in targets:
            status,distance=geometry_status(hypothetical_eye,eye_yaw+delta,eye_pitch,
                                            t['position'],max_distance,half_angle)
            blocker=''
            if status=='Visible':
                clear,blocker=visibility_ray(hypothetical_eye, t['position'], t)
                if not clear:
                    status='Occluded'
            items.append({'id':t['id'], 'name':t['name'], 'status':status,
                          'distance_cm':round(distance,3), 'blocker':blocker})
        visible=sum(item['status']=='Visible' for item in items)
        hidden=len(items)-visible
        ranked.append({'yaw':round(yaw,5),'changed_degrees':round(abs(delta),5),
                       'eye':tuple(round(x,4) for x in hypothetical_eye),
                       'visible':visible,'hidden':hidden,'sources':items})
    candidates=[p for p in ranked if p['visible']>=1 and p['hidden']>=1]
    candidates.sort(key=lambda p:(-p['visible'],p['changed_degrees'],p['yaw']))
    chosen=candidates[0] if candidates else None
    return {'chosen':chosen,'original':ranked[0],
            'candidates_considered':len(ranked),
            'best_possible_visible':max(p['visible'] for p in ranked),
            'needs_authoring':chosen is not None and chosen['changed_degrees']>0.05}
