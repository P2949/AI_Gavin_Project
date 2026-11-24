"""Independent before/after and Git/LFS validation of Editor sightline rotation.
This program does not edit map bytes. Only a caller-controlled exact `git add`
may stage a single already-verified Pedro-owned binary map.
"""
from __future__ import annotations
import argparse
import hashlib
import json
from pathlib import Path
import re
import subprocess

MAPS={'test':'Content/AI_Gavin/Encounters/Pedro/Validation/B2b6e/L_Pedro_AutonomousTest.umap',
      'production':'Content/AI_Gavin/Encounters/Pedro/Maps/L_Pedro_Prototype.umap'}


def require(ok,msg):
    if not ok:raise RuntimeError('PEDRO_VISIBILITY STOP: '+msg)


def hashes(path):
    result={}
    for line in path.read_text().splitlines():
        m=re.fullmatch(r'([0-9a-f]{64})  (.+)',line)
        require(m is not None,'malformed snapshot hash row')
        require(m.group(2) not in result,'duplicate snapshot path')
        result[m.group(2)]=m.group(1)
    return result


def compare(report_dir,mode):
    report_dir=Path(report_dir)
    kind,which=mode.split('-',1)
    require(kind in ('probe','align') and which in MAPS,'invalid audit mode')
    before=hashes(report_dir/'files-before.sha256')
    after=hashes(report_dir/'files-after.sha256')
    require(len(before)==len(after) and len(before)>=614,'unexpected project inventory')
    first=(report_dir/'status-before.txt').read_bytes()
    last=(report_dir/'status-after.txt').read_bytes()
    result=json.loads((report_dir/'results.json').read_text())
    if result.get('status') != 'PASS' or result.get('mode') != mode:
        detail = {
            'status': result.get('status'), 'mode': result.get('mode'),
            'reason': str(result.get('reason', ''))[:1200],
            'best_visible_count': result.get('best_visible_count'),
            'evaluated_yaws': result.get('evaluated_yaws'),
            'original_sight': result.get('original_sight'),
            'candidate': result.get('candidate'),
            'traceback_tail': str(result.get('traceback', ''))[-1600:],
        }
        print('PEDRO_VISIBILITY_EDITOR_RESULT ' + json.dumps(detail, sort_keys=True), flush=True)
        require(False, 'Unreal did not report PASS; source reason and geometry printed above')
    choice=result.get('candidate')
    require(isinstance(choice,dict) and choice.get('visible',0)>=1 and
            choice.get('hidden',0)>=1 and choice.get('visible',0)+choice.get('hidden',0)==4 and
            result.get('rotation_needed') is True,
            'missing real visible/hidden core selection')
    diff={p for p in before.keys()|after.keys() if before.get(p)!=after.get(p)}
    if kind=='probe':
        require(result.get('authoring')=='NONE_READ_ONLY' and not diff and first==last,
                'read-only Editor probe modified saved project')
    else:
        only=MAPS[which]
        require(result.get('authoring')=='ROTATION_ONLY_SAVED' and diff=={only},
                'Editor saved files outside the one approved Pedro map: '+repr(sorted(diff)))
        require(after[only]==result.get('saved_map_sha256') and
                before[only]==result.get('map_sha256'),
                'saved map digest does not match Editor-authored report')
        # Git status in a partially staged checkout becomes XY=MM on only the
        # touched map; the caller must stage it after this independent check.
        def parse(data):
            require(not data or data.endswith(b'\0'),'missing porcelain NUL delimiter')
            rows={}
            for row in data.split(b'\0'):
                if not row:continue
                require(len(row)>3 and row[2:3]==b' ','malformed Git porcelain row')
                path=row[3:].decode()
                require(path not in rows and row[:2] in (b'A ',b'M ',b'D ',b'T ',b'MM',b'AM'),
                        'unapproved Git status')
                rows[path]=row[:2]
            return rows
        old,new=parse(first),parse(last)
        require(set(new)==set(old) and all(
            new[p]==(v[:1]+b'M' if p==only else v) for p,v in old.items()) and old.get(only) in (b'M ',b'A '),
            'unexpected staged/unstaged Git status after Editor save')
    return {'status':'PASS','mode':mode,'binary_changes':sorted(diff),
            'project_file_count':len(before),'visible_core':choice['visible'],
            'hidden_core':choice['hidden']}


def git(root,*args):
    return subprocess.check_output(['git',*args],cwd=root)


def check_staged(project,which):
    project=Path(project)
    rel=MAPS[which]
    require(not git(project,'diff','--name-only').strip(),
            'unstaged project changes remain after exact staging')
    ptr=git(project,'show',':'+rel).decode('ascii')
    m=re.fullmatch(r'version https://git-lfs.github.com/spec/v1\noid sha256:([0-9a-f]{64})\nsize ([0-9]+)\n',ptr)
    require(m is not None,'index contains invalid production/test LFS pointer')
    data=(project/rel).read_bytes()
    require(hashlib.sha256(data).hexdigest()==m.group(1) and len(data)==int(m.group(2)),
            'staged LFS pointer does not match actual saved Unreal map')
    git(project,'lfs','fsck')
    return {'status':'PASS','staged_map':rel,'sha256':m.group(1)}


def main():
    p=argparse.ArgumentParser()
    p.add_argument('mode',choices=['compare','staged'])
    p.add_argument('arg1')
    p.add_argument('arg2')
    args=p.parse_args()
    result=compare(args.arg1,args.arg2) if args.mode=='compare' else check_staged(args.arg1,args.arg2)
    print(json.dumps(result,indent=2,sort_keys=True))

if __name__=='__main__':main()
