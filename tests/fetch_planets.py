"""Optional maintainer download; normal planet tests use the saved offline file."""
import datetime as dt
import argparse
import json
from pathlib import Path
import urllib.parse
import urllib.request

here=Path(__file__).resolve().parent
parser=argparse.ArgumentParser(description=__doc__)
parser.add_argument('--output',type=Path,default=here.parent/'build/references/planets_horizons.json')
output=parser.parse_args().output
dates=[dt.datetime.strptime(c['date'],'%Y-%m-%d %H:%M:%S')
       for c in json.loads((here/'horizons.json').read_text())['cases']]
q={'format':'json','EPHEM_TYPE':"'OBSERVER'",'CENTER':"'coord@399'",
   'COORD_TYPE':"'GEODETIC'",'SITE_COORD':"'19.155472222,49.6485,0.395'",
   'QUANTITIES':"'4'",'CSV_FORMAT':"'YES'",'APPARENT':"'AIRLESS'",
   'TLIST':"'"+' '.join(str(d.toordinal()+1721424.5+d.hour/24+d.minute/1440+d.second/86400)
                         for d in dates)+"'"}
data={'source':'JPL Horizons, AIRLESS topocentric',
      'retrieved':dt.datetime.now(dt.timezone.utc).date().isoformat(),
      'url':'https://ssd-api.jpl.nasa.gov/doc/horizons.html',
      'observer':[1787346,689597,395],
      'names':['Mercury','Venus','Mars','Jupiter','Saturn','Uranus'],
      'cases':[{'date':d.strftime('%Y-%m-%d %H:%M:%S'),'planets':[]} for d in sorted(dates)]}
for name,body in zip(data['names'],[199,299,499,599,699,799]):
    q['COMMAND']="'"+str(body)+"'"
    with urllib.request.urlopen('https://ssd.jpl.nasa.gov/api/horizons.api?'+urllib.parse.urlencode(q),timeout=45) as f:
        result=json.load(f)['result']
    if '$$SOE' not in result:raise RuntimeError(result)
    rows=result.split('$$SOE')[1].split('$$EOE')[0].strip().splitlines()
    assert len(rows)==len(data['cases'])
    for case,row in zip(data['cases'],rows):
        case['planets'].append([float(v.strip()) for v in row.split(',')[3:5]])
    print(name,len(rows),flush=True)
output.parent.mkdir(parents=True,exist_ok=True)
output.write_text(json.dumps(data,indent=2)+'\n')
