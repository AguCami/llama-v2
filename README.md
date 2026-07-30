# Mi Llama — mascota virtual 3D para ESP32-S3-Touch-LCD-1.83

Un Tamagotchi para la placa Waveshare **ESP32-S3-Touch-LCD-1.83** (SKU 32790),
sobre su pantalla táctil de 1.83" (240 × 284). Sin LVGL: hay un motor 3D propio
con z-buffer que dibuja el corral, el cerco y la cordillera, y la llama es el
modelo de 29.314 triángulos generado con Higgsfield, pre-renderizado desde 16
ángulos con su textura de 2048×2048. Lo que se ve en pantalla es ese modelo, no
una aproximación hecha a mano.

| Corral | Modelo 3D | Cría |
|---|---|---|
| ![pantalla principal](docs/img/principal.png) | ![vista 3/4](docs/img/llama_3d.png) | ![cría](docs/img/cria.png) |

| Minijuego | Noche | Comida |
|---|---|---|
| ![minijuego](docs/img/minijuego.png) | ![noche](docs/img/noche.png) | ![menú de comida](docs/img/comida.png) |

Todas las capturas salen del simulador de escritorio, que corre exactamente el
mismo código que la placa.

## Qué hace

- **La llama es el modelo original**: 16 ángulos pre-renderizados que la placa
  copia como sprites con alfa de 4 bits, respetando el z-buffer del corral.
  Arrastrando el dedo la cámara la orbita y va cambiando de ángulo.
- Se mueve por el corral, come, salta, se enferma y **escupe** si la querés
  sobrealimentar.
- **Necesidades reales**: hambre, ánimo, energía, higiene y salud bajan con el
  tiempo, incluso con el equipo apagado (se recupera con el RTC al encender).
- **Ciclo de vida**: cría → joven → adulta → anciana. Cambian el tamaño, las
  proporciones (la cría tiene la cabeza más grande) y el color de la lana.
- **Economía**: la lana crece sola; cuando llega al 60 % aparece el botón de
  esquila y te deja monedas para comprar grano, manzanas o remedios.
- **Minijuego "Salto de la llama"**: la llama corre y hay que tocar la pantalla
  para saltar las piedras. Los puntos dan monedas y suben el ánimo.
- **Sensores**: sacudir la placa hace saltar a la llama (o la despierta), la
  inclinación mueve suavemente la cámara y el arrastre con el dedo la orbita.
- **Día y noche** según el reloj RTC: cielo, estrellas, luna y luces cambian.
- **Estaciones** (hemisferio sur): el paisaje de fondo es un panorama de 360°
  generado con Higgsfield, uno por estación, y el piso es una textura cenital
  también de Higgsfield dibujada al estilo "Mode 7" (dos sumas y una lectura
  por píxel). Nieve en invierno, hojas en otoño, pétalos en primavera.
- **El corral sigue el diseño de Higgsfield** (`assets/ref/corral.png`):
  tablones anchos con talla escalonada, travesaños que sobrepasan la esquina y
  una faja tejida por lado.
- **Tiempo variable**: viento con ráfagas que empuja las partículas y lluvia
  que se larga sola de vez en cuando (según la estación), tapa el sol y
  agrisa la luz de toda la escena.
- **Ahorro de energía**: baja el brillo a los 25 s sin uso y entra en sueño
  profundo a los 3 min; se despierta al tocar la pantalla.

## Hardware

Placa: **Waveshare ESP32-S3-Touch-LCD-1.83** (ESP32-S3R8, 8 MB PSRAM, 16 MB
flash). El detalle completo de pines está en [`docs/hardware.md`](docs/hardware.md).

| Bloque | Chip | Conexión |
|---|---|---|
| Pantalla | ST7789, 240 × 284 IPS | SPI: DC 4, CS 5, SCK 6, MOSI 7, RST 38, BL 40 |
| Táctil | CST816T | I2C 0x15, RST 39, INT 13 |
| IMU | QMI8658 (6 ejes) | I2C 0x6B |
| RTC | PCF85063 | I2C 0x51 |
| Energía | AXP2101 | I2C 0x34 |

El bus I2C es compartido: **SDA 15, SCL 14**.

## Compilar y grabar

Requiere ESP-IDF 5.5 o superior.

```bash
idf.py set-target esp32s3
idf.py build
idf.py -p /dev/ttyACM0 flash monitor
```

La partición `factory` es de 4 MB y la partida se guarda en NVS, así que
sobrevive a reinicios y a regrabaciones del firmware (mientras no borres NVS).

## Probarlo en la computadora

El motor, el juego y la interfaz son C portable sin dependencias del ESP-IDF, así
que se puede correr en cualquier PC y sacar capturas:

```bash
cd sim
make            # compila build/llamasim
make png        # genera capturas PNG de todas las pantallas en build/shots
./build/llamasim build/shots minigame   # una escena puntual
```

Escenas disponibles: `boot`, `main`, `turn`, `food`, `stats`, `minigame`,
`sleep`, `sick`, `dead`, `cria`, `night`, `shake`, `estaciones`, `horas`,
`lluvia`, `grilla0`…`grilla3` (una fila de la grilla estación × hora).

## Cómo está armado

```
components/g3d/          motor 3D + dibujo 2D (portable, sin ESP-IDF)
  include/g3d.h            matemática, mallas, rasterizador con z-buffer,
                           sombreado suave y niebla de distancia
  include/g2d.h            primitivas 2D, fuente 5x7 con acentos y ñ
components/llamapet/     el juego (portable)
  src/pet.c                simulación de necesidades y ciclo de vida
  src/llama_model.c        geometría low-poly y animaciones de la llama
  src/llama_scene.c        corral, cerco, cordillera y objetos
  src/game.c               bucle, cámara, gestos, partículas
  src/ui.c                 HUD, botones, menús
  src/minigame.c           minijuego de saltos
main/                    firmware ESP32-S3 (drivers + bucle principal)
sim/                     simulador de escritorio
docs/                    documentación y capturas
```

La única frontera con el hardware es
[`llama_platform.h`](components/llamapet/include/llama_platform.h): hora, guardado,
sonido y semilla aleatoria. El ESP32 la implementa con RTC + NVS; el simulador,
con memoria y un reloj falso.

Más detalle: [`docs/motor3d.md`](docs/motor3d.md) (cómo funciona el
rasterizador) y [`docs/mecanicas.md`](docs/mecanicas.md) (números del juego).

## Rendimiento y memoria

- Framebuffer RGB565: 240 × 284 × 2 = **133 KB** en PSRAM.
- Z-buffer (float, guarda 1/w): **266 KB** en PSRAM.
- Buffers DMA de volcado: 2 × 19 KB en RAM interna.
- Hoja de sprites: **392 KB** embutidos en el binario y **mapeados desde flash**,
  o sea que no gastan un byte de RAM.
- Geometría 3D por cuadro: **~620 triángulos** (solo el escenario).
- Hoja de sprites: **4 poses × 16 ángulos = 1,9 MB**. Las poses (parada,
  agachada, pastando, echada) salen de deformar la geometría del GLB antes de
  renderizar — doblar el cuello sobre el pixel ya renderizado hunde la cabeza
  dentro del cuerpo en vez de bajarla.
- Panorama + baldosas del piso: **2,3 MB** más embutidos en flash, cero RAM.
- Medición de referencia: **1.39 ms por cuadro** en x86-64 con `-O2` con el
  piso texturizado a pantalla y el corral nuevo (0.87 ms con el piso de malla).
  El número sobre la placa todavía no está medido (ver limitaciones).
- El bucle principal se limita a ~30 fps a propósito, para cuidar la batería.

### Por qué sprites y no la malla

El modelo tiene 29.314 triángulos: unas 30 veces el presupuesto de la placa, o
sea ~2 fps. Decimarlo tampoco sirve — es una reconstrucción hecha desde una
imagen, sin aristas limpias que preservar, y al bajarlo a 758 triángulos pierde
la forma (está en `assets/ref/llama_lowpoly.bin` si querés comparar). Renderizarlo
**fuera** de la placa no tiene límite de tiempo, así que ahí se usan los 29.314
triángulos completos con su textura, y el ESP32 solo copia el resultado.

Para regenerar la hoja después de cambiar el modelo o la cámara:

```bash
python3 tools/mksprites.py --angles 16 --ss 3
```

## Limitaciones conocidas

- **Sonido sin implementar.** La placa lleva ES8311 + ES7210, pero los pines de
  audio sólo están publicados dentro del BSP oficial de Waveshare. `llama_plat_tone()`
  queda como enganche listo en `main/platform_esp32.c` en lugar de adivinar el
  cableado; el juego ya llama a esa función en todos los eventos.
- El driver del **AXP2101** es mínimo (porcentaje de batería y estado de carga).
- **Una sola pose.** Por ahora la hoja tiene la pose de pie en 16 ángulos, así
  que la llama no camina ni se echa a dormir: se mueve por el corral, rebota y
  escala, pero durmiendo se la ve parada. Las poses se generan igual que los
  ángulos (posando la malla offline, donde la calidad no cuesta nada); es el
  siguiente paso.
- Las cuatro etapas de vida se diferencian por escala, no por proporciones.
- **El firmware no está probado sobre la placa física** (no tengo una acá), así
  que los fps reales y el ajuste fino de la pantalla (inversión de color,
  orientación del táctil) hay que confirmarlos al grabarla. Lo que sí está
  verificado es el juego completo: 40 000 cuadros con entrada aleatoria bajo
  AddressSanitizer y UndefinedBehaviorSanitizer, sin fugas ni errores, más una
  revisión visual de todas las pantallas. Los pines salen del repositorio
  oficial de la placa, no de suposiciones.

## Créditos de arte

La llama se generó con Higgsfield: primero la imagen de referencia y después el
modelo 3D texturizado a partir de ella. El detalle del proceso, los identificadores
de los trabajos y el pipeline de sprites están en [`docs/assets.md`](docs/assets.md).

| Archivo | Qué es |
|---|---|
| `assets/ref/llama.glb` | modelo original texturizado (29.314 triángulos, textura 2048²) |
| `assets/llama_sprites.bin` | los 16 ángulos ya renderizados, lo que va a la placa |
| `assets/ref/llama_lowpoly.bin` | la malla decimada a 758 triángulos, solo como comparación |
| `assets/ref/pano/*.png` | los cuatro paisajes de estación (→ `assets/pano.bin` con `tools/mkpano.py`) |
| `assets/ref/ground/*.png` | las cuatro texturas del piso (→ `assets/ground.bin` con `tools/mkground.py`) |
| `assets/ref/corral.png` | el diseño del cerco que copia `build_fence()` |

El modelo procedural de superficies de revolución sigue en el repositorio
(`components/llamapet/src/llama_model.c`) y se usa solo/como respaldo si falta la
hoja de sprites, además de proveer el esqueleto que ubica las partículas.
