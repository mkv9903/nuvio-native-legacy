#!/bin/bash
# HOMEBREW CHANNEL (webOS) REPOSITORY, generated for each release.
#
# The Homebrew Channel accepts extra repositories: a URL returning JSON
# with `packages[]`, each with the app manifest (id, version, ipkUrl,
# sha256). Same format as official repository (repo.webosbrew.org/api/apps.json).
#
# SERVERLESS: the two files are ATTACHED to the release, and the URL users
# register in the channel is the GitHub "latest release" URL, which always redirects
# to the newest one:
#   https://github.com/mkv9903/nuvio-native-legacy/releases/latest/download/repo.json
#
# Usage: bash tools/hb-repo.sh <package.ipk> <output-folder>
# Generates <output>/webosbrew.manifest.json and <output>/repo.json. Publishing them
# is the job of `gh release create/upload` in the release recipe.
set -euo pipefail
cd "$(dirname "$0")/.."

IPK="${1:?usage: hb-repo.sh <package.ipk> <output>}"
SAIDA="${2:?usage: hb-repo.sh <package.ipk> <output>}"
REPO="mkv9903/nuvio-native-legacy"
mkdir -p "$SAIDA"

python3 - "$IPK" "$SAIDA" "$REPO" <<'EOF'
import hashlib, json, os, sys
ipk, saida, repo = sys.argv[1], sys.argv[2], sys.argv[3]
app = json.load(open("deploy/app/appinfo.json"))
versao = app["version"]
nome = os.path.basename(ipk)
# The attachment name must be that of the published file: ipkUrl is derived from it.
esperado = f"{app['id']}_{versao}_arm.ipk"
if nome != esperado:
    sys.exit(f"hb-repo.sh: expected {esperado}, got {nome}")
h = hashlib.sha256(open(ipk, "rb").read()).hexdigest()
base = f"https://github.com/{repo}/releases/download/v{versao}"
icone = f"https://raw.githubusercontent.com/{repo}/v{versao}/deploy/app/icon-large-20.png"
manifesto = {
    "id": app["id"],
    "version": versao,
    "type": "native",
    "title": app["title"],
    "appDescription": "Unofficial native Nuvio client for LG webOS",
    "iconUri": icone,
    "sourceUrl": f"https://github.com/{repo}",
    # The app runs without root (jail uid); only video depends on root
    # on some TVs, which the channel cannot know. See INSTALL.md.
    "rootRequired": False,
    "ipkUrl": f"{base}/{nome}",
    "ipkHash": {"sha256": h},
    "ipkSize": os.path.getsize(ipk),
}
indice = {
    "paging": {"page": 1, "count": 1, "maxPage": 1, "itemsTotal": 1,
               "prevUrl": None, "nextUrl": None},
    "packages": [{
        "id": app["id"],
        "title": manifesto["title"],
        "iconUri": icone,
        "manifestUrl": f"{base}/webosbrew.manifest.json",
        "manifest": manifesto,
        "pool": "main",
        "shortDescription": manifesto["appDescription"],
    }],
}
json.dump(manifesto, open(os.path.join(saida, "webosbrew.manifest.json"), "w"), indent=2)
json.dump(indice, open(os.path.join(saida, "repo.json"), "w"), indent=2)
print(f"hb-repo.sh: {versao} sha256={h[:12]}... -> {saida}/repo.json")
EOF
