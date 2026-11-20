# Extract every translatable English string -> work/strings.json
#   [{"src": file, "ctx": context id, "en": text}], unique by (en) with all sources kept
import os,sys,json,struct,re
sys.path.insert(0,os.path.dirname(__file__))
import cmp,fmt_text,lps
G=os.path.abspath(os.path.join(os.path.dirname(__file__),'..','..'))
ROOT=os.path.abspath(os.path.join(os.path.dirname(__file__),'..'))
out=[]
def add(src,ctx,en):
    if en is None: return
    if not re.search(r'[A-Za-z]{2}',en): return
    out.append({'src':src,'ctx':ctx,'en':en})

# 1. text.cmp
for gid,wide,ents in fmt_text.read(os.path.join(G,'localizations/english/text.cmp')):
    for eid,t1,t2 in ents:
        add('text',f'{gid}:{eid}',t1); add('text',f'{gid}:{eid}b',t2)
# 2. dialogtxt.dat
def dialog_strings():
    d=open(os.path.join(G,'localizations/english/dialogtxt.dat'),'rb').read()
    a,b,n=struct.unpack_from('<3I',d,0);base=12+8*n;res=[]
    for i in range(n):
        off,ln=struct.unpack_from('<2I',d,12+8*i);raw=d[base+off:base+off+ln]
        res.append(''.join(chr(c-1) for c in struct.unpack('<%dH'%(ln//2),raw)))
    return res
for i,s in enumerate(dialog_strings()): add('dialog',i,s)
# 3. mono.cmp / itemgen.cmp (length-prefixed strings)
for src,p in (('mono','localizations/english/mono.cmp'),('itemgen','dat/English/itemgen.cmp')):
    for off,t in lps.lpstrings(open(os.path.join(G,p),'rb').read()):
        s=t.decode('latin1')
        if s=='Divinity Monologues V1.0': continue
        add(src,off,s)
# 4. line based text files
def lines(src,data):
    for i,l in enumerate(data.decode('latin1').split('\n')):
        l=l.rstrip('\r')
        if re.match(r'^\s*-?\d+(\s+-?\d+)*\s*$',l) or l.startswith('[bookentry]') or l=='<not defined>': continue
        add(src,i,l)
lines('skills',open(os.path.join(G,'localizations/english/skills.txt'),'rb').read())
lines('hints',open(os.path.join(G,'localizations/english/hints.txt'),'rb').read())
d,o,ents=cmp.read(os.path.join(G,'dat/global.cmp'))
for name,off,size,c,f in ents:
    if name.lower().startswith('dat/english/'.replace('/',chr(92))):
        b=d[off:off+size]
        if c: b=cmp.xor(b)
        base=os.path.basename(name).lower()
        if base=='teleporter.txt':
            ls=b.decode('latin1').split('\n')
            for i,l in enumerate(ls):
                l=l.rstrip('\r')
                if not l.lower().startswith('teleporter') and not re.match(r'^\s*-?\d+\s*$',l): add(base,i,l)
        elif base=='diary_monsterlog.txt':
            for i,l in enumerate(b.decode('latin1').split('\n')):
                l=l.rstrip('\r')
                if re.match(r'^\S+\s+\d+\s*$',l): continue
                add(base,i,l)
        else: lines(base,b)
json.dump(out,open(os.path.join(ROOT,'work','strings.json'),'w',encoding='utf-8'),ensure_ascii=False,indent=0)
from collections import Counter
c=Counter(x['src'] for x in out); u={}
for x in out: u.setdefault(x['en'],x)
print(c, 'total',len(out),'unique',len(u),'chars',sum(len(k) for k in u))
cu=Counter(x['src'] for x in u.values())
print('unique by first src',cu, {s:sum(len(k) for k,v in u.items() if v['src']==s) for s in cu})
