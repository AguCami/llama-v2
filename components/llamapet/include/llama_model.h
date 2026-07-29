/*
 * Modelo 3D low-poly de la llama: jerarquia de partes + animaciones.
 * Cada parte se construye alrededor de su propio pivote; `bind` la ubica
 * respecto del padre y `local` es la animacion.
 */
#ifndef LLAMA_MODEL_H
#define LLAMA_MODEL_H

#include "g3d.h"
#include "llama_pet.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    LP_BODY = 0,
    LP_WOOL,     /* mechones de lana (se ocultan al esquilar) */
    LP_BLANKET,  /* manta andina */
    LP_NECK,
    LP_HEAD,
    LP_EAR_L,
    LP_EAR_R,
    LP_EYE_L,
    LP_EYE_R,
    LP_TAIL,
    LP_LEG_FL,
    LP_LEG_FR,
    LP_LEG_BL,
    LP_LEG_BR,
    LP_COUNT
} llama_part_id;

typedef enum {
    LA_IDLE = 0,
    LA_WALK,
    LA_EAT,
    LA_SLEEP,
    LA_HAPPY,
    LA_SICK,
    LA_SPIT,
    LA_SHEAR,
    LA_DEAD,
    LA_COUNT
} llama_anim;

typedef struct {
    g3d_mesh mesh;
    int      parent;   /* indice de la parte padre, -1 = raiz */
    g3d_mat4 bind;     /* pivote respecto del padre */
    g3d_mat4 local;    /* animacion sobre el pivote */
    g3d_mat4 world;    /* calculada en llama_model_update */
    bool     visible;
} llama_part;

typedef struct {
    llama_part parts[LP_COUNT];

    /* Configuracion visual actual (se reconstruye la malla si cambia). */
    llama_stage stage;
    bool        sheared;
    bool        blanket;
    g3d_color   wool_color;

    /* Estado en el mundo. */
    g3d_v3 pos;
    float  heading;      /* radianes, 0 = mirando a +Z */
    float  scale;

    /* Animacion. */
    llama_anim anim;
    float      t;            /* tiempo global */
    float      anim_t;       /* tiempo dentro de la animacion */
    float      blink_timer;
    float      blink_close;  /* 0 = ojo abierto, 1 = cerrado */
    float      ear_twitch;
    float      body_y;       /* altura del cuerpo tras la animacion */
    float      walk_speed;
} llama_model;

bool llama_model_init(llama_model *m);
void llama_model_free(llama_model *m);

/* Reconstruye la geometria si cambio la etapa, la lana o la manta. */
void llama_model_configure(llama_model *m, llama_stage stage, bool sheared,
                           bool blanket, g3d_color wool);

void   llama_model_set_anim(llama_model *m, llama_anim a);
void   llama_model_update(llama_model *m, float dt);
void   llama_model_draw(g3d_target *t, const g3d_ctx *ctx, const llama_model *m, float tint);

/* Posiciones utiles para particulas y para saber donde toco el dedo. */
g3d_v3 llama_model_head_pos(const llama_model *m);
g3d_v3 llama_model_mouth_pos(const llama_model *m);
g3d_v3 llama_model_body_pos(const llama_model *m);

#ifdef __cplusplus
}
#endif
#endif /* LLAMA_MODEL_H */
