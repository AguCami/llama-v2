/*
 * Escenario: corral, cerco, montanas y objetos (comedero, caca, pelota...).
 */
#ifndef LLAMA_SCENE_H
#define LLAMA_SCENE_H

#include "g3d.h"
#include "llama_ambient.h"

#ifdef __cplusplus
extern "C" {
#endif

#define LLAMA_POOP_SPOTS 5

typedef struct {
    g3d_mesh ground;
    g3d_mesh fence;
    g3d_mesh mountains;
    g3d_mesh tufts;
    g3d_mesh clouds;
    g3d_mesh bowl;
    g3d_mesh hay;
    g3d_mesh apple;
    g3d_mesh ball;
    g3d_mesh poop;
    g3d_mesh grave;
    g3d_mesh rock;      /* obstaculo del minijuego */
    llama_season season; /* la estacion con la que se armo el piso */
    bool     backdrop_3d; /* false cuando el fondo lo pone el panorama */
    bool     ok;
} llama_scene;

bool llama_scene_init(llama_scene *s);

/* Rearma el piso y las matas con la paleta de la estacion. Es barato: unos
 * cientos de triangulos, y solo se hace cuando cambia el mes. */
void llama_scene_set_season(llama_scene *s, llama_season season);
void llama_scene_free(llama_scene *s);

/* `night` mezcla la paleta hacia tonos nocturnos. */
void llama_scene_draw_world(g3d_target *t, const g3d_ctx *ctx, const llama_scene *s, float night);
void llama_scene_draw_poops(g3d_target *t, const g3d_ctx *ctx, const llama_scene *s,
                            int count, float night);
void llama_scene_draw_mesh_at(g3d_target *t, const g3d_ctx *ctx, const g3d_mesh *m,
                              g3d_v3 pos, float yaw, float scale, float tint);

/* Sombra elipsoidal proyectada bajo un objeto. */
void llama_scene_shadow(g3d_target *t, const g3d_ctx *ctx, g3d_v3 pos, float radius,
                        uint8_t alpha);

g3d_v3 llama_scene_poop_pos(int index);

#ifdef __cplusplus
}
#endif
#endif /* LLAMA_SCENE_H */
