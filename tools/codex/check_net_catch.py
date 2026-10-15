"""Require a real net contact, capture animation and persistence in the viewer host."""
import json
from check_viewer_game import ROOT, standalone, run


def main():
    output=ROOT/'extract/codex-task/net_catch'
    output.mkdir(parents=True,exist_ok=True)
    host=standalone('tools/codex/net_catch_harness.c','net_catch_host')
    result=run([host],output/'host.log')
    line=next(line for line in result.splitlines() if line.startswith('NET '))
    assert 'caught=1' in line and 'capture=1 removed=1' in line and 'saved=2' in line,line
    (output/'report.json').write_text(json.dumps(dict(failures=0,result=line),indent=2)+'\n')
    print(line)


if __name__=='__main__':main()
