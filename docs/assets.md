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
