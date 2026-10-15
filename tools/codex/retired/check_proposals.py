"""Validate open inbox CSV names and write only those proposals to ignored scratch CSV."""
import re
from pathlib import Path

ROOT = Path(__file__).resolve().parents[3]


def main():
    text = (ROOT/'notes/codex-inbox.md').read_text(encoding='utf-8')
    proposals = []
    addresses = set()
    for section in re.split(r'(?m)^## ',text)[1:]:
        if '[status: open]' not in section.splitlines()[0]:
            continue
        for block in re.findall(r'```csv\n(.*?)\n```',section,re.S):
            for line in block.splitlines():
                address,name,comment = line.split(',',2)
                if not re.fullmatch(r'(ovl_[0-9A-F]{3}::)?[0-9a-fA-F]{8}',address):
                    raise ValueError(f'invalid address {address}')
                if not re.fullmatch(r'[A-Za-z_][A-Za-z_0-9]*',name):
                    raise ValueError(f'invalid symbol {name}')
                if address.lower() in addresses:
                    raise ValueError(f'duplicate address {address}')
                addresses.add(address.lower())
                proposals.append(line)
    path = ROOT/'extract/codex/open-proposals.csv'
    path.write_text('\n'.join(proposals)+'\n',encoding='utf-8')
    print(f'{len(proposals)} syntactically valid, unique-address proposals -> {path}')


if __name__ == '__main__':
    main()
