"""Offline controller checks with a fake worker, plus a real isolated worktree."""
import importlib.util,json,subprocess,sys,uuid
from pathlib import Path
from unittest.mock import patch

HERE=Path(__file__).resolve().parent
spec=importlib.util.spec_from_file_location('bridge',HERE/'bridge.py');b=importlib.util.module_from_spec(spec);spec.loader.exec_module(b)

def main():
    state=b.ROOT/'extract/codex/bridge-tests'/uuid.uuid4().hex[:12];state.mkdir(parents=True)
    count=0
    def check(condition):
        nonlocal count
        assert condition;count+=1
    for invalid in ('../a','C:/outside','notes/30-native-audio.md','tools/codex/../../bad','tools/codex/.git/x'):
        try:b.relative(invalid)
        except ValueError:check(True)
        else:raise AssertionError(invalid)
    with patch.object(b,'STATE',state):
        b.save(state/'local.json',dict(claude=sys.executable,total_budget_usd=5))
        job=b.enqueue('probe','Create the permitted file',['tools/codex/bridge_probe.txt'],.25,20)
        folder=b.location('probe');b.save(folder/'mcp.json',{'mcpServers':{}})
        cmd=b.command(job,folder)
        for option in ('--restricted','--safe-mode','--no-session-persistence','--strict-mcp-config'):check(option in cmd)
        check(cmd[cmd.index('--model')+1]=='claude-sonnet-5-5')
        check('Bash' not in cmd[cmd.index('--tools')+1])
        check('--dangerously-skip-permissions' not in cmd)
        fake=state/'fake.py'
        fake.write_text('''import json,sys
from pathlib import Path
prompt=sys.stdin.read()
Path('tools/codex/bridge_probe.txt').write_text('BRIDGE_OK\\n')
print(json.dumps(dict(result='Completed',is_error=False,total_cost_usd=.01,modelUsage={'claude-sonnet-5-5':{'inputTokens':10,'outputTokens':3}},session_id='test')))
''')
        before=b.git('status','--porcelain')
        with patch.object(b,'command',lambda job,folder:[sys.executable,str(fake)]):result=b.run('probe')
        check(result['status']=='review_required');check(result['reported_cost_usd']==.01)
        check(result['deliverables'][0]['primary_unchanged'])
        check((folder/'deliverables/tools/codex/bridge_probe.txt').read_text()=='BRIDGE_OK\n')
        check(not (b.ROOT/'tools/codex/bridge_probe.txt').exists())
        check(before==b.git('status','--porcelain'))
        try:b.run('probe')
        except RuntimeError:check(True)
        else:raise AssertionError('Repeated job accepted')
        work=Path(result['worktree']);(work/'tools/codex/forbidden.txt').write_text('no')
        try:b.collect(folder,result)
        except RuntimeError:check(True)
        else:raise AssertionError('Out-of-scope output accepted')
        job=b.enqueue('expensive','No model invocation',['tools/codex/never.txt'],6,20)
        try:b.allowance(job)
        except RuntimeError:check(True)
        else:raise AssertionError('Budget exceeded')
        for name,payload in (
            ('wrong-model',dict(result='wrong model',is_error=False,total_cost_usd=.01,modelUsage={'claude-opus-5-5':{}})),
            ('auth-error',dict(result='Sign-in expired',is_error=True,total_cost_usd=0,modelUsage={})),
        ):
            b.enqueue(name,'Do nothing',['tools/codex/unused.txt'],.25,20)
            fake.write_text('import sys,json\nsys.stdin.read()\nprint('+repr(json.dumps(payload))+')\n')
            with patch.object(b,'command',lambda job,folder:[sys.executable,str(fake)]):result=b.run(name)
            check(result['status']=='blocked');check(not (b.location(name)/'deliverables.json').exists())
        b.enqueue('timeout','Do nothing',['tools/codex/unused.txt'],.25,1)
        fake.write_text('import sys,time\nsys.stdin.read()\ntime.sleep(10)\n')
        with patch.object(b,'command',lambda job,folder:[sys.executable,str(fake)]):result=b.run('timeout')
        check(result['status']=='blocked');check('time limit' in result['error']);check(not (state/'worker.lock').exists())
        report=dict(checks=count,failures=0,scope='Offline fake worker plus real isolated worktree, model flags, scope audit, primary-tree preservation and budget admission. Authentication separately live-tested.')
        b.save(state/'validation.json',report)
    print(json.dumps(report,indent=2));print('Artifacts:',state)

if __name__=='__main__':main()
