#!/bin/bash
# Builds and runs on Mac, with the same art folder as the TV package.
#
# PERSISTS LOGIN ACROSS RUNS. Session lives in $NUVIO_DADOS, which here points
# to ~/.nuvio: log in ONCE (by scanning the screen QR) and every subsequent
# launch opens already logged into the account, as the refresh token renews itself.
# VERIFIED: 1st launch "[sessao] logado como ..."; restarting without scanning
# anything, "[sessao] sessao restaurada ..." and "[addons] N vindos da conta".
#
# To test as a NEW user (first launch after install),
# point the variable to an empty directory:
#     NUVIO_DADOS=/tmp/nuvio-novo bash tools/mac.sh
#
# "Sign out of account", in Settings, deletes the entire ~/.nuvio (session, settings, and
# progress) — after which scanning is needed again.
set -e
cd "$(dirname "$0")/.."
# Server -D flags come from the SAME local.properties as the web app (tools/env.sh).
# Without them the app compiles, installs and opens — and the only symptom is the login screen
# stating "This package was built without a server". None of these values enter
# version-controlled code files.
ENV_D=$(tools/env.sh)
# Outside package folder: Mac must not save session inside deploy/.
export NUVIO_DADOS="${NUVIO_DADOS:-$HOME/.nuvio}"
eval cc src/*.c src/dts/*.c -o /tmp/nuvio-native-legacy-mac -O1 -g "$ENV_D" \
  -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 \
  -L/opt/homebrew/lib -lSDL2 -lSDL2_image -lSDL2_ttf -lz \
  -framework OpenGL -Wno-deprecated-declarations
exec /tmp/nuvio-native-legacy-mac "$(pwd)/deploy/app/art" "$@"
