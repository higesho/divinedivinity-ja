# drop common English words from text/glossary_auto.json (single-word "names" like Death, Eye, Guard)
import json, os, re, collections
ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), '..'))
S = json.load(open(os.path.join(ROOT, 'work', 'strings.json'), encoding='utf-8'))
low = collections.Counter()
for x in S:
    if x['src'] in ('dialog', 'books.txt', 'mono'):
        low.update(re.findall(r"\b[a-z][a-z']+\b", x['en']))
p = os.path.join(ROOT, 'text', 'glossary_auto.json')
g = json.load(open(p, encoding='utf-8'))
out = {}
for en, ja in g.items():
    words = en.split()
    if all(low[w.lower()] >= 2 for w in words): continue   # made of ordinary words: not a fixed term
    out[en] = ja
json.dump(out, open(p, 'w', encoding='utf-8'), ensure_ascii=False, indent=0)
print(len(g), '->', len(out))
