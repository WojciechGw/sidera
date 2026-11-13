"""Compare local events with archived USNO daily data; no network during tests."""
import datetime as dt
import json
from pathlib import Path
import subprocess
import sys

root=(Path(__file__).resolve().parents[1] / 'src')
cases=json.loads((root.parent/'tests/riseset_usno.json').read_text())
lines=[]
for case in cases:
    d=dt.date.fromisoformat(case['date'])
    # Noon UTC lies on the same local date in these reference locations.
    lines.append(f"{case['zone']} {d.year} {d.month} {d.day} 12 0 "
                 f"{round(case['latitude']*36000)} {round(case['longitude']*36000)}")
result=subprocess.run([sys.argv[1],str(root/'rom')],input='\n'.join(lines)+'\n',
                      text=True,capture_output=True,check=True)
rows=result.stdout.splitlines()
assert len(rows)==len(cases),(result.stdout,result.stderr)
worst=0
for case,row in zip(cases,rows):
    sun,moon,steps,calls,state=row.split('|')
    assert state=='2',row
    assert int(steps)<2000 and int(calls)<1600,row
    for text,key in ((sun,'sundata'),(moon,'moondata')):
        ref={event['phen']:event['time'] for event in case['usno']['properties']['data'][key]}
        for kind,actual in zip(('Rise','Set'),(text[5:10],text[15:20])):
            expected=ref.get(kind)
            if expected is None:
                assert actual=='--:--',(case['name'],text,ref)
            else:
                assert actual!='--:--',(case['name'],text,ref)
                def minutes(t):
                    h,m=map(int,t.split(':'));return h*60+m
                error=abs(minutes(actual)-minutes(expected));worst=max(worst,error)
                assert error<=3,(case['name'],case['date'],kind,actual,expected)
        for phrase,marker in (('above',' UP'),('below',' DOWN')):
            if f'Object continuously {phrase} the Horizon' in ref:assert text.endswith(marker),text
    print(case['name'],case['date'],sun,moon,f'({calls} ephemerides)')
print(f'PASS: {len(cases)} USNO local days; maximum sampled difference {worst} minutes')
# Missing coverage at the UTC range edges must never claim a polar day/night.
extra=[
    'Europe/Warsaw 1900 1 1 12 0 1880272 756424',
    'Etc/UTC 2200 12 31 12 0 1880272 756424',
    'Europe/Warsaw 1900 1 2 12 0 1880272 756424',
    'Europe/Warsaw 2200 12 30 12 0 1880272 756424',
    # Different UTC dates, same Kathmandu civil date: the second result is cached.
    'Asia/Kathmandu 2026 11 9 19 0 997261 3071542',
    'Asia/Kathmandu 2026 11 10 12 0 997261 3071542',
    # Lord Howe has a 30-minute daylight saving transition.
    'Australia/Lord_Howe 2026 10 4 1 0 -1135800 5727240',
]
out=subprocess.run([sys.argv[1],str(root/'rom')],input='\n'.join(extra)+'\n',
                   text=True,capture_output=True,check=True).stdout.splitlines()
for row in out[:2]:assert row.split('|')[-1]=='3' and row.count(' N/A')==2,row
for row in out[2:]:assert row.split('|')[-1]=='2',row
assert out[4].split('|')[:2]==out[5].split('|')[:2]
assert out[5].split('|')[2:4]==['0','0'],out[5]
print('PASS: date boundaries, local/UTC day difference, 30-minute DST, same-day cache')
