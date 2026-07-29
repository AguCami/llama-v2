/*
 * API publica del juego. El firmware solo tiene que crear el juego,
 * pasarle la entrada (tactil + IMU) y un framebuffer por cuadro.
 */
#ifndef LLAMA_GAME_H
#define LLAMA_GAME_H

#include "g3d.h"
#include "llama_pet.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    bool  touch_down;
    int   touch_x, touch_y;
    float ax, ay, az;      /* aceleracion en g */
    float gx, gy, gz;      /* giro en grados/s */
    int   battery_pct;     /* -1 si no se conoce */
    bool  charging;
} llama_input;

typedef struct llama_game llama_game;

llama_game *llama_game_create(int w, int h);
void        llama_game_destroy(llama_game *g);

/* Un cuadro completo: logica + render sobre `fb`. */
void llama_game_frame(llama_game *g, const llama_input *in, float dt, g3d_target *fb);

/* Guarda la partida ya mismo (por ejemplo antes de dormir el equipo). */
void llama_game_save(llama_game *g);

const llama_pet *llama_game_pet(const llama_game *g);

/* true si conviene mantener la pantalla encendida (hay animacion en curso). */
bool llama_game_busy(const llama_game *g);

#ifdef __cplusplus
}
#endif
#endif /* LLAMA_GAME_H */
