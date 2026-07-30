# Referencias de arte (Higgsfield)

El diseño del personaje se definió primero con Higgsfield y después se modeló a
mano en `components/llamapet/src/llama_model.c`, porque una malla generada tiene
miles de triángulos y en la placa hay presupuesto para unos cientos.

## 1. Referencia de personaje (imagen)

- Modelo: `nano_banana_pro` · 1024 × 1024
- Job: `1ab86a13-4337-45ca-94c7-c33b55132c2c`
- Prompt: llama low-poly estilizada para un juego de mascota virtual, vista de
  tres cuartos, caras facetadas, lana crema y beige, pezuñas y hocico grises,
  ojos grandes, orejas curvas, manta andina roja/verde azulada/mostaza, fondo
  gris plano.

De ahí salieron la paleta y las proporciones que usa el modelo de la placa:

| Elemento | RGB | Dónde |
|---|---|---|
| Lana crema | 250, 232, 194 | `WOOL_CREAM` en `game.c` |
| Lana de anciana | 224, 214, 200 | `WOOL_GREY` |
| Hocico | 196, 170, 140 | `COL_MUZZLE` |
| Pezuñas | 72, 64, 60 | `COL_HOOF` |
| Manta roja | 204, 62, 56 | `COL_BLK_RED` |
| Manta mostaza | 232, 178, 60 | `COL_BLK_GOLD` |
| Manta verde azulada | 38, 150, 148 | `COL_BLK_TEAL` |

## 2. Malla 3D (GLB)

- Modelo: `image_to_3d` (a partir de la imagen anterior)
- Job: `1588385f-3e70-4bf3-95a9-b8c262b933b3`
- Salida: `.glb` sin texturas

Sirve para mirar el personaje desde cualquier ángulo, para renders de promoción o
para una versión web del juego. **No se usa en el firmware**: la llama que corre
en el ESP32-S3 son 14 partes hechas con cajas, prismas y pirámides, unos 230
triángulos en total, animadas por jerarquía de huesos.

Los archivos quedan en la cuenta de Higgsfield de quien los generó (el entorno de
este repositorio no tiene salida a la CDN para descargarlos y versionarlos acá).
Si querés incorporarlos, bajalos y guardalos en `docs/img/` o `assets/`.

## Regenerar el personaje

Para probar otra estética (llama punk, con gorro, de otro color), alcanza con
generar una referencia nueva y cambiar los colores de la tabla de arriba más las
proporciones en `props_for()` de `llama_model.c`. El simulador de escritorio
(`cd sim && make png`) muestra el resultado sin necesidad de grabar la placa.


## 3. Modelo texturizado y pipeline de sprites

El primer GLB salió **sin textura** (`should_texture` viene en false por
defecto): posiciones, normales y UV, pero cero materiales. Se regeneró con
textura:

- Modelo: `image_to_3d` con `should_texture: true` (30 créditos)
- Job: `e40bc89f-c05d-4858-8969-be369973981b`
- Resultado: 29.314 triángulos, textura JPEG de 2048×2048, 4 MB
- Guardado en `assets/ref/llama.glb`

### Qué hace la herramienta

`tools/mksprites.py` carga ese GLB con `tools/glb.py` (lector mínimo de glTF
binario) y lo rasteriza con `tools/render.py`, un renderizador offline con
z-buffer, UV con corrección de perspectiva y la **misma cámara y la misma luz
que el motor del juego** — misma distancia, misma altura, mismo campo de visión,
misma dirección de luz y mismo ambiente hemisférico. Por eso el sprite se
integra con el corral 3D en vez de parecer pegado encima.

Cada ángulo se renderiza a 240×284 con supermuestreo 3× (720×852) y se reduce
promediando ponderado por cobertura, de donde sale el alfa de los bordes. El
resultado se recorta a su caja útil y se guarda con el desplazamiento respecto
del punto de anclaje, que es el origen del modelo (entre las patas) proyectado
a pantalla. En la placa se proyecta la posición de la llama, se compara la
escala contra la de referencia y se copia el sprite escalado — así la cría, los
saltos y el paseo por el corral salen del mismo bitmap.

La orientación del modelo: **Y arriba, y a yaw 0 mira hacia +Z**, que es la
convención del juego. Ojo que la versión pelada venía con los ejes al revés.

### Números

| | |
|---|---|
| Ángulos | 16 (uno cada 22,5°) |
| Tamaño típico del recorte | 110 × 125 px |
| Hoja completa | 392 KB, mapeada desde flash |
| Generación | ~21 s en x86 para los 16 ángulos |

### Descartado: importar la malla

Se probó decimar el GLB para dibujarlo en 3D real en la placa. Soldando primero
los vértices duplicados (25.008 → 14.761) el decimador por cuádricas baja hasta
758 triángulos, pero el resultado pierde la forma: es una reconstrucción hecha
desde una foto, su detalle está en el ruido de la superficie y no tiene aristas
limpias que preservar. Quedó en `assets/ref/llama_lowpoly.bin` como comparación.
