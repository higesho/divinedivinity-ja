# text/chunks + text/ja -> build/tr.json (English -> Japanese) and text/glossary_auto.json (names from wave 1)
import os, json, re, glob, sys
ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), '..'))
SPEC = re.compile(r'%[-+ #0]*\d*(?:\.\d+)?[dsciuxXf]')

def norm(en, ja, kind):
    j = ja.strip() if en == en.strip() else ja
    # a bare % would break printf-style strings
    j = re.sub(r'%(?![-+ #0]*\d*(?:\.\d+)?[dsciuxXf])', '％', j)
    # "Strength %d" -> "筋力 %d": keep the gap between a label and its number
    if kind == 'UI / system message':
        j = re.sub(r'([぀-ヿ一-鿿）])(%[-+ #0]*\d*[dsc])', lambda m: m.group(1) + ' ' + m.group(2)
                   if re.search(r'[A-Za-z:] %[-+ #0]*\d*[dsc]', en) else m.group(0), j)
    if kind and kind.startswith('random item name part') and en.lower().startswith('of '):
        core = re.sub(r'^[・の（(\s]+|[）)\s]+$', '', j)
        j = '（%s）' % core
    return j

def main():
    tr, kinds = {}, {}
    names = {}
    for p in sorted(glob.glob(os.path.join(ROOT, 'text', 'chunks', '*.json'))):
        name = os.path.splitext(os.path.basename(p))[0]
        q = os.path.join(ROOT, 'text', 'ja', name + '.json')
        if not os.path.exists(q): continue
        ch = json.load(open(p, encoding='utf-8'))
        ja = {u['id']: u['ja'] for u in json.load(open(q, encoding='utf-8')) if isinstance(u, dict) and 'id' in u}
        for u in ch['units']:
            j = ja.get(u['id'])
            if not isinstance(j, str) or not j: continue
            j = norm(u['en'], j, u.get('kind'))
            tr[u['en']] = j
            k = u.get('kind', '')
            if k.startswith(('NPC', 'place', 'weapon', 'item /')) and len(u['en']) <= 40:
                names[u['en']] = j
    rp = os.path.join(ROOT, 'text', 'replace.json')
    if os.path.exists(rp):
        for rule in json.load(open(rp, encoding='utf-8')):
            a, b = rule[0], rule[1]
            ctx = rule[2] if len(rule) > 2 else None
            for en in tr:
                if a in tr[en] and (ctx is None or ctx in en): tr[en] = tr[en].replace(a, b)
    # resource paths / internal names stay English
    for en in list(tr):
        if chr(92) in en and re.search(r'[.](tga|bmp|wav|ogg)$', en, re.I): del tr[en]
    fx = os.path.join(ROOT, 'text', 'fixes.json')
    if os.path.exists(fx):
        for k, v in json.load(open(fx, encoding='utf-8')).items():
            tr[k] = v
            if k in names: names[k] = v
    os.makedirs(os.path.join(ROOT, 'build'), exist_ok=True)
    json.dump(tr, open(os.path.join(ROOT, 'build', 'tr.json'), 'w', encoding='utf-8'), ensure_ascii=False, indent=0)
    json.dump(names, open(os.path.join(ROOT, 'text', 'glossary_auto.json'), 'w', encoding='utf-8'), ensure_ascii=False, indent=0)
    allen = {x['en'] for x in json.load(open(os.path.join(ROOT, 'work', 'strings.json'), encoding='utf-8'))}
    print('translations', len(tr), 'of', len(allen), 'names', len(names))

if __name__ == '__main__':
    main()
