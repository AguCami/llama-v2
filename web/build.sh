#!/bin/sh
# Compila el juego a WebAssembly y arma la pagina con los assets adentro.
# Necesita wasi-sdk (https://github.com/WebAssembly/wasi-sdk): WASI_SDK=/ruta ./build.sh
set -e
cd "$(dirname "$0")/.."
: "${WASI_SDK:?falta WASI_SDK=/ruta/a/wasi-sdk}"
"$WASI_SDK/bin/clang" --target=wasm32-wasi -mexec-model=reactor -O2 -std=c11 \
    -Wall -Wextra -Wno-unused-parameter \
    -Icomponents/g3d/include -Icomponents/llamapet/include -Icomponents/llamapet/src \
    components/g3d/src/*.c components/llamapet/src/*.c web/wasm_main.c \
    -o web/llama.wasm -Wl,--export=malloc -Wl,--initial-memory=8388608
python3 - <<'PY'
import base64
def b64(p): return base64.b64encode(open(p,'rb').read()).decode()
html = open('web/sim_template.html').read()
for k, p in [('WASM','web/llama.wasm'), ('SPRITES','assets/llama_sprites.bin'),
             ('PANO','assets/pano.bin'), ('GROUND','assets/ground.bin')]:
    html = html.replace('@@%s@@' % k, b64(p))
open('web/ramona_sim.html', 'w').write(html)
print('web/ramona_sim.html listo')
PY
