/*
 * Simulacion de la llama: necesidades, salud, evolucion y economia.
 */
#include "llama_pet.h"
#include <string.h>
#include <math.h>

/* ------------------------------------------------------------ aleatorio */

static uint32_t s_rng = 0x1BADB002u;

void llama_rand_seed(uint32_t s) { s_rng = s ? s : 0x1BADB002u; }

uint32_t llama_rand(void)
{
    /* xorshift32 */
    s_rng ^= s_rng << 13;
    s_rng ^= s_rng >> 17;
    s_rng ^= s_rng << 5;
    return s_rng;
}

float llama_randf(void) { return (float)(llama_rand() & 0xFFFFFF) / 16777216.f; }

int llama_rand_range(int a, int b)
{
    if (b <= a) return a;
    return a + (int)(llama_rand() % (uint32_t)(b - a + 1));
}

/* -------------------------------------------------------------- ayudas */

static float clampf(float v, float lo, float hi)
{
    return v < lo ? lo : (v > hi ? hi : v);
}

/* Ritmos por hora de juego, ajustados por etapa de vida. */
typedef struct {
    float hunger_per_h;
    float happy_per_h;
    float energy_per_h;
    float hygiene_per_h;
    float wool_per_h;
} llama_rates;

static llama_rates stage_rates(llama_stage s)
{
    switch (s) {
    case LLAMA_STAGE_CRIA:    { llama_rates r = { 11.f, 7.0f, 8.0f, 5.0f, 1.2f }; return r; }
    case LLAMA_STAGE_JOVEN:   { llama_rates r = {  9.f, 6.0f, 6.0f, 4.5f, 2.0f }; return r; }
    case LLAMA_STAGE_ADULTA:  { llama_rates r = {  7.f, 5.0f, 5.0f, 4.0f, 2.5f }; return r; }
    default:                  { llama_rates r = {  5.f, 4.5f, 6.5f, 3.5f, 1.5f }; return r; }
    }
}

llama_stage llama_pet_stage(const llama_pet *p)
{
    const uint32_t d = 86400u;
    if (p->age_s < 1 * d)  return LLAMA_STAGE_CRIA;
    if (p->age_s < 3 * d)  return LLAMA_STAGE_JOVEN;
    if (p->age_s < 12 * d) return LLAMA_STAGE_ADULTA;
    return LLAMA_STAGE_ANCIANA;
}

const char *llama_stage_name(llama_stage s)
{
    switch (s) {
    case LLAMA_STAGE_CRIA:    return "Cría";
    case LLAMA_STAGE_JOVEN:   return "Joven";
    case LLAMA_STAGE_ADULTA:  return "Adulta";
    case LLAMA_STAGE_ANCIANA: return "Anciana";
    default:                  return "?";
    }
}

const char *llama_food_name(llama_food f)
{
    switch (f) {
    case LLAMA_FOOD_PASTO:   return "Pasto";
    case LLAMA_FOOD_GRANO:   return "Grano";
    case LLAMA_FOOD_MANZANA: return "Manzana";
    default:                 return "?";
    }
}

int llama_food_price(llama_food f)
{
    switch (f) {
    case LLAMA_FOOD_PASTO:   return 0;
    case LLAMA_FOOD_GRANO:   return 1;
    case LLAMA_FOOD_MANZANA: return 3;
    default:                 return 0;
    }
}

/* ----------------------------------------------------------- ciclo vida */

void llama_pet_new(llama_pet *p, double now, const char *name)
{
    memset(p, 0, sizeof(*p));
    p->magic   = LLAMA_SAVE_MAGIC;
    p->version = LLAMA_SAVE_VERSION;
    strncpy(p->name, (name && name[0]) ? name : "Pelusa", sizeof(p->name) - 1);
    p->birth_time = now;
    p->last_seen  = now;
    p->hunger     = 70.f;
    p->happiness  = 80.f;
    p->energy     = 90.f;
    p->hygiene    = 100.f;
    p->health     = 100.f;
    p->wool       = 20.f;
    p->weight     = 12.5f;
    p->coins      = 5;
    p->poop_timer = 3600.f;
}

float llama_pet_mood(const llama_pet *p)
{
    float m = p->happiness * 0.40f + p->hunger * 0.22f +
              p->hygiene   * 0.14f + p->energy * 0.14f + p->health * 0.10f;
    return clampf(m, 0.f, 100.f);
}

bool llama_pet_needs_attention(const llama_pet *p)
{
    if (llama_pet_dead(p)) return false;
    return p->hunger < 25.f || p->happiness < 25.f || p->hygiene < 25.f ||
           p->health < 40.f || p->poops >= 3 || llama_pet_sick(p);
}

void llama_pet_update(llama_pet *p, float dt)
{
    if (dt <= 0.f) return;
    if (llama_pet_dead(p)) { p->last_seen += dt; return; }

    dt *= LLAMA_TIME_SCALE;
    const float h = dt / 3600.f;   /* fraccion de hora */
    p->age_s += (uint32_t)dt;

    const llama_rates r = stage_rates(llama_pet_stage(p));
    const bool sleeping = llama_pet_sleeping(p);
    const bool sick     = llama_pet_sick(p);

    /* --- necesidades --- */
    p->hunger -= r.hunger_per_h * h * (sleeping ? 0.35f : 1.f) * (sick ? 1.3f : 1.f);
    p->energy += sleeping ? (22.f * h) : (-r.energy_per_h * h);
    p->hygiene -= r.hygiene_per_h * h * (1.f + 0.6f * p->poops);
    p->wool += r.wool_per_h * h;

    float happy_drop = r.happy_per_h * h;
    if (p->poops >= 3)   happy_drop *= 1.6f;
    if (sick)            happy_drop *= 1.8f;
    if (p->hunger < 20.f) happy_drop *= 1.5f;
    p->happiness -= sleeping ? happy_drop * 0.25f : happy_drop;

    p->hunger    = clampf(p->hunger, 0.f, 100.f);
    p->energy    = clampf(p->energy, 0.f, 100.f);
    p->hygiene   = clampf(p->hygiene, 0.f, 100.f);
    p->happiness = clampf(p->happiness, 0.f, 100.f);
    p->wool      = clampf(p->wool, 0.f, 100.f);

    /* Se despierta sola cuando esta descansada. */
    if (sleeping && p->energy >= 99.f) p->flags &= ~LLAMA_FLAG_SLEEPING;
    /* Si esta agotada se duerme sola. */
    if (!sleeping && p->energy <= 2.f) {
        p->flags |= LLAMA_FLAG_SLEEPING;
        p->care_mistakes++;
    }

    /* --- caca --- */
    p->poop_timer -= dt;
    if (p->poop_timer <= 0.f) {
        if (p->poops < LLAMA_MAX_POOPS) p->poops++;
        p->poop_timer = (float)llama_rand_range(2400, 5400);
    }

    /* --- enfermedad --- */
    if (!sick) {
        float risk = 0.f;
        if (p->hygiene < 30.f) risk += (30.f - p->hygiene) * 0.02f;
        if (p->hunger  < 20.f) risk += (20.f - p->hunger)  * 0.02f;
        if (p->poops >= 4)     risk += 0.5f;
        if (p->health  < 50.f) risk += 0.3f;
        /* `risk` es la probabilidad esperada por hora. */
        if (risk > 0.f && llama_randf() < risk * h) {
            p->flags |= LLAMA_FLAG_SICK;
            p->sick_timer = 0.f;
            p->care_mistakes++;
        }
    } else {
        p->sick_timer += dt;
    }

    /* --- salud --- */
    float health_delta = 0.f;
    if (p->hunger < 12.f)  health_delta -= 5.f * h;
    if (p->hygiene < 12.f) health_delta -= 3.f * h;
    if (sick)              health_delta -= 4.f * h;
    if (p->happiness < 10.f) health_delta -= 2.f * h;
    if (!sick && p->hunger > 55.f && p->hygiene > 55.f && p->happiness > 45.f) {
        health_delta += 6.f * h;
    }
    /* La vejez pasa factura. */
    if (llama_pet_stage(p) == LLAMA_STAGE_ANCIANA) health_delta -= 0.6f * h;

    p->health = clampf(p->health + health_delta, 0.f, 100.f);
    if (p->health <= 0.f) {
        p->flags |= LLAMA_FLAG_DEAD;
        p->flags &= ~LLAMA_FLAG_SLEEPING;
    }

    /* --- peso --- */
    if (p->hunger < 25.f) p->weight -= 0.02f * h;
    p->weight = clampf(p->weight, 6.f, 60.f);
}

void llama_pet_catch_up(llama_pet *p, double now)
{
    double elapsed = now - p->last_seen;
    if (elapsed < 1.0) {
        p->last_seen = now;
        return;
    }
    const double max_gap = 7.0 * 86400.0;
    if (elapsed > max_gap) elapsed = max_gap;

    /* En pasos de 5 minutos para que los eventos (caca, enfermedad) ocurran. */
    while (elapsed > 0.0) {
        float step = (float)(elapsed > 300.0 ? 300.0 : elapsed);
        llama_pet_update(p, step);
        elapsed -= step;
    }
    p->last_seen = now;
}

/* -------------------------------------------------------------- acciones */

llama_result llama_pet_feed(llama_pet *p, llama_food food)
{
    if (llama_pet_dead(p)) return LLAMA_ACT_MUERTA;
    if (llama_pet_sleeping(p)) return LLAMA_ACT_DURMIENDO;
    if (p->hunger > 92.f) {
        /* Llama llena = llama que escupe. */
        p->happiness = clampf(p->happiness - 4.f, 0.f, 100.f);
        return LLAMA_ACT_LLENA;
    }
    int price = llama_food_price(food);
    if (p->coins < price) return LLAMA_ACT_SIN_MONEDAS;
    p->coins = (uint16_t)(p->coins - price);

    switch (food) {
    case LLAMA_FOOD_PASTO:
        p->hunger += 18.f; p->happiness += 2.f; p->weight += 0.10f; break;
    case LLAMA_FOOD_GRANO:
        p->hunger += 34.f; p->happiness += 4.f; p->weight += 0.30f; break;
    default:
        p->hunger += 14.f; p->happiness += 12.f; p->weight += 0.08f; break;
    }
    p->hunger    = clampf(p->hunger, 0.f, 100.f);
    p->happiness = clampf(p->happiness, 0.f, 100.f);
    p->times_fed++;
    if (p->poop_timer > 1800.f) p->poop_timer = 1800.f;
    return LLAMA_ACT_OK;
}

llama_result llama_pet_play(llama_pet *p, int score)
{
    if (llama_pet_dead(p)) return LLAMA_ACT_MUERTA;
    if (llama_pet_sleeping(p)) return LLAMA_ACT_DURMIENDO;
    if (p->energy < 12.f) return LLAMA_ACT_CANSADA;

    p->happiness = clampf(p->happiness + 10.f + score * 0.4f, 0.f, 100.f);
    p->energy    = clampf(p->energy - 9.f, 0.f, 100.f);
    p->hunger    = clampf(p->hunger - 4.f, 0.f, 100.f);
    p->weight    = clampf(p->weight - 0.08f, 6.f, 60.f);
    p->times_played++;
    if (score > 0) {
        p->coins = (uint16_t)(p->coins + (score / 3 > 0 ? score / 3 : 1));
        if (score > p->best_score) p->best_score = (uint16_t)score;
    }
    return LLAMA_ACT_OK;
}

llama_result llama_pet_clean(llama_pet *p)
{
    if (llama_pet_dead(p)) return LLAMA_ACT_MUERTA;
    if (p->poops == 0 && p->hygiene > 95.f) return LLAMA_ACT_NO_HACE_FALTA;
    p->poops   = 0;
    p->hygiene = 100.f;
    p->happiness = clampf(p->happiness + 3.f, 0.f, 100.f);
    p->times_cleaned++;
    return LLAMA_ACT_OK;
}

llama_result llama_pet_sleep_toggle(llama_pet *p)
{
    if (llama_pet_dead(p)) return LLAMA_ACT_MUERTA;
    if (llama_pet_sleeping(p)) {
        p->flags &= ~LLAMA_FLAG_SLEEPING;
        /* Despertarla antes de tiempo la pone de mal humor. */
        if (p->energy < 60.f) p->happiness = clampf(p->happiness - 5.f, 0.f, 100.f);
    } else {
        p->flags |= LLAMA_FLAG_SLEEPING;
    }
    return LLAMA_ACT_OK;
}

llama_result llama_pet_medicine(llama_pet *p)
{
    if (llama_pet_dead(p)) return LLAMA_ACT_MUERTA;
    if (!llama_pet_sick(p) && p->health > 90.f) return LLAMA_ACT_NO_HACE_FALTA;
    if (p->coins < 2) return LLAMA_ACT_SIN_MONEDAS;
    p->coins = (uint16_t)(p->coins - 2);
    p->flags &= ~LLAMA_FLAG_SICK;
    p->health    = clampf(p->health + 35.f, 0.f, 100.f);
    p->happiness = clampf(p->happiness - 2.f, 0.f, 100.f);
    return LLAMA_ACT_OK;
}

llama_result llama_pet_shear(llama_pet *p)
{
    if (llama_pet_dead(p)) return LLAMA_ACT_MUERTA;
    if (llama_pet_sleeping(p)) return LLAMA_ACT_DURMIENDO;
    if (p->wool < 60.f) return LLAMA_ACT_NO_HACE_FALTA;
    int earned = 4 + (int)(p->wool / 10.f);
    p->coins = (uint16_t)(p->coins + earned);
    p->wool  = 0.f;
    p->happiness = clampf(p->happiness - 6.f, 0.f, 100.f);
    p->times_sheared++;
    return LLAMA_ACT_OK;
}

llama_result llama_pet_pet(llama_pet *p)
{
    if (llama_pet_dead(p)) return LLAMA_ACT_MUERTA;
    if (llama_pet_sleeping(p)) return LLAMA_ACT_DURMIENDO;
    p->happiness = clampf(p->happiness + 1.5f, 0.f, 100.f);
    return LLAMA_ACT_OK;
}
