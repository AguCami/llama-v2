# Mecánicas del juego

Todo lo que sigue vive en `components/llamapet/src/pet.c` y es tiempo real: una
hora de reloj es una hora de juego. Para probar la evolución sin esperar días se
puede compilar con `-DLLAMA_TIME_SCALE=60` (una hora de juego por minuto).

## Estadísticas

| Estadística | Significado | 0 significa |
|---|---|---|
| Hambre | 100 = repleta | famélica |
| Ánimo | ganas de vivir | deprimida |
| Energía | descanso | se duerme sola |
| Higiene | limpieza del corral | enfermedad casi segura |
| Salud | resultado de todo lo anterior | se muere |

El HUD muestra las cuatro primeras; la salud aparece en la ficha y como aviso
rojo cuando la llama se enferma.

## Caída por hora (según etapa)

| Etapa | Hambre | Ánimo | Energía | Higiene | Lana |
|---|---|---|---|---|---|
| Cría | −11 | −7.0 | −8.0 | −5.0 | +1.2 |
| Joven | −9 | −6.0 | −6.0 | −4.5 | +2.0 |
| Adulta | −7 | −5.0 | −5.0 | −4.0 | +2.5 |
| Anciana | −5 | −4.5 | −6.5 | −3.5 | +1.5 |

Modificadores:

- Durmiendo: el hambre baja al 35 %, el ánimo al 25 % y la energía sube +22/h.
- Cada caca acelera la caída de higiene un 60 %.
- Con 3 o más cacas el ánimo cae 1.6 ×; enferma, 1.8 ×.
- La salud sube +6/h si hambre > 55, higiene > 55 y ánimo > 45; baja con hambre
  < 12, higiene < 12, enfermedad o vejez.

## Ciclo de vida

| Etapa | Edad |
|---|---|
| Cría | 0 – 1 día |
| Joven | 1 – 3 días |
| Adulta | 3 – 12 días |
| Anciana | más de 12 días |

Cambian el tamaño, el largo del cuello, la proporción de la cabeza y, en la
vejez, el color de la lana. Si la salud llega a 0 la llama muere: aparece la
lápida y se puede criar una nueva (se conservan los récords en el resumen).

## Acciones

| Acción | Efecto | Costo |
|---|---|---|
| Pasto | +18 hambre, +2 ánimo | gratis |
| Grano | +34 hambre, +4 ánimo | 1 moneda |
| Manzana | +14 hambre, +12 ánimo | 3 monedas |
| Jugar | +10 ánimo (+0.4 por punto), −9 energía, −4 hambre | energía ≥ 12 |
| Aseo | limpia todas las cacas, higiene a 100 | gratis |
| Dormir | alterna el sueño; despertarla cansada le baja el ánimo | — |
| Remedio | cura la enfermedad, +35 salud | 2 monedas |
| Esquilar | +4 y +1 por cada 10 % de lana; −6 ánimo | lana ≥ 60 % |
| Caricia | +1.5 ánimo (tocando a la llama) | gratis |

Si le das de comer con el hambre por encima de 92 **te escupe** y pierde ánimo.

## Cacas y enfermedad

- Aparece una caca cada 40–90 minutos (más rápido después de comer), hasta 5.
- La probabilidad de enfermarse por hora crece con higiene baja, hambre baja,
  cantidad de cacas y salud baja.
- Enferma: se mueve lento, cabizbaja, pierde salud y ánimo más rápido.

## Monedas

Se ganan esquilando y jugando (un tercio de los puntos, mínimo 1). Se gastan en
grano, manzanas y remedios. Se arranca con 5.

## Minijuego "Salto de la llama"

Vista lateral, la llama corre y hay que tocar la pantalla para saltar las
piedras. Tres vidas, 45 segundos, la velocidad sube de a poco. Cada piedra
esquivada es un punto; el récord queda guardado en la ficha.

## Tiempo apagado

Al encender, `llama_pet_catch_up()` avanza la simulación en pasos de 5 minutos
por todo el tiempo transcurrido según el RTC, con un tope de 7 días. Así que sí:
si la dejás una semana sin dar bola, te la vas a encontrar mal.
