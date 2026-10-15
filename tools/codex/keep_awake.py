"""Keep this Windows session awake until a UTC deadline; no persistent power changes."""
import argparse,ctypes,json,os,time
from datetime import datetime,timezone
from pathlib import Path

def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--until',required=True);p.add_argument('--status',required=True);a=p.parse_args()
    deadline=datetime.fromisoformat(a.until).timestamp();status=Path(a.status)
    set_state=ctypes.windll.kernel32.SetThreadExecutionState
    set_state.argtypes=[ctypes.c_uint32];set_state.restype=ctypes.c_uint32
    if not set_state(0x80000001):raise OSError('Windows rejected temporary system-awake request')
    status.write_text(json.dumps(dict(pid=os.getpid(),until=a.until,started=datetime.now(timezone.utc).isoformat(),state='active'),indent=2)+'\n')
    try:
        while time.time()<deadline:time.sleep(min(30,max(0,deadline-time.time())))
    finally:
        set_state(0x80000000)
        status.write_text(json.dumps(dict(pid=os.getpid(),until=a.until,state='finished'),indent=2)+'\n')

if __name__=='__main__':main()
