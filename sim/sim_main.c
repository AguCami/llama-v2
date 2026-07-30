/*
 * Simulador de escritorio: corre exactamente el mismo juego que el ESP32-S3
 * pero renderiza a archivos PPM, para poder revisar el resultado sin la placa.
 *
 * Uso:  llamasim <carpeta_salida> [escenario]
 * Escenarios: main, boot, food, stats, minigame, sleep, sick, dead, cria, night
 */
#include "game_internal.h"
#include "llama_game.h"
#include "llama_platform.h"
#include "llama_pet.h"
#include "g3d.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <sys/stat.h>

#define W 240
#define H 284

/* Base horaria del simulador: 16:00 UTC (de dia). */
static double g_clock = 1785340800.0;
static uint32_t g_millis = 0;
static llama_pet g_slot;
static bool g_slot_valid = false;

double   llama_plat_time(void)   { return g_clock; }
uint32_t llama_plat_millis(void) { return g_millis; }
void     llama_plat_tone(int f, int ms) { (void)f; (void)ms; }
void     llama_plat_haptic(int ms)      { (void)ms; }
uint32_t llama_plat_seed(void)   { return 0xC0FFEE11u; }

/* Hoja de sprites: en la PC se lee del archivo que genera tools/mksprites.py. */
static void *g_sprites;
static size_t g_sprites_len;

const void *llama_plat_sprites(size_t *len)
{
    if (!g_sprites) {
        const char *path = getenv("LLAMA_SPRITES");
        if (!path) path = "../assets/llama_sprites.bin";
        FILE *f = fopen(path, "rb");
        if (!f) {
            fprintf(stderr, "sin hoja de sprites (%s): uso el modelo procedural\n", path);
            return NULL;
        }
        fseek(f, 0, SEEK_END);
        long n = ftell(f);
        fseek(f, 0, SEEK_SET);
        g_sprites = malloc((size_t)n);
        if (g_sprites && fread(g_sprites, 1, (size_t)n, f) == (size_t)n) {
            g_sprites_len = (size_t)n;
        } else {
            free(g_sprites);
            g_sprites = NULL;
        }
        fclose(f);
    }
    if (len) *len = g_sprites_len;
    return g_sprites;
}

bool llama_plat_save(const void *blob, size_t len)
{
    if (len > sizeof(g_slot)) return false;
    memcpy(&g_slot, blob, len);
    g_slot_valid = true;
    return true;
}

bool llama_plat_load(void *blob, size_t len)
{
    if (!g_slot_valid || len > sizeof(g_slot)) return false;
    memcpy(blob, &g_slot, len);
    return true;
}

/* Tira panoramica: mismo criterio que la hoja de sprites. */
static void *g_pano;
static size_t g_pano_len;

const void *llama_plat_pano(size_t *len)
{
    if (!g_pano) {
        const char *path = getenv("LLAMA_PANO");
        if (!path) path = "../assets/pano.bin";
        FILE *f = fopen(path, "rb");
        if (!f) return NULL;
        fseek(f, 0, SEEK_END);
        long n = ftell(f);
        fseek(f, 0, SEEK_SET);
        g_pano = malloc((size_t)n);
        if (g_pano && fread(g_pano, 1, (size_t)n, f) == (size_t)n) {
            g_pano_len = (size_t)n;
        } else {
            free(g_pano);
            g_pano = NULL;
        }
        fclose(f);
    }
    if (len) *len = g_pano_len;
    return g_pano;
}

/* Baldosas del piso: mismo criterio. */
static void *g_ground;
static size_t g_ground_len;

const void *llama_plat_ground(size_t *len)
{
    if (!g_ground) {
        const char *path = getenv("LLAMA_GROUND");
        if (!path) path = "../assets/ground.bin";
        FILE *f = fopen(path, "rb");
        if (!f) return NULL;
        fseek(f, 0, SEEK_END);
        long n = ftell(f);
        fseek(f, 0, SEEK_SET);
        g_ground = malloc((size_t)n);
        if (g_ground && fread(g_ground, 1, (size_t)n, f) == (size_t)n) {
            g_ground_len = (size_t)n;
        } else {
            free(g_ground);
            g_ground = NULL;
        }
        fclose(f);
    }
    if (len) *len = g_ground_len;
    return g_ground;
}

/* ------------------------------------------------------------------ salida */

static void write_ppm(const char *path, const g3d_target *fb)
{
    FILE *f = fopen(path, "wb");
    if (!f) { fprintf(stderr, "no se pudo escribir %s\n", path); return; }
    fprintf(f, "P6\n%d %d\n255\n", fb->w, fb->h);
    for (int i = 0; i < fb->w * fb->h; i++) {
        uint16_t c = fb->color[i];
        uint8_t rgb[3];
        rgb[0] = (uint8_t)(((c >> 11) & 0x1F) * 255 / 31);
        rgb[1] = (uint8_t)(((c >> 5) & 0x3F) * 255 / 63);
        rgb[2] = (uint8_t)((c & 0x1F) * 255 / 31);
        fwrite(rgb, 1, 3, f);
    }
    fclose(f);
}

/* -------------------------------------------------------------- escenarios */

typedef struct { int frame; int x, y; } scripted_tap;

int main(int argc, char **argv)
{
    const char *outdir = (argc > 1) ? argv[1] : ".";
    const char *scene  = (argc > 2) ? argv[2] : "main";
    mkdir(outdir, 0755);

    g3d_target fb;
    fb.w = W;
    fb.h = H;
    fb.color = (uint16_t *)malloc(sizeof(uint16_t) * W * H);
    fb.depth = (float *)malloc(sizeof(float) * W * H);
    if (!fb.color || !fb.depth) return 1;

    scripted_tap taps[8];
    int ntaps = 0;
    int total_frames = 120;
    int shots[6] = { 20, 45, 70, 95, 115, -1 };
    bool sleeping = false, sick = false, dead = false;
    llama_stage force_stage = LLAMA_STAGE_COUNT;
    bool night = false;
    bool shake = false;
    bool rain = false;

    if (!strcmp(scene, "boot")) {
        total_frames = 40;
        shots[0] = 5; shots[1] = 20; shots[2] = 35; shots[3] = -1;
    } else if (!strcmp(scene, "food")) {
        taps[ntaps++] = (scripted_tap){ 10, 24, 260 };   /* boton Comer */
        taps[ntaps++] = (scripted_tap){ 45, 120, 190 };  /* elegir Manzana */
        total_frames = 200;
        shots[0] = 20; shots[1] = 40; shots[2] = 70; shots[3] = 120; shots[4] = 190; shots[5] = -1;
    } else if (!strcmp(scene, "stats")) {
        taps[ntaps++] = (scripted_tap){ 10, 120, 20 };
        total_frames = 40;
        shots[0] = 25; shots[1] = -1;
    } else if (!strcmp(scene, "minigame")) {
        taps[ntaps++] = (scripted_tap){ 10, 72, 260 };
        taps[ntaps++] = (scripted_tap){ 90, 120, 150 };
        taps[ntaps++] = (scripted_tap){ 150, 120, 150 };
        total_frames = 220;
        shots[0] = 30; shots[1] = 95; shots[2] = 130; shots[3] = 160; shots[4] = 210; shots[5] = -1;
    } else if (!strcmp(scene, "sleep")) {
        sleeping = true; night = true;
        total_frames = 90;
        shots[0] = 30; shots[1] = 60; shots[2] = 85; shots[3] = -1;
    } else if (!strcmp(scene, "sick")) {
        sick = true;
        total_frames = 80;
        shots[0] = 30; shots[1] = 70; shots[2] = -1;
    } else if (!strcmp(scene, "dead")) {
        dead = true;
        total_frames = 60;
        shots[0] = 20; shots[1] = 50; shots[2] = -1;
    } else if (!strcmp(scene, "cria")) {
        force_stage = LLAMA_STAGE_CRIA;
        total_frames = 90;
        shots[0] = 25; shots[1] = 55; shots[2] = 85; shots[3] = -1;
    } else if (!strcmp(scene, "night")) {
        night = true;
        total_frames = 80;
        shots[0] = 40; shots[1] = 75; shots[2] = -1;
    } else if (!strcmp(scene, "estaciones")) {
        /* Un cuadro por estacion: el reloj avanza tres meses entre capturas. */
        total_frames = 4 * 40;
        shots[0] = 35; shots[1] = 75; shots[2] = 115; shots[3] = 155; shots[4] = -1;
    } else if (!strncmp(scene, "grilla", 6)) {
        /* Una fila de la grilla estacion x hora: el digito elige la estacion. */
        int q = scene[6] ? scene[6] - '0' : 0;
        g_clock += (double)q * 91.0 * 86400.0;
        total_frames = 4 * 40;
        shots[0] = 35; shots[1] = 75; shots[2] = 115; shots[3] = 155; shots[4] = -1;
    } else if (!strcmp(scene, "horas")) {
        /* Amanecer, mediodia, atardecer y noche. */
        total_frames = 4 * 40;
        shots[0] = 35; shots[1] = 75; shots[2] = 115; shots[3] = 155; shots[4] = -1;
    } else if (!strcmp(scene, "stress")) {
        /* Golpea la pantalla al azar durante horas de juego simuladas. */
        total_frames = 40000;
        shots[0] = 39990; shots[1] = -1;
    } else if (!strcmp(scene, "turn")) {
        total_frames = 130;
        shots[0] = 10; shots[1] = 40; shots[2] = 70; shots[3] = 100; shots[4] = 125; shots[5] = -1;
    } else if (!strcmp(scene, "lluvia")) {
        rain = true;
        total_frames = 120;
        shots[0] = 40; shots[1] = 70; shots[2] = 100; shots[3] = -1;
    } else if (!strcmp(scene, "shake")) {
        shake = true;
        total_frames = 90;
        shots[0] = 20; shots[1] = 40; shots[2] = 60; shots[3] = 85; shots[4] = -1;
    }

    if (night) g_clock += 8.0 * 3600.0;   /* pasa a la noche */

    llama_game *game = llama_game_create(W, H);
    if (!game) { fprintf(stderr, "no se pudo crear el juego\n"); return 1; }

    /* Ajustes forzados del escenario sobre el estado inicial. */
    llama_pet *pet = (llama_pet *)llama_game_pet(game);
    if (sleeping) pet->flags |= LLAMA_FLAG_SLEEPING;
    if (sick)     { pet->flags |= LLAMA_FLAG_SICK; pet->health = 42.f; pet->hygiene = 18.f; }
    if (dead)     {
        pet->flags |= LLAMA_FLAG_DEAD;
        pet->health = 0.f;
        pet->care_mistakes = 7;
        game->screen = SCREEN_DEAD;
    }
    if (force_stage == LLAMA_STAGE_CRIA) pet->age_s = 600;
    else pet->age_s = 4 * 86400;          /* adulta por defecto */
    pet->wool = 72.f;                      /* muestra el boton de esquila */
    pet->poops = 2;
    pet->coins = 12;
    if (rain) {
        /* Chaparron con viento fijo, para la captura. */
        game->rain = game->rain_target = 0.9f;
        game->wind = game->wind_target = 2.8f;
        game->rain_timer = game->wind_timer = 1e6f;
    }

    const float dt = 1.f / 30.f;
    char path[512];
    int shot_idx = 0;

    for (int frame = 0; frame < total_frames; frame++) {
        llama_input in;
        memset(&in, 0, sizeof(in));
        in.ax = 0.02f; in.ay = -0.98f; in.az = 0.05f;
        in.battery_pct = 76;
        in.charging = false;

        for (int i = 0; i < ntaps; i++) {
            if (frame >= taps[i].frame && frame < taps[i].frame + 3) {
                in.touch_down = true;
                in.touch_x = taps[i].x;
                in.touch_y = taps[i].y;
            }
        }
        if (shake && frame > 20 && frame < 26) {
            in.ax = (frame & 1) ? 1.6f : -1.6f;
            in.az = (frame & 1) ? -1.2f : 1.4f;
        }
        /* En el arranque hay que tocar una vez para entrar. */
        if (strcmp(scene, "boot") && strcmp(scene, "dead") && frame == 2) {
            in.touch_down = true;
            in.touch_x = 120;
            in.touch_y = 150;
        }
        if (frame == 4 && strcmp(scene, "boot")) {
            in.touch_down = false;
        }

        if (!strcmp(scene, "stress")) {
            /* Toques y sacudidas seudoaleatorias, reproducibles. */
            uint32_t r = (uint32_t)frame * 2654435761u;
            in.touch_down = ((r >> 13) & 7) < 3;
            in.touch_x = (int)((r >> 3) % 240u);
            in.touch_y = (int)((r >> 17) % 284u);
            in.ax = ((float)((r >> 5) & 255) / 128.f) - 1.f;
            in.az = ((float)((r >> 21) & 255) / 128.f) - 1.f;
        }
        if (!strcmp(scene, "estaciones") && frame > 0 && frame % 40 == 0) {
            g_clock += 91.0 * 86400.0;          /* un trimestre */
        }
        if (!strcmp(scene, "horas") || !strncmp(scene, "grilla", 6)) {
            static const float HOURS[4] = { 6.3f, 13.f, 19.6f, 23.f };
            int slot = frame / 40;
            if (slot > 3) slot = 3;
            double day = floor(g_clock / 86400.0) * 86400.0;
            g_clock = day + HOURS[slot] * 3600.0;
        }
        if (!strcmp(scene, "turn")) game->cam_yaw = frame * 0.048f;
        llama_game_frame(game, &in, dt, &fb);
        g_clock += dt;
        g_millis += (uint32_t)(dt * 1000.f);

        const char *dump = getenv("LLAMA_DUMP");
        if (dump && frame % 2 == 0) {
            snprintf(path, sizeof(path), "%s/f_%04d.ppm", dump, frame);
            write_ppm(path, &fb);
        }
        if (shots[shot_idx] == frame) {
            snprintf(path, sizeof(path), "%s/%s_%02d.ppm", outdir, scene, shot_idx);
            write_ppm(path, &fb);
            printf("%s\n", path);
            shot_idx++;
            if (shot_idx >= 6 || shots[shot_idx] < 0) shot_idx = 5, shots[5] = -2;
        }
    }

    printf("triangulos dibujados en el ultimo cuadro: %d\n", game->ctx.tris_drawn);
    llama_game_destroy(game);
    free(fb.color);
    free(fb.depth);
    return 0;
}
