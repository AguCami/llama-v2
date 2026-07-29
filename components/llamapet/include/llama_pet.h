/*
 * Simulacion de la mascota: estadisticas, necesidades, ciclo de vida.
 * Codigo portable, sin dependencias de hardware ni de graficos.
 */
#ifndef LLAMA_PET_H
#define LLAMA_PET_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#define LLAMA_SAVE_MAGIC   0x4C4C4D41u /* "LLMA" */
#define LLAMA_SAVE_VERSION 2u

/* Escala de tiempo del juego. 1.0 = tiempo real. Subirla acelera todo
 * (util para probar la evolucion sin esperar dias). */
#ifndef LLAMA_TIME_SCALE
#define LLAMA_TIME_SCALE 1.0f
#endif

typedef enum {
    LLAMA_STAGE_CRIA = 0,   /* recien nacida */
    LLAMA_STAGE_JOVEN,
    LLAMA_STAGE_ADULTA,
    LLAMA_STAGE_ANCIANA,
    LLAMA_STAGE_COUNT
} llama_stage;

typedef enum {
    LLAMA_FOOD_PASTO = 0,   /* gratis, poco alimento */
    LLAMA_FOOD_GRANO,       /* 1 moneda, mucho alimento */
    LLAMA_FOOD_MANZANA,     /* 3 monedas, poco alimento pero mucha alegria */
    LLAMA_FOOD_COUNT
} llama_food;

/* Resultado de una accion, para que la interfaz muestre el mensaje correcto. */
typedef enum {
    LLAMA_ACT_OK = 0,
    LLAMA_ACT_LLENA,        /* no tiene hambre: escupe */
    LLAMA_ACT_SIN_MONEDAS,
    LLAMA_ACT_CANSADA,
    LLAMA_ACT_DURMIENDO,
    LLAMA_ACT_NO_HACE_FALTA,
    LLAMA_ACT_MUERTA
} llama_result;

typedef struct {
    uint32_t magic;
    uint32_t version;
    char     name[16];

    double   birth_time;    /* unix */
    double   last_seen;     /* unix del ultimo guardado */

    float    hunger;        /* 0 = famelica, 100 = repleta */
    float    happiness;     /* 0..100 */
    float    energy;        /* 0..100 */
    float    hygiene;       /* 0..100 */
    float    health;        /* 0..100 */
    float    wool;          /* 0..100, crece sola */
    float    weight;        /* kg */

    uint32_t age_s;         /* segundos de vida acumulados */
    uint16_t coins;
    uint8_t  poops;         /* 0..LLAMA_MAX_POOPS */
    uint8_t  flags;

    uint16_t care_mistakes;
    uint16_t times_fed;
    uint16_t times_played;
    uint16_t times_cleaned;
    uint16_t times_sheared;
    uint16_t best_score;

    /* Temporizadores internos (segundos). */
    float    poop_timer;
    float    sick_timer;
    float    mood_timer;
    uint32_t reserved[4];
} llama_pet;

#define LLAMA_FLAG_SLEEPING (1u << 0)
#define LLAMA_FLAG_SICK     (1u << 1)
#define LLAMA_FLAG_DEAD     (1u << 2)

#define LLAMA_MAX_POOPS 5

void llama_pet_new(llama_pet *p, double now, const char *name);

/* Avanza la simulacion `dt` segundos de tiempo real. */
void llama_pet_update(llama_pet *p, float dt);

/* Recupera el tiempo transcurrido con el aparato apagado (tope: 7 dias). */
void llama_pet_catch_up(llama_pet *p, double now);

llama_result llama_pet_feed(llama_pet *p, llama_food food);
llama_result llama_pet_play(llama_pet *p, int score);
llama_result llama_pet_clean(llama_pet *p);
llama_result llama_pet_sleep_toggle(llama_pet *p);
llama_result llama_pet_medicine(llama_pet *p);
llama_result llama_pet_shear(llama_pet *p);
llama_result llama_pet_pet(llama_pet *p);   /* caricia */

llama_stage llama_pet_stage(const llama_pet *p);
const char *llama_stage_name(llama_stage s);
const char *llama_food_name(llama_food f);
int         llama_food_price(llama_food f);

/* 0..100: promedio ponderado del bienestar, para la carita del HUD. */
float       llama_pet_mood(const llama_pet *p);
bool        llama_pet_needs_attention(const llama_pet *p);

static inline bool llama_pet_sleeping(const llama_pet *p) { return (p->flags & LLAMA_FLAG_SLEEPING) != 0; }
static inline bool llama_pet_sick(const llama_pet *p)     { return (p->flags & LLAMA_FLAG_SICK) != 0; }
static inline bool llama_pet_dead(const llama_pet *p)     { return (p->flags & LLAMA_FLAG_DEAD) != 0; }

/* Generador pseudoaleatorio propio (reproducible, sin depender de libc). */
void     llama_rand_seed(uint32_t s);
uint32_t llama_rand(void);
float    llama_randf(void);           /* 0..1 */
int      llama_rand_range(int a, int b);

#ifdef __cplusplus
}
#endif
#endif /* LLAMA_PET_H */
