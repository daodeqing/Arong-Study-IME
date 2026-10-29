"""Rebuild the bundled CET lexicon from the SHA256-pinned ECDICT source."""
from pathlib import Path
import sys,urllib.request,hashlib,json,csv,re,io
sys.stdout.reconfigure(encoding='utf-8')
root=Path(__file__).resolve().parents[1]
lock=json.loads((root/'scripts/dictionary_build/inputs.lock.json').read_text(encoding='utf-8'))['external']['ecdict.csv']
with urllib.request.urlopen(lock['url'],timeout=60) as response:
 data=response.read()
assert len(data)==lock['size'] and hashlib.sha256(data).hexdigest()==lock['sha256']
csv.field_size_limit(1<<30)
entries=[]
for row in csv.DictReader(io.StringIO(data.decode('utf-8-sig'))):
 tags=row.get('tag','').split(); word=row['word'].lower()
 if not re.fullmatch('[a-z]+',word) or not {'cet4','cet6'}.intersection(tags):continue
 translation=row.get('translation','').replace('\\n','\n')
 gloss=translation.splitlines()[0] if translation else ''
 match=re.match(r'^([a-z./ ]+)\.\s*(.+)',gloss)
 pos=match.group(1)+'.' if match else row.get('pos','')
 gloss=match.group(2) if match else gloss
 gloss=re.split('[；;]',gloss)[0][:64]
 if not gloss:continue
 forms=[]
 for item in row.get('exchange','').split('/'):
  if ':' in item:
   tag,value=item.split(':',1)
   if tag not in ['0','1'] and re.fullmatch('[a-z]+',value):forms.append(value)
 entries.append({'word':word,'level':'CET6' if 'cet6' in tags and 'cet4' not in tags else 'CET4',
  'phonetic':row.get('phonetic',''),'pos':pos,'gloss':gloss,'forms':sorted(set(forms))})
out=root/'server/assets/vocabulary';out.mkdir(parents=True,exist_ok=True)
payload={'schema':1,'source':lock['url'],'source_sha256':lock['sha256'],'entries':sorted(entries,key=lambda x:x['word'])}
(out/'cet.json').write_text(json.dumps(payload,ensure_ascii=False,separators=(',',':'))+'\n',encoding='utf-8')
with urllib.request.urlopen(lock['url'].rsplit('/',1)[0]+'/LICENSE',timeout=30) as response:license=response.read().decode('utf-8')
(out/'ECDICT-LICENSE.txt').write_text(license,encoding='utf-8')
print('Generated CET vocabulary',len(entries),'entries,', (out/'cet.json').stat().st_size,'bytes; verified source SHA256',lock['sha256'])
