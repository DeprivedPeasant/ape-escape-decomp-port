"""Local Sonnet implementation jobs; stdlib only, explicit review before integration."""
import argparse,hashlib,json,os,re,shutil,subprocess,time,uuid
from pathlib import Path

ROOT=Path(__file__).resolve().parents[4]
STATE=ROOT/'extract/codex/bridge'
MODEL='claude-sonnet-5-5'

def save(path,data):
    temp=path.with_suffix('.tmp');temp.write_text(json.dumps(data,indent=2)+'\n',encoding='utf-8');temp.replace(path)

def git(*args,cwd=ROOT):
    return subprocess.run(['git',*args],cwd=cwd,check=True,capture_output=True).stdout

def location(name):
    if not re.fullmatch(r'[a-z0-9][a-z0-9-]{0,63}',name):raise ValueError('Invalid job id')
    return STATE/'jobs'/name

def relative(path):
    p=Path(path)
    if p.is_absolute() or '..' in p.parts or not p.parts:raise ValueError('Unsafe output path')
    value=p.as_posix()
    if not value.startswith('tools/codex/') or any(v in p.parts for v in ('.git','.claude','.codex')):
        raise ValueError('Worker output must be an explicit tools/codex path')
    return value

def digest(path):return hashlib.sha256(path.read_bytes()).hexdigest() if path.is_file() else None

def executable():
    config=STATE/'local.json'
    value=json.loads(config.read_text())['claude'] if config.exists() else shutil.which('claude')
    if not value or not Path(value).is_file():raise RuntimeError('Configure an existing Claude Code executable with configure --claude PATH')
    return value

def enqueue(name,prompt,paths,budget=1.0,timeout=900):
    paths=sorted({relative(p) for p in paths})
    if not paths or budget<=0 or timeout<=0:raise ValueError('Need output files and positive limits')
    folder=location(name);folder.mkdir(parents=True,exist_ok=False)
    base=git('rev-parse','HEAD').decode().strip()
    job=dict(id=name,status='queued',model=MODEL,base=base,paths=paths,budget_usd=budget,timeout_seconds=timeout,
             created=time.time(),prompt=prompt,baseline={p:digest(ROOT/p) for p in paths})
    save(folder/'job.json',job);return job

def command(job,folder):
    return [executable(),'-p','--model',MODEL,'--effort','medium','--output-format','json',
            '--safe-mode','--restricted','--strict-mcp-config','--mcp-config',str(folder/'mcp.json'),
            '--no-session-persistence','--permission-mode','dontAsk','--tools','Read,Glob,Grep,Edit,Write',
            '--allowedTools','Read,Glob,Grep,Edit,Write','--max-budget-usd',str(job['budget_usd'])]

def changed(work):
    return sorted(set(git('ls-files','--modified','--deleted','--others','--exclude-standard','-z',cwd=work).decode().strip('\0').split('\0'))-{''})

def allowance(job):
    config=json.loads((STATE/'local.json').read_text())
    ceiling=config.get('total_budget_usd',5.0)
    used=0.0
    for file in (STATE/'jobs').glob('*/job.json'):
        previous=json.loads(file.read_text())
        if previous['id']==job['id'] or previous['status']=='queued':continue
        cost=previous.get('reported_cost_usd')
        used+=float(cost if cost is not None else previous['budget_usd'])
    if used+job['budget_usd']>ceiling:raise RuntimeError(f'Bridge budget admission limit: {used:.2f} used/reserved, {ceiling:.2f} configured')

def collect(folder,job):
    work=Path(job['worktree']);paths=changed(work)
    forbidden=set(paths)-set(job['paths'])
    if forbidden:raise RuntimeError('Out-of-scope changes: '+', '.join(sorted(forbidden)))
    files=[]
    for name in paths:
        src=work/name
        if src.is_symlink() or not src.resolve().is_relative_to(work.resolve()):raise RuntimeError('Linked output refused')
        dest=folder/'deliverables'/name
        if src.exists():
            if src.stat().st_size>2*1024*1024:raise RuntimeError('Output larger than 2 MiB')
            dest.parent.mkdir(parents=True,exist_ok=True);shutil.copyfile(src,dest)
        files.append(dict(path=name,sha256=digest(src),deleted=not src.exists(),current_primary_sha256=digest(ROOT/name),
                          primary_unchanged=digest(ROOT/name)==job['baseline'][name]))
    save(folder/'deliverables.json',files)
    (folder/'tracked.patch').write_bytes(git('diff','--binary',job['base'],'--',*job['paths'],cwd=work))
    return files

def run(name):
    folder=location(name);job=json.loads((folder/'job.json').read_text())
    if job['status']!='queued':raise RuntimeError('Jobs run once; enqueue a new job to retry')
    lock=STATE/'worker.lock';STATE.mkdir(parents=True,exist_ok=True)
    with lock.open('x') as f:f.write(str(os.getpid()))
    try:
        executable() # fail before creating a worktree if configuration is absent
        allowance(job)
        work=STATE/'work'/name;work.parent.mkdir(parents=True,exist_ok=True)
        git('worktree','add','--detach',str(work),job['base'])
        job.update(status='running',worktree=str(work),started=time.time());save(folder/'job.json',job)
        save(folder/'mcp.json',{'mcpServers':{}})
        instructions=(
            'You are a delegated implementation worker for Ape Escape. Codex is the coordinator. '
            'This is an isolated worktree. Read notes/COORDINATION.md for project facts. '
            'The user authorizes only the following output files for THIS delegated job: '+', '.join(job['paths'])+'. '
            'Do not modify other files, git metadata, settings, user directories, player module, capture tooling or naming tables. '
            'Do not start agents, use network, open Ghidra, commit, or contact the existing Claude session. '
            'Shell tools are unavailable; Codex runs builds/tests after reviewing your result. '
            'Never copy code from reference/ApeEscapeRecomp. Original data and ignored exports are absent here; '
            'if evidence is missing, report the blocker rather than guessing. '
            'Finish with changed files, reasoning, validation still required, and blockers.\n\nTASK:\n'+job['prompt'])
        (folder/'prompt.txt').write_text(instructions,encoding='utf-8')
        env=dict(os.environ,DISABLE_AUTOUPDATER='1',CLAUDE_CODE_DISABLE_NONESSENTIAL_TRAFFIC='1')
        with (folder/'stdout.json').open('wb') as out,(folder/'stderr.txt').open('wb') as err:
            process=subprocess.Popen(command(job,folder),cwd=work,env=env,stdin=subprocess.PIPE,stdout=out,stderr=err)
            job['pid']=process.pid;save(folder/'job.json',job)
            try:process.communicate(instructions.encode('utf-8'),timeout=job['timeout_seconds'])
            except subprocess.TimeoutExpired:
                process.kill();process.wait();raise RuntimeError('Owned worker exceeded time limit; stopped only this process')
        result=json.loads((folder/'stdout.json').read_text(encoding='utf-8-sig'))
        job['exit_code']=process.returncode;job['reported_cost_usd']=result.get('total_cost_usd')
        job['model_usage']=result.get('modelUsage',{})
        job['session_id']=result.get('session_id');job['response']=result.get('result','')
        if process.returncode or result.get('is_error'):raise RuntimeError('Claude failed: '+str(result.get('result',result.get('subtype',''))))
        if not job['model_usage'] or any(not m.startswith(MODEL) for m in job['model_usage']):
            raise RuntimeError('Requested Sonnet5.5 model was not confirmed; no automatic fallback accepted')
        job['deliverables']=collect(folder,job);job['status']='review_required'
    except Exception as error:
        job.update(status='blocked',error=str(error))
    finally:
        job['finished']=time.time();save(folder/'job.json',job);lock.unlink()
    print(json.dumps({k:job.get(k) for k in ('id','status','error','reported_cost_usd','model_usage','deliverables')},indent=2))
    return job

def main():
    p=argparse.ArgumentParser(description=__doc__);sub=p.add_subparsers(dest='action',required=True)
    cfg=sub.add_parser('configure');cfg.add_argument('--claude',required=True);cfg.add_argument('--total-budget',type=float,default=5)
    add=sub.add_parser('enqueue');add.add_argument('id');add.add_argument('--prompt-file',required=True);add.add_argument('--output',action='append',required=True);add.add_argument('--budget',type=float,default=1);add.add_argument('--timeout',type=int,default=900)
    start=sub.add_parser('run');start.add_argument('id')
    status=sub.add_parser('status');status.add_argument('id',nargs='?')
    args=p.parse_args();STATE.mkdir(parents=True,exist_ok=True)
    if args.action=='configure':
        path=Path(args.claude).resolve();assert path.is_file()
        old=json.loads((STATE/'local.json').read_text()) if (STATE/'local.json').exists() else {}
        old.update(claude=str(path),total_budget_usd=args.total_budget);save(STATE/'local.json',old)
        print('Configured existing CLI; user application settings unchanged')
    elif args.action=='enqueue':print(json.dumps(enqueue(args.id,Path(args.prompt_file).read_text(encoding='utf-8'),args.output,args.budget,args.timeout),indent=2))
    elif args.action=='run':run(args.id)
    else:
        files=[location(args.id)/'job.json'] if args.id else sorted((STATE/'jobs').glob('*/job.json'))
        for file in files:
            job=json.loads(file.read_text());print(json.dumps({k:job.get(k) for k in ('id','status','model','reported_cost_usd','error')},indent=2))

if __name__=='__main__':main()
