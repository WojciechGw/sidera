"""Build the offline city catalogue. Optional --import-dir refreshes GeoNames.

The import directory contains countryInfo.txt, cities15000.txt and PL.txt from
https://download.geonames.org/export/dump/ (CC BY 4.0).
Normal builds use the reviewed data/cities.json without network access.
"""
import argparse
import datetime as dt
import json
from pathlib import Path
import struct
import unicodedata

ROOT=(Path(__file__).resolve().parents[1] / 'src')
COUNTRIES='''AD AE AF AG AL AM AO AR AT AU AZ BA BB BD BE BF BG BH BI BJ BN BO BR BS BT BW BY BZ
CA CD CF CG CH CI CL CM CN CO CR CU CV CY CZ DE DJ DK DM DO DZ EC EE EG ER ES ET FI FJ FM FR
GA GB GD GE GH GM GN GQ GR GT GW GY HN HR HT HU ID IE IL IN IQ IR IS IT JM JO JP KE KG KH KI
KM KN KP KR KW KZ LA LB LC LI LK LR LS LT LU LV LY MA MC MD ME MG MH MK ML MM MN MR MT MU MV
MW MX MY MZ NA NE NG NI NL NO NP NR NZ OM PA PE PG PH PK PL PS PT PW PY QA RO RS RU RW SA SB
SC SD SE SG SI SK SL SM SN SO SR SS ST SV SY SZ TD TG TH TJ TL TM TN TO TR TT TV TZ UA UG US
UY UZ VA VC VE VN VU WS YE ZA ZM ZW'''.split()
assert len(set(COUNTRIES))==195

# Additional capital/seat-of-government locations. First capital remains the
# country-code default; role notes are retained in the source JSON.
SECONDARY={
 'BJ':['Cotonou'],'BO':['La Paz'],'CI':['Abidjan'],'LK':['Colombo'],
 'MY':['Putrajaya'],'NL':['The Hague'],'PS':['Ramallah'],'SZ':['Lobamba'],
 'ZA':['Cape Town','Bloemfontein'],
}
EXTRAS={
 'PL':['Bialystok','Bydgoszcz','Gdansk','Gorzow Wielkopolski','Katowice','Kielce',
       'Krakow','Lodz','Lublin','Olsztyn','Opole','Poznan','Rzeszow','Szczecin',
       'Torun','Wroclaw','Zielona Gora','Sosnowiec','Radziechowy'],
 'US':['New York City','Chicago','Denver','Los Angeles','Anchorage','Honolulu','Phoenix'],
 'CA':["St. John's",'Halifax','Toronto','Winnipeg','Calgary','Vancouver','Yellowknife','Whitehorse'],
 'AU':['Sydney','Brisbane','Adelaide','Darwin','Perth','Hobart'],
 'CN':['Shanghai','Guangzhou','Harbin','Chengdu','Urumqi','Lhasa'],
 'RU':['Kaliningrad','Saint Petersburg','Samara','Yekaterinburg','Omsk','Novosibirsk','Krasnoyarsk',
       'Irkutsk','Yakutsk','Chita','Vladivostok','Magadan','Petropavlovsk-Kamchatsky'],
 'BR':['Sao Paulo','Rio de Janeiro','Manaus','Belem','Recife','Porto Alegre','Rio Branco','Cuiaba'],
 'IN':['Mumbai','Kolkata','Chennai','Bengaluru','Srinagar','Guwahati'],
 'ID':['Medan','Denpasar','Makassar','Jayapura'],
 'AR':['Cordoba','Mendoza','Salta','Ushuaia'],
 'KZ':['Almaty','Aktau','Atyrau','Oskemen','Shymkent'],
 'MX':['Tijuana','Hermosillo','Ciudad Juarez','Cancun','Merida','Guadalajara'],
 'CL':['Arica','Punta Arenas'],
 'PE':['Iquitos','Arequipa','Cusco'],
 'CO':['Cartagena','Leticia','Cali'],
 'DZ':['Oran','Tamanrasset'],
 'CD':['Lubumbashi','Kisangani','Goma'],
 'SD':['Port Sudan','Nyala'],
 'SA':['Jeddah','Dammam','Tabuk'],
 'IR':['Tabriz','Mashhad','Bandar Abbas'],
 'MN':['Olgii','Choibalsan'],
 'PK':['Karachi','Quetta','Lahore'],
 'NZ':['Auckland','Christchurch','Invercargill'],
 'JP':['Sapporo','Osaka','Naha'],
 'NO':['Bergen','Tromso'],
 'SE':['Gothenburg','Kiruna'],
 'FI':['Oulu','Rovaniemi'],
 'ES':['Barcelona','Seville','Las Palmas de Gran Canaria'],
 'PT':['Ponta Delgada','Funchal'],
 'ZA':['Durban','Johannesburg'],
 'YE':['Aden'],
 'BI':['Bujumbura'],
 'TZ':['Dar es Salaam'],
}
# English display names, including familiar spellings accepted by /location.
RENAMES={'New York City':'New York','Washington, D.C.':'Washington'}
PRIMARY={'PW':'Ngerulmud','LK':'Sri Jayewardenepura Kotte'}
COUNTRY_NAMES={'PS':'Palestine','VA':'Holy See'}
# GeoNames has no DEM height for these four coastal points. Copernicus GLO-90
# via Open-Meteo Elevation API, retrieved 2026-11-09 at the same coordinates.
HEIGHTS={3874787:12,2198148:7,2993458:44,3133895:14}
SEATS={('BJ','Cotonou'),('BO','La Paz'),('CI','Abidjan'),('MY','Putrajaya'),
       ('NL','The Hague'),('PS','Ramallah')}
# Pin populated places, not similarly named villages or administrative areas.
# GeoNames admin1 83 = Slaskie; Radziechowy is the village near Zywiec.
POLISH_PLACES={
 'Bialystok':(776069,'81'),'Bydgoszcz':(3102014,'73'),'Gdansk':(3099434,'82'),
 'Gorzow Wielkopolski':(3098722,'76'),'Katowice':(3096472,'83'),'Kielce':(769250,'84'),
 'Krakow':(3094802,'77'),'Lodz':(3093133,'74'),'Lublin':(765876,'75'),
 'Olsztyn':(763166,'85'),'Opole':(3090048,'79'),'Poznan':(3088171,'86'),
 'Rzeszow':(759734,'80'),'Szczecin':(3083829,'87'),'Torun':(3083271,'73'),
 'Wroclaw':(3081368,'72'),'Zielona Gora':(3080165,'76'),
 'Sosnowiec':(3085128,'83'),'Radziechowy':(3087431,'83'),
}

def ascii_name(s):
    s=s.replace('’',"'").replace('‘',"'").replace('ʻ',"'")
    return unicodedata.normalize('NFKD',s).encode('ascii','ignore').decode()

def imported(directory):
    countries={r[0]:r for line in (directory/'countryInfo.txt').read_text(encoding='utf8').splitlines()
               if line and not line.startswith('#') for r in [line.split('\t')] if r[0] in COUNTRIES}
    cities=[line.split('\t') for line in (directory/'cities15000.txt').read_text(encoding='utf8').splitlines()]
    polish={int(r[0]):r for line in (directory/'PL.txt').read_text(encoding='utf8').splitlines()
            for r in [line.split('\t')]}
    result=[];issues=[]
    def find(code,name):
        if code=='PL' and name in POLISH_PLACES:
            ident,region=POLISH_PLACES[name];r=polish[ident]
            assert r[8]=='PL' and r[6]=='P' and r[10]==region,(name,r)
            return r
        match=[r for r in cities if r[8]==code and ascii_name(name).casefold() in
               [ascii_name(s).casefold() for s in [r[1],r[2],*r[3].split(',')]]]
        if not match:issues.append(f'Missing {code}: {name}');return None
        return max(match,key=lambda r:(r[7]=='PPLC',int(r[14])))
    def add(code,name,capital=False,default=False):
        r=find(code,name)
        if r is None:return
        if any(c['code']==code and c['geonames_id']==int(r[0]) for c in result):return
        display=ascii_name(RENAMES.get(r[2],name))
        height=HEIGHTS.get(int(r[0]),r[15] or r[16])
        regional=code=='PL' and name in POLISH_PLACES
        if regional:height=r[16] # terrain at the reference point, not a city-wide height
        if not height or int(height)==-9999:
            issues.append(f'Missing elevation {code} {name}: id={r[0]} lat={r[4]} lon={r[5]}');return
        seat=(code,name) in SEATS or (default and code in ('NR','CH'))
        result.append(dict(code=code,country=ascii_name(COUNTRY_NAMES.get(code,countries[code][4])),city=display,
            latitude=float(r[4]),longitude=float(r[5]),altitude=int(height),
            capital=capital and not seat,seat=seat,default=default,geonames_id=int(r[0]),
            elevation_source='Open-Meteo Copernicus GLO-90' if int(r[0]) in HEIGHTS else 'GeoNames elevation' if r[15] else 'GeoNames DEM (SRTM3/GTOPO30)',
            timezone=r[17],source_date=r[18]))
        if regional:
            result[-1].update(admin1=r[10],elevation_source='GeoNames DEM (SRTM3/GTOPO30)')
    for code in COUNTRIES:
        add(code,PRIMARY.get(code,countries[code][5]),True,True)
        for name in SECONDARY.get(code,[]):add(code,name,True)
        for name in EXTRAS.get(code,[]):add(code,name)
    if issues:raise ValueError('\n'.join(issues))
    return {'source':'GeoNames cities15000.txt, PL.txt and countryInfo.txt; CC BY 4.0',
            'url':'https://download.geonames.org/export/dump/',
            'retrieved':dt.datetime.now(dt.timezone.utc).date().isoformat(),
            'countries':'193 UN members plus Holy See and State of Palestine',
            'cities':sorted(result,key=lambda c:(c['country'].casefold(),not c['default'],c['city'].casefold()))}

def build(data):
    from timezones import zone_names
    zones=zone_names()
    rows=data['cities'];seen=set()
    assert {r['code'] for r in rows}==set(COUNTRIES)
    for code in COUNTRIES:assert sum(r['default'] for r in rows if r['code']==code)==1,code
    output=bytearray(struct.pack('<8sHHI',b'SIDCITY2',len(rows),96,0))
    for r in rows:
        key=(r['code'],r['city'].lower());assert key not in seen,key;seen.add(key)
        assert len(r['country'])<40 and len(r['city'])<40,r
        assert -90<=r['latitude']<=90 and -180<=r['longitude']<=180 and -500<=r['altitude']<=10000,r
        output+=struct.pack('<3s40s40siihBH',r['code'].encode('ascii'),r['country'].encode('ascii'),
                            r['city'].encode('ascii'),round(r['latitude']*36000),round(r['longitude']*36000),
                            r['altitude'],int(r['default'])+2*int(r['capital'])+4*int(r['seat']),zones.index(r['timezone']))
    return output

if __name__=='__main__':
    p=argparse.ArgumentParser();p.add_argument('--import-dir',type=Path)
    p.add_argument('--output',type=Path,default=ROOT.parent/'build/generated/rom/sidera.cty');a=p.parse_args()
    a.output.parent.mkdir(parents=True,exist_ok=True)
    source=ROOT/'data/cities.json'
    if a.import_dir:
        source=a.output.parent.parent/'data/cities.json';source.parent.mkdir(parents=True,exist_ok=True)
        source.write_text(json.dumps(imported(a.import_dir),ensure_ascii=False,indent=2)+'\n',encoding='utf8')
    data=json.loads(source.read_text(encoding='utf8'))
    output=build(data);a.output.write_bytes(output)
    print(len(data['cities']),'cities in 195 countries;',len(output),'bytes')
