"""Check local documentation targets and active CMake source membership."""
from pathlib import Path
import re
import sys
from urllib.parse import unquote

ROOT = Path(__file__).resolve().parents[1]


def main():
    errors = []
    documents = sorted(ROOT.glob('*.md')) + sorted((ROOT / 'docs').rglob('*.md'))
    links = 0
    for document in documents:
        text = document.read_text(encoding='utf-8-sig')
        for match in re.finditer(r'\]\(([^\s)]+)\)', text):
            target = match.group(1).strip('<>')
            if re.match(r'^[a-zA-Z][a-zA-Z0-9+.-]*:', target):
                continue
            path = unquote(target.split('#', 1)[0])
            if not path:
                continue
            links += 1
            if not (document.parent / path).exists():
                line = text.count('\n', 0, match.start()) + 1
                errors.append(f'{document.relative_to(ROOT)}:{line}: missing {path}')

    cmake = (ROOT / 'CMakeLists.txt').read_text(encoding='utf-8')
    sources = set(re.findall(r'\b(?:Source|tests)/[\w./-]+\.cpp\b', cmake))
    for source in sorted(sources):
        if not (ROOT / source).is_file():
            errors.append(f'CMakeLists.txt: missing {source}')
    for source in sorted((ROOT / 'Source').rglob('*.cpp')):
        relative = source.relative_to(ROOT).as_posix()
        if relative not in sources:
            errors.append(f'Active source is not registered in CMakeLists.txt: {relative}')

    for error in errors:
        print(error)
    print(f'{len(documents)} documents; {links} local links; {len(sources)} CMake sources; {len(errors)} errors')
    return bool(errors)


if __name__ == '__main__':
    sys.exit(main())
