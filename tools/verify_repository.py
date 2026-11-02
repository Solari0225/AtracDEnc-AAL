"""Check public document links, fixture hashes, versioning and source contents."""
import hashlib, json, pathlib, re
root = pathlib.Path(__file__).resolve().parents[1]
excluded = {'bin', 'build', 'dist', '.git', '__pycache__', '.venv'}
files = [p for p in root.rglob('*') if p.is_file() and not (set(p.relative_to(root).parts) & excluded)]
errors = []
version = (root/'VERSION').read_text().strip()
for name, item in json.loads((root/'validation/fixture-hashes.json').read_text()).items():
    data = (root/'tests'/name).read_bytes()
    if len(data) != item['bytes'] or hashlib.sha256(data).hexdigest() != item['sha256']:
        errors.append('Fixture changed: '+name)
readme = (root/'README.md').read_text(encoding='utf-8')
if '基于 [AtracDEnc]' not in readme or 'built on the [AtracDEnc]' not in readme:
    errors.append('Bilingual upstream attribution is missing')
if 'AtracDEnc-AAL '+version not in (root/'src/cli.cpp').read_text(encoding='utf-8'):
    errors.append('CLI version mismatch')
for path in files:
    relative = path.relative_to(root).as_posix()
    if path.suffix.lower() in ('.exe', '.dll', '.pdb', '.obj', '.o', '.a', '.elf'):
        errors.append('Compiled file in source: '+relative)
    if path.suffix.lower() not in ('.md', '.txt', '.json', '.yml', '.cpp', '.mm', '.in', '.c', '.h', '.hpp', '.inc', '.py', '.ps1', '.sh'):
        continue
    text = path.read_text(encoding='utf-8')
    if re.search(r'[A-Za-z]:[\\/]+Users[\\/]+', text):
        errors.append('Private user path: '+relative)
    if path.suffix != '.md': continue
    for destination in re.findall(r'\]\(([^\s)]+)(?:\s+"[^"]*")?\)', text):
        destination = destination.strip('<>').split('#', 1)[0]
        if not destination or re.match(r'[a-zA-Z][a-zA-Z0-9+.-]*:', destination): continue
        target = path.parent/destination.replace('%20', ' ')
        if not target.exists(): errors.append('Broken link: '+relative+' -> '+destination)
if errors:
    print('\n'.join(errors)); raise SystemExit(1)
print(f'PASS: {len(files)} public source files; bilingual attribution, versions, local links and fixture hashes valid')
