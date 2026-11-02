"""Package AtracDEnc-AAL source or native assets using the Python standard library."""
import argparse, datetime, hashlib, json, pathlib, platform, stat, zipfile

root = pathlib.Path(__file__).resolve().parents[1]
parser = argparse.ArgumentParser()
parser.add_argument('--platform', required=True, choices=['windows-x64', 'macos-universal', 'macos-arm64', 'macos-x86_64', 'source'])
parser.add_argument('--bin-dir', type=pathlib.Path, default=root/'bin')
parser.add_argument('--output-dir', type=pathlib.Path, default=root/'dist')
args = parser.parse_args()
version = (root/'VERSION').read_text().strip()
name = f'AtracDEnc-AAL-v{version}-{args.platform}'
args.output_dir.mkdir(parents=True, exist_ok=True)
destination = args.output_dir/(name+'.zip')
files = {}
excluded = {'bin', 'build', 'dist', '.git', '__pycache__', '.venv'}
def add(file, relative=None):
    if not file.is_file(): raise FileNotFoundError(file)
    files[str(relative or file.relative_to(root)).replace('\\', '/')] = file.read_bytes()
if args.platform == 'source':
    for file in sorted(root.rglob('*')):
        if file.is_file() and not (set(file.relative_to(root).parts) & excluded) and args.output_dir.resolve() not in file.resolve().parents:
            add(file)
else:
    for filename in ('README.md', 'LICENSE', 'THIRD-PARTY-NOTICES.txt', 'UPSTREAM.md', 'CREDITS.md', 'VERSION'):
        add(root/filename)
    for directory in ('docs', 'licenses', 'validation'):
        for file in sorted((root/directory).rglob('*')):
            if file.is_file(): add(file)
    add(root/'include/independent_aal.h')
    add(root/'examples/encode_buffer.c')
    add(root/'examples/decode_buffer.c')
    add(root/'examples/stream_callbacks.c')
    if args.platform == 'windows-x64':
        add(args.bin_dir/'atracdenc-aal.exe', 'atracdenc-aal.exe')
        add(args.bin_dir/'AtracDEnc-AAL-GUI.exe', 'AtracDEnc-AAL-GUI.exe')
        add(args.bin_dir/'libatracdenc_aal-x86_64-windows-gnu.a', 'lib/libatracdenc_aal.a')
    elif args.platform == 'macos-universal':
        add(args.bin_dir/'atracdenc-aal-macos-universal', 'atracdenc-aal')
        for target in ('aarch64', 'x86_64'):
            add(args.bin_dir/f'libatracdenc_aal-{target}-macos.a', f'lib/{target}/libatracdenc_aal.a')
    else:
        add(args.bin_dir/'atracdenc-aal', 'atracdenc-aal')
        add(args.bin_dir/'libatracdenc_aal.a', 'lib/libatracdenc_aal.a')
    if args.platform.startswith('macos-'):
        app=args.bin_dir/'AtracDEnc-AAL-GUI.app'
        if not app.is_dir(): raise FileNotFoundError('Native macOS converter bundle is required: '+str(app))
        for file in sorted(app.rglob('*')):
            if file.is_file(): add(file, pathlib.Path(app.name)/file.relative_to(app))
    checks=root/'build/offline-checks.json'
    check_report=json.loads(checks.read_text(encoding='utf-8')) if checks.is_file() else None
    large_checks=root/'build/large-file-validation.json'
    large_report=json.loads(large_checks.read_text(encoding='utf-8')) if large_checks.is_file() else None
    if check_report and (not check_report['passed'] or check_report['version']!=version):
        raise ValueError('Offline checks failed or belong to a different version')
    files['BUILD-INFO.json'] = (json.dumps({
        'project': 'AtracDEnc-AAL', 'version': version, 'package_platform': args.platform,
        'packaging_host': platform.system()+'/'+platform.machine(),
        'created_utc': datetime.datetime.now(datetime.timezone.utc).isoformat(),
        'binary_sha256': {key: hashlib.sha256(value).hexdigest() for key, value in files.items()
                          if key.endswith(('.exe', '.a', '/Contents/MacOS/AtracDEnc-AAL-GUI')) or key == 'atracdenc-aal'},
        'offline_checks': check_report,
        'large_file_checks': large_report,
        'validation_scope': 'See validation/README.md. Packaging does not itself establish native execution or hardware compatibility.'},
        ensure_ascii=False, indent=2)+'\n').encode()
with zipfile.ZipFile(destination, 'w', compression=zipfile.ZIP_DEFLATED, compresslevel=9) as package:
    for relative, data in sorted(files.items()):
        item = zipfile.ZipInfo(name+'/'+relative, (2026, 11, 1, 0, 0, 0))
        item.create_system = 3
        executable = relative.endswith(('.sh', '.exe', '/Contents/MacOS/AtracDEnc-AAL-GUI')) or relative == 'atracdenc-aal'
        item.external_attr = (stat.S_IFREG | (0o755 if executable else 0o644)) << 16
        item.compress_type = zipfile.ZIP_DEFLATED
        package.writestr(item, data)
with zipfile.ZipFile(destination) as package:
    assert package.testzip() is None
sums = []
for file in sorted(args.output_dir.glob('AtracDEnc-AAL-v'+version+'-*.zip')):
    sums.append(hashlib.sha256(file.read_bytes()).hexdigest()+'  '+file.name+'\n')
(args.output_dir/'SHA256SUMS.txt').write_text(''.join(sums), encoding='utf-8', newline='\n')
print(json.dumps({'file': destination.name, 'files': len(files), 'bytes': destination.stat().st_size,
                  'sha256': hashlib.sha256(destination.read_bytes()).hexdigest()}))
