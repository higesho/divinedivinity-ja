# translations (en -> ja, UTF-8 json) -> build/ddjp.bin : "DDJT", n, sorted (u64 fnv1a(en latin-1), u32 off), EUC blob
import json, os, struct, sys
ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), '..'))

def fnv64(b):
    h = 1469598103934665603
    for c in b:
        h ^= c
        h = (h * 1099511628211) & 0xffffffffffffffff
    return h

def key_bytes(en):
    # dialog text is UTF-16: the DLL maps the few cp1252 punctuation marks back to their byte
    try: return en.encode('latin-1')
    except UnicodeEncodeError: return en.encode('cp1252')

def to_euc(s):
    # characters EUC-JP lacks
    s = s.replace('—', '―').replace('～', '〜').replace('－', '−').replace(' ', ' ')
    return s.encode('euc_jp')

def build(tr, out):
    ents = {}
    blob = bytearray()
    for en, ja in tr.items():
        if not ja: continue
        h = fnv64(key_bytes(en))
        if h in ents: continue
        ents[h] = len(blob)
        blob += to_euc(ja) + b'\0'
    keys = sorted(ents)
    data = b'DDJT' + struct.pack('<I', len(keys)) + b''.join(struct.pack('<QI', k, ents[k]) for k in keys) + bytes(blob)
    open(out, 'wb').write(data)
    print('entries', len(keys), 'bytes', len(data))

if __name__ == '__main__':
    src = sys.argv[1] if len(sys.argv) > 1 else os.path.join(ROOT, 'build', 'tr.json')
    build(json.load(open(src, encoding='utf-8')), os.path.join(ROOT, 'build', 'ddjp.bin'))

def build_exe(out):
    src = os.path.join(ROOT, 'text', 'exe_ja.json')
    d = json.load(open(src, encoding='utf-8')) if os.path.exists(src) else {}
    b = bytearray()
    for en, ja in d.items():
        b += en.encode('latin-1') + b'\0' + to_euc(ja) + b'\0'
    b += b'\0'
    open(out, 'wb').write(bytes(b))
    print('exe strings', len(d))

build_exe(os.path.join(ROOT, 'build', 'ddjp_exe.bin'))
