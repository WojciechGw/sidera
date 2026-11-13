"""Optional maintainer command: refresh independent JPL DE441 test fixtures.
Normal tests are offline. Horizons AIRLESS, topocentric, UTC/UT1, 395 m.
https://ssd-api.jpl.nasa.gov/doc/horizons.html
"""
import datetime as dt
import argparse
import json
import urllib.parse
import urllib.request
from pathlib import Path

parser=argparse.ArgumentParser(description=__doc__)
parser.add_argument('--output',type=Path,default=Path(__file__).resolve().parents[1]/'build/references/horizons.json')
output=parser.parse_args().output

dates=[dt.datetime(y,m,d,h) for y in range(1900,2201,25)
       for m,d,h in [(1,1,0),(3,21,6),(6,21,12),(10,1,18)]]
dates += [dt.datetime(2026,11,8,12),dt.datetime(2038,1,19,4),
          dt.datetime(2100,2,28,23),dt.datetime(2200,12,31,23)]
q={'format':'json','EPHEM_TYPE':"'OBSERVER'",'CENTER':"'coord@399'",
   'COORD_TYPE':"'GEODETIC'",'SITE_COORD':"'19.155472222,49.6485,0.395'",
   'QUANTITIES':"'4,10'",'CSV_FORMAT':"'YES'",'APPARENT':"'AIRLESS'",
   'TLIST':"'"+' '.join(str(d.toordinal()+1721424.5+d.hour/24) for d in dates)+"'"}
data={'source':'JPL Horizons DE441, AIRLESS topocentric, retrieved 2026-11-08',
      'url':'https://ssd-api.jpl.nasa.gov/doc/horizons.html',
      'observer':[1787346,689597,395], 'cases':[]}
rows={}
for body in ['10','301']:
    q['COMMAND']="'"+body+"'"
    with urllib.request.urlopen('https://ssd.jpl.nasa.gov/api/horizons.api?'+urllib.parse.urlencode(q),timeout=45) as f:
        result=json.load(f)['result']
    if '$$SOE' not in result: raise RuntimeError(result)
    table=result.split('$$SOE')[1].split('$$EOE')[0].strip().splitlines()
    # Horizons sorts TLIST chronologically, including out-of-order additions.
    rows[body]=[[float(s.strip()) for s in line.split(',')[3:6]] for line in table]
for i,d in enumerate(sorted(dates)):
    data['cases'].append({'date':d.strftime('%Y-%m-%d %H:%M:%S'),
                          'sun':rows['10'][i], 'moon':rows['301'][i]})
output.parent.mkdir(parents=True,exist_ok=True)
output.write_text(json.dumps(data,indent=2)+'\n')
print('Stored',len(data['cases']),'independent reference cases')
