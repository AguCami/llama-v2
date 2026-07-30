/*
 * Puente WebAssembly: el mismo juego que corre en el ESP32-S3, en el navegador.
 *
 * El lado JS reserva los blobs de assets con lp_alloc(), llama lp_init() una
 * vez y despues lp_frame() por cuadro; el framebuffer RGB565 queda expuesto
 * con lp_framebuffer() y JS lo convierte a ImageData.
 */
#include "llama_game.h"
#include "llama_platform.h"
#include "g3d.h"
#include <stdlib.h>
#include <string.h>

#define W 240
#define H 284

#define EXPORT(name) __attribute__((export_name(#name)))

/* Reloj: JS manda el tiempo unix (con el desfase de la maquina del tiempo). */
static double  g_now;
static uint32_t g_millis;

double   llama_plat_time(void)   { return g_now; }
uint32_t llama_plat_millis(void) { return g_millis; }
uint32_t llama_plat_seed(void)   { return 0x5EED1234u ^ (uint32_t)g_now; }

/* Sonido y vibracion: avisos hacia JS (WebAudio / navigator.vibrate). */
__attribute__((import_module("env"), import_name("js_tone")))
void js_tone(int freq_hz, int ms);
__attribute__((import_module("env"), import_name("js_haptic")))
void js_haptic(int ms);

void llama_plat_tone(int f, int ms) { js_tone(f, ms); }
void llama_plat_haptic(int ms)      { js_haptic(ms); }

/* Partida guardada: un slot en memoria que JS espeja en localStorage. */
static uint8_t g_save[512];
static int     g_save_len;

bool llama_plat_save(const void *blob, size_t len)
{
    if (len > sizeof(g_save)) return false;
    memcpy(g_save, blob, len);
    g_save_len = (int)len;
    return true;
}

bool llama_plat_load(void *blob, size_t len)
{
    if (g_save_len <= 0 || len > (size_t)g_save_len) return false;
    memcpy(blob, g_save, len);
    return true;
}

EXPORT(lp_save_buf) uint8_t *lp_save_buf(void) { return g_save; }
EXPORT(lp_save_len) int      lp_save_len(void) { return g_save_len; }
EXPORT(lp_save_set) void     lp_save_set(int len) { g_save_len = len; }

/* Assets: JS los copia a memoria antes de lp_init(). */
static void  *g_sprites, *g_pano, *g_ground;
static size_t g_sprites_len, g_pano_len, g_ground_len;

const void *llama_plat_sprites(size_t *len) { if (len) *len = g_sprites_len; return g_sprites; }
const void *llama_plat_pano(size_t *len)    { if (len) *len = g_pano_len;    return g_pano; }
const void *llama_plat_ground(size_t *len)  { if (len) *len = g_ground_len;  return g_ground; }

EXPORT(lp_alloc) void *lp_alloc(int n) { return malloc((size_t)n); }
EXPORT(lp_set_sprites) void lp_set_sprites(void *p, int n) { g_sprites = p; g_sprites_len = (size_t)n; }
EXPORT(lp_set_pano)    void lp_set_pano(void *p, int n)    { g_pano = p;    g_pano_len = (size_t)n; }
EXPORT(lp_set_ground)  void lp_set_ground(void *p, int n)  { g_ground = p;  g_ground_len = (size_t)n; }

/* ------------------------------------------------------------------- juego */

static llama_game *g_game;
static g3d_target  g_fb;
static uint16_t    g_color[W * H];
static float       g_depth[W * H];

EXPORT(lp_init) int lp_init(double unix_time)
{
    g_now = unix_time;
    g_fb.w = W;
    g_fb.h = H;
    g_fb.color = g_color;
    g_fb.depth = g_depth;
    g_game = llama_game_create(W, H);
    return g_game != NULL;
}

EXPORT(lp_frame)
void lp_frame(double unix_time, float dt, int touch, int tx, int ty,
              float ax, float ay, float az)
{
    g_now = unix_time;
    g_millis += (uint32_t)(dt * 1000.f);
    llama_input in;
    memset(&in, 0, sizeof(in));
    in.touch_down = touch != 0;
    in.touch_x = tx;
    in.touch_y = ty;
    in.ax = ax; in.ay = ay; in.az = az;
    in.battery_pct = 88;
    llama_game_frame(g_game, &in, dt, &g_fb);
}

EXPORT(lp_framebuffer) uint16_t *lp_framebuffer(void) { return g_color; }
EXPORT(lp_width)  int lp_width(void)  { return W; }
EXPORT(lp_height) int lp_height(void) { return H; }
