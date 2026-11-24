#!/usr/bin/env python3
"""Independent, fail-closed before/after audit for measured observer placement."""
from __future__ import annotations
import argparse
import hashlib
import json
from pathlib import Path
import re
import subprocess

MAPS={
 'test':'Content/AI_Gavin/Encounters/Pedro/Validation/B2b6e/L_Pedro_AutonomousTest.umap',
 'production':'Content/AI_Gavin/Encounters/Pedro/Maps/L_Pedro_Prototype.umap',
}

def need(ok,why):
    if not ok: raise ValueError('PEDRO_PLACEMENT STOP: '+why)

def hashes(path):
    data={}
    for line in path.read_text(encoding='utf-8').splitlines():
        match=re.fullmatch(r'([a-f0-9]{64})  ([^\n]+)',line)
        need(match is not None,'invalid snapshot manifest')
        p=match.group(2)
        need(p not in data and not p.startswith('/') and '..' not in Path(p).parts,
             'unsafe or duplicate path')
        data[p]=match.group(1)
    return data

def statuses(data):
    need(not data or data.endswith(b'\0'),'status is not NUL terminated')
    rows={}
    for item in data.split(b'\0'):
        if not item:continue
        need(len(item)>3 and item[2:3]==b' ','invalid porcelain row')
        code=item[:2];path=item[3:].decode('utf-8')
        need(code in (b'A ',b'M ',b'AM',b'MM') and path not in rows,
             'unexpected staged or unstaged Git state')
        rows[path]=code
    return rows

def audit(run,mode):
    run=Path(run)
    kind,which=mode.split('-',1)
    need(kind in ('probe','apply') and which in MAPS,'invalid mode')
    before=hashes(run/'files-before.sha256')
    after=hashes(run/'files-after.sha256')
    need(len(before)>=625 and len(after)>=625,'unexpected truncated project inventory')
    b=statuses((run/'status-before.txt').read_bytes())
    a=statuses((run/'status-after.txt').read_bytes())
    result=json.loads((run/'results.json').read_text())
    if result.get('status')!='PASS' or result.get('mode')!=mode:
        print('PEDRO_PLACEMENT_EDITOR_RESULT '+json.dumps({
            k:result.get(k) for k in ('status','mode','reason','search','candidate')
        },sort_keys=True),flush=True)
        need(False,'Editor did not prove a valid placement')
    choice=result.get('candidate')
    need(isinstance(choice,dict) and isinstance(choice.get('sight'),dict),
         'no measured candidate')
    sight=choice['sight']
    need(1<=sight.get('visible',0)<=3 and sight.get('visible',0)+sight.get('hidden',0)==4 and
         len(sight.get('sources',[]))==4 and choice.get('movement_path_cm',0)>=150,
         'candidate lacks genuine visible/hidden evidence')
    changed={p for p in (before.keys()|after.keys()) if before.get(p)!=after.get(p)}
    if kind=='probe':
        need(not changed and b==a and result.get('authoring')=='NONE_READ_ONLY',
             'read-only probe changed project')
    else:
        target=MAPS[which]
        need(changed=={target} and result.get('authoring')=='OBSERVER_ROOT_AND_YAW_ONLY_SAVED',
             'unauthorized map/source change: '+repr(sorted(changed)))
        need(after[target]==result.get('saved_map_sha256') and
             before[target]==result.get('map_sha256'),'saved map digest differs from Editor report')
        need(set(b)==set(a) and b.get(target) in (b'M ',b'A ') and all(
            a[p]==(b[p][0:1]+b'M' if p==target else b[p]) for p in b),
            'Git staged state changed outside approved one map')
    return {'status':'PASS','mode':mode,'changed_assets':sorted(changed),
            'visible_core':sight['visible'],'hidden_core':sight['hidden'],
            'project_inventory':len(before)}

def git(root,*args):
    r=subprocess.run(['git',*args],cwd=root,stdout=subprocess.PIPE,stderr=subprocess.PIPE)
    need(r.returncode==0,'git command failed: '+r.stderr.decode(errors='replace')[-500:])
    return r.stdout

def staged(project,kind):
    project=Path(project)
    rel=MAPS[kind]
    need(not git(project,'diff','--name-only').strip(),'unexpected unstaged files after map staging')
    ptr=git(project,'show',':'+rel).decode('ascii')
    match=re.fullmatch(r'version https://git-lfs.github.com/spec/v1\noid sha256:([a-f0-9]{64})\nsize ([0-9]+)\n',ptr)
    need(match is not None,'approved map no longer staged as valid LFS pointer')
    contents=(project/rel).read_bytes()
    need(hashlib.sha256(contents).hexdigest()==match.group(1) and
         len(contents)==int(match.group(2)),'staged LFS map payload mismatch')
    git(project,'lfs','fsck')
    git(project,'diff','--cached','--check')
    return {'status':'PASS','map':rel,'saved_map_sha256':match.group(1)}

def main():
    parser=argparse.ArgumentParser()
    parser.add_argument('operation',choices=['compare','staged'])
    parser.add_argument('arg1')
    parser.add_argument('arg2')
    a=parser.parse_args()
    r=audit(a.arg1,a.arg2) if a.operation=='compare' else staged(a.arg1,a.arg2)
    print(json.dumps(r,indent=2,sort_keys=True))

if __name__=='__main__':main()
