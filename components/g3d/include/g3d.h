/*
 * g3d - motor 3D por software para pantallas pequenas (RGB565).
 *
 * Portable: solo usa la libreria estandar de C, no depende de ESP-IDF.
 * Sistema de coordenadas: Y arriba, Z hacia el observador (mano derecha).
 */
#ifndef G3D_H
#define G3D_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ------------------------------------------------------------------ color */

typedef uint16_t g3d_color; /* RGB565 */

static inline g3d_color g3d_rgb(uint8_t r, uint8_t g, uint8_t b)
{
    return (g3d_color)(((uint16_t)(r & 0xF8) << 8) | ((uint16_t)(g & 0xFC) << 3) | (b >> 3));
}

g3d_color g3d_color_shade(g3d_color c, float f);          /* multiplica brillo */
g3d_color g3d_color_lerp(g3d_color a, g3d_color b, float t);

/* ------------------------------------------------------------------- math */

typedef struct { float x, y, z; } g3d_v3;
typedef struct { float m[16]; } g3d_mat4; /* fila-mayor: m[fila*4 + col] */

static inline g3d_v3 g3d_v(float x, float y, float z) { g3d_v3 r = { x, y, z }; return r; }

g3d_v3 g3d_v3_add(g3d_v3 a, g3d_v3 b);
g3d_v3 g3d_v3_sub(g3d_v3 a, g3d_v3 b);
g3d_v3 g3d_v3_mul(g3d_v3 a, float s);
g3d_v3 g3d_v3_cross(g3d_v3 a, g3d_v3 b);
float  g3d_v3_dot(g3d_v3 a, g3d_v3 b);
float  g3d_v3_len(g3d_v3 a);
g3d_v3 g3d_v3_norm(g3d_v3 a);
g3d_v3 g3d_v3_lerp(g3d_v3 a, g3d_v3 b, float t);

g3d_mat4 g3d_mat4_identity(void);
g3d_mat4 g3d_mat4_mul(g3d_mat4 a, g3d_mat4 b);
g3d_mat4 g3d_mat4_translate(float x, float y, float z);
g3d_mat4 g3d_mat4_scale(float x, float y, float z);
g3d_mat4 g3d_mat4_rot_x(float a);
g3d_mat4 g3d_mat4_rot_y(float a);
g3d_mat4 g3d_mat4_rot_z(float a);
g3d_mat4 g3d_mat4_perspective(float fovy_rad, float aspect, float znear, float zfar);
g3d_mat4 g3d_mat4_look_at(g3d_v3 eye, g3d_v3 center, g3d_v3 up);

g3d_v3 g3d_mat4_point(const g3d_mat4 *m, g3d_v3 p);
g3d_v3 g3d_mat4_dir(const g3d_mat4 *m, g3d_v3 d);

/* --------------------------------------------------------------- superficie */

typedef struct {
    int        w, h;
    g3d_color *color; /* w*h pixeles RGB565 */
    float     *depth; /* w*h, guarda 1/w (mayor = mas cerca, 0 = vacio) */
} g3d_target;

void g3d_clear_color(g3d_target *t, g3d_color c);
void g3d_clear_depth(g3d_target *t);
void g3d_sky_gradient(g3d_target *t, g3d_color top, g3d_color bottom, int y0, int y1);

/* -------------------------------------------------------------------- mesh */

#define G3D_TRI_SMOOTH 0x01u   /* usa las normales por vertice (gouraud) */

typedef struct { uint16_t a, b, c; g3d_color color; uint8_t flags; } g3d_tri;

typedef struct {
    g3d_v3  *v;
    g3d_v3  *n;      /* normales por vertice (solo para caras suaves) */
    int      nv, cap_v;
    g3d_tri *t;
    int      nt, cap_t;
} g3d_mesh;

bool g3d_mesh_init(g3d_mesh *m, int max_v, int max_t);
void g3d_mesh_free(g3d_mesh *m);
void g3d_mesh_reset(g3d_mesh *m);
int  g3d_mesh_vertex(g3d_mesh *m, g3d_v3 p);
void g3d_mesh_tri(g3d_mesh *m, int a, int b, int c, g3d_color col);
void g3d_mesh_quad(g3d_mesh *m, int a, int b, int c, int d, g3d_color col);
void g3d_mesh_transform(g3d_mesh *m, g3d_mat4 xf);
/* Igual, pero solo sobre los vertices agregados a partir de `first_v`. */
void g3d_mesh_transform_from(g3d_mesh *m, int first_v, g3d_mat4 xf);

/* Caja centrada en `c` con semiejes `half`. */
void g3d_mesh_box(g3d_mesh *m, g3d_v3 c, g3d_v3 half, g3d_color col);
/* Prisma: base rectangular (bw x bd) en `base`, tapa (tw x td) desplazada `top_off`. */
void g3d_mesh_frustum(g3d_mesh *m, g3d_v3 base, float bw, float bd,
                      float tw, float td, float height, g3d_v3 top_off, g3d_color col);
/* Piramide de base rectangular. */
void g3d_mesh_pyramid(g3d_mesh *m, g3d_v3 base, float bw, float bd, float height,
                      g3d_v3 apex_off, g3d_color col);

/*
 * Superficie de revolucion alrededor del eje Y: la herramienta principal para
 * las formas organicas (cuerpo, cuello, cabeza, patas). Cada anillo tiene su
 * altura, sus dos radios (seccion eliptica) y un desplazamiento del centro,
 * que permite curvar la pieza. Las caras laterales salen con normales por
 * vertice, asi que se sombrean suave.
 *
 * `a0`/`a1` acotan el arco (usar 0 y 2*PI para una pieza cerrada); con un arco
 * parcial se obtiene una cascara, util para la manta.
 */
typedef struct { float y, rx, rz, cx, cz; } g3d_ring;

#define G3D_CAP_LO 0x01u
#define G3D_CAP_HI 0x02u

void g3d_mesh_revolve(g3d_mesh *m, const g3d_ring *rings, int nrings, int sides,
                      float a0, float a1, g3d_color col, unsigned caps);

/* Esfera achatada, construida sobre g3d_mesh_revolve. */
void g3d_mesh_blob(g3d_mesh *m, g3d_v3 center, float rx, float ry, float rz,
                   int sides, int stacks, g3d_color col);

/* -------------------------------------------------------------------- draw */

typedef struct {
    g3d_v3    light_dir; /* direccion HACIA la luz, normalizada */
    float     ambient;   /* 0..1 */
    float     diffuse;   /* 0..1 */
    float     rim;       /* realce en bordes, 0..1 */
    g3d_color fog_color; /* color del horizonte */
    float     fog_start; /* distancia donde empieza la niebla */
    float     fog_end;   /* distancia donde todo es niebla */
} g3d_light;

typedef struct {
    g3d_mat4  view_proj;
    g3d_mat4  view;
    g3d_v3    eye;
    float     proj_f; /* factor de escala vertical de la proyeccion */
    g3d_light light;
    int       tris_drawn; /* estadistica del ultimo frame */
} g3d_ctx;

void g3d_ctx_camera(g3d_ctx *ctx, g3d_v3 eye, g3d_v3 target, float fovy_rad,
                    float aspect, float znear, float zfar);

/* Dibuja una malla. `tint` multiplica el color final (1.0 = normal). */
void g3d_draw_mesh(g3d_target *t, const g3d_ctx *ctx, const g3d_mesh *m,
                   const g3d_mat4 *model, float tint);

/* Proyecta un punto del mundo a pantalla. Devuelve false si queda detras. */
bool g3d_project(const g3d_target *t, const g3d_ctx *ctx, g3d_v3 world,
                 float *sx, float *sy, float *scale);

#ifdef __cplusplus
}
#endif
#endif /* G3D_H */
