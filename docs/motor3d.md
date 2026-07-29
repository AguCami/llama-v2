# El motor 3D (`components/g3d`)

Un rasterizador por software chiquito, pensado para un ESP32-S3 con PSRAM y sin
GPU. Todo en C99 portable: no incluye un solo header del ESP-IDF, por eso el
simulador de escritorio corre el mismo código.

## Pipeline

1. **Transformación**: cada malla se multiplica por `modelo` (para el mundo, que
   se usa en la iluminación) y por `vista · proyección` (para el clip space).
2. **Descarte de caras traseras**: se calcula la normal de la cara con el
   producto cruz de sus aristas en espacio de mundo y se compara con la dirección
   a la cámara. Sirve doble: descarta la mitad de los triángulos y da el factor
   de realce de bordes.
3. **Recorte contra el plano cercano** (`z + w > 0`) con Sutherland–Hodgman. Sólo
   ese plano: los demás los resuelve el recorte del rectángulo en pantalla.
4. **División perspectiva** y paso a coordenadas de pantalla (con el eje Y dado
   vuelta).
5. **Rasterizado** con funciones de arista incrementales y prueba de profundidad.

## Z-buffer con 1/w

El buffer de profundidad guarda `1/w` en vez de `z`, y gana el valor **mayor**.
Dos ventajas:

- `1/w` es lineal en el espacio de pantalla, así que se interpola con una simple
  combinación baricéntrica, sin división por píxel.
- La precisión se concentra cerca de la cámara, que es justo donde está la llama.

El buffer se limpia a 0 (infinitamente lejos) en cada cuadro.

## Sombreado plano por cara

Cada triángulo tiene un color RGB565 propio. La intensidad se calcula una vez
por cara:

```
shade = ambiente + difusa · max(0, n · luz) + rim · (1 − n·ojo)²
```

Es sombreado plano a propósito: es lo que le da el aire "low-poly", y evita
interpolar color por píxel. El bucle interno del rasterizador es apenas tres
sumas, una comparación y dos escrituras.

## Convención de bobinado

Los triángulos se generan **antihorarios vistos desde afuera**. Como la
proyección da vuelta el eje Y, en pantalla quedan antihorarios con Y hacia abajo,
lo que da determinante negativo con la fórmula clásica. El rasterizador trabaja
con el signo invertido (`raster_tri` en `g3d_raster.c`); si alguna primitiva
nueva sale "hueca", casi seguro tiene el bobinado al revés.

## Primitivas de malla

No hay cargador de modelos: la llama y el escenario se arman con tres
primitivas, que además son fáciles de animar y ocupan nada en flash.

- `g3d_mesh_box` — caja centrada
- `g3d_mesh_frustum` — prisma con tapa de otro tamaño y desplazada (cuellos,
  patas, cuencos)
- `g3d_mesh_pyramid` — pirámide de base rectangular (orejas, montañas, cacas)

## Jerarquía y animación

`llama_model.c` define 14 partes, cada una con su propia malla construida
alrededor de su pivote:

```
cuerpo ├── lana
       ├── manta
       ├── cuello ── cabeza ── orejas (2) + ojos (2)
       ├── cola
       └── patas (4)
```

Por cuadro se calcula `mundo = mundo(padre) · pivote · animación`. Las
animaciones (`LA_IDLE`, `LA_WALK`, `LA_EAT`, `LA_SLEEP`, `LA_HAPPY`, `LA_SICK`,
`LA_SPIT`, `LA_SHEAR`, `LA_DEAD`) son funciones de seno y rampas sobre esos
ángulos: no hay keyframes ni interpolación de poses guardadas.

El parpadeo se hace escalando los ojos en Y, y la esquila reconstruye la malla
sin los mechones de lana.

## Capa 2D (`g2d.h`)

Se dibuja después del 3D, ignorando la profundidad: barras, botones, paneles con
transparencia (mezcla en RGB565) y texto con una fuente 5×7 propia que decodifica
UTF-8 para las vocales acentuadas, la ñ y los signos ¡ ¿.

Las partículas (corazones, Zzz, monedas, escupitajos) son posiciones 3D
proyectadas a pantalla con `g3d_project()` y dibujadas como iconos vectoriales,
escalados por distancia.

## Costo aproximado

Para el cuadro típico (450 triángulos, 68 000 píxeles):

| Etapa | Costo |
|---|---|
| Transformación de vértices | ~600 vértices × 2 matrices |
| Descarte + iluminación | 1 producto cruz y 1 normalización por triángulo |
| Rasterizado | 3 sumas + 1 test de profundidad por píxel |
| Volcado a la pantalla | 68 000 inversiones de bytes + 7 transferencias DMA |
