// kit_dev.h - developer modes for a raylib game: a sound board, an asset gallery and your own
// sandboxes, all inside the game's own binary and listed by one command.
//
// Single header. In ONE .c file:
//     #define KIT_DEV_IMPLEMENTATION
//     #include "kit_dev.h"
//
//     int main(int argc, char **argv) {
//         kit_dev_sounds(SOUNDS, SOUND_COUNT);                       // register what the game has...
//         kit_dev_gallery("props", "every prop, alone on a stage", PROPS, PROP_COUNT, draw_prop, PROP_KEYS, 2);
//         kit_dev_custom("arena", "empty arena, no enemies", ARENA_KEYS, 3);
//         kit_dev_init(argc, argv);                                  // ...then read --dev (prints the list and exits if asked)
//
//         InitWindow(1280, 720, "game");
//         InitAudioDevice();
//         if (kit_dev_run()) { CloseWindow(); return 0; }            // a built-in mode ran instead of the game
//         bool arena = kit_dev_is("arena");                          // your own modes: branch on them
//         ... the game ...
//     }
//
//     ./game --dev                 list every mode, its items and its keys
//     ./game --dev sounds          the sound board
//     ./game --dev props barrel    the gallery, starting on one item
//     ./game --dev props --shots out/     save one PNG per item, then quit (add --state 3 for that state)
//
// MIT license. https://github.com/binRick/raylib-dev-kit
#ifndef KIT_DEV_H
#define KIT_DEV_H

#include "raylib.h"
#include <stdbool.h>

// ---- the sound board ------------------------------------------------------------------
typedef enum {
    KIT_DEV_OK,        // has a sound, and it is right
    KIT_DEV_MISSING,   // a moment in the game with no sound yet (also set automatically when the file is absent)
    KIT_DEV_SHARED,    // borrows a sound that belongs to something else
    KIT_DEV_IMPROVE    // plays, but falls short
} KitDevStatus;

typedef struct {
    const char *group;     // "player", "enemies", "interface"...
    const char *name;      // what you call it
    const char *trigger;   // the moment that plays it, in words
    const char *file;      // NULL when there is none yet
    KitDevStatus status;
    const char *note;      // shown when the row is hovered (may be NULL)
} KitDevSound;

// ---- galleries --------------------------------------------------------------------------
typedef struct {
    const char *name;      // also what you type: ./game --dev props <name>
    const char *file;      // a model to load (.glb .obj .iqm ...). NULL when `draw` makes the item itself
    const char *note;      // shown under the name (may be NULL)
} KitDevItem;

typedef struct { const char *keys, *does; } KitDevKey;   // one line of help: { "1-3", "damage state" }

typedef struct {
    int item;              // index into the items you registered
    int state;             // the last number key pressed, 1-9 (starts at 1)
    float time;            // seconds on this item; stops while paused
    bool paused;
    Model model;           // the item's file, loaded (meshCount == 0 when it has none)
} KitDevStage;

// Called between BeginMode3D and EndMode3D, with the item standing at the origin on y = 0.
// Pass NULL to kit_dev_gallery to have each item's `file` drawn for you.
typedef void (*KitDevDraw)(const KitDevStage *stage);

void kit_dev_sounds(const KitDevSound *rows, int count);
void kit_dev_gallery(const char *mode, const char *about, const KitDevItem *items, int count,
                     KitDevDraw draw, const KitDevKey *keys, int key_count);
void kit_dev_custom(const char *mode, const char *about, const KitDevKey *keys, int key_count);

void kit_dev_init(int argc, char **argv);   // before InitWindow: `--dev` alone prints the list and exits
bool kit_dev_active(void);                  // was --dev <mode> given?
bool kit_dev_is(const char *mode);          // ...and is it this one?
const char *kit_dev_item(void);             // the item named after the mode, or NULL

bool kit_dev_run(void);    // runs a built-in mode until its window closes. false: not a built-in mode, run the game
bool kit_dev_step(void);   // one frame of it, for platforms that own the loop (browser, iOS). false when done
Shader kit_dev_shader(void);   // the gallery's simple lit shader, for models you draw in a KitDevDraw

#endif // KIT_DEV_H

#ifdef KIT_DEV_IMPLEMENTATION
#ifndef KIT_DEV_IMPLEMENTATION_DONE
#define KIT_DEV_IMPLEMENTATION_DONE

#include "raymath.h"
#include "rlgl.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define KIT_DEV__MAX_MODES 16
#define KIT_DEV__MAX_ROWS 256
#define KIT_DEV__MAX_ITEMS 128

typedef enum { KIT_DEV__SOUNDS, KIT_DEV__GALLERY, KIT_DEV__CUSTOM } KitDev__Kind;

typedef struct {
    const char *name, *about;
    KitDev__Kind kind;
    const KitDevSound *rows;
    const KitDevItem *items;
    int count;
    KitDevDraw draw;
    const KitDevKey *keys;
    int key_count;
} KitDev__Mode;

static struct {
    KitDev__Mode modes[KIT_DEV__MAX_MODES];
    int mode_count, current;       // current: index of the mode being run, -1 for none
    const char *item_arg, *shots;
    int start_state;               // --state <n>: the state items open in
    bool ready, done;
    // sound board
    Sound sounds[KIT_DEV__MAX_ROWS];
    float lengths[KIT_DEV__MAX_ROWS];
    KitDevStatus status[KIT_DEV__MAX_ROWS];
    int playing;
    bool shopping;
    float scroll;
    // gallery
    Model models[KIT_DEV__MAX_ITEMS];
    ModelAnimation *anims[KIT_DEV__MAX_ITEMS];
    int anim_count[KIT_DEV__MAX_ITEMS];
    KitDevStage stage;
    float yaw, pitch, dist;
    bool spin, dragging;
    int settle;                    // frames drawn on the current item (for --shots)
    Shader shader;
} kdv = { .current = -1, .start_state = 1 };

static const KitDevKey KIT_DEV__SOUND_KEYS[] = {
    { "click a row", "play it" }, { "Space", "stop" }, { "wheel / arrows", "scroll" },
    { "Tab", "next group" }, { "M", "only the shopping list (missing, shared, to improve)" }, { "Esc", "quit" },
};
static const KitDevKey KIT_DEV__GALLERY_KEYS[] = {
    { "Tab / ]", "next item" }, { "[", "previous" }, { "click the list", "jump to an item" },
    { "drag / arrows", "turn and tilt" }, { "wheel", "zoom" }, { "R", "spin on/off" }, { "C", "reset the view" },
    { "1-9", "state (an animation clip, for a loaded model)" }, { "Space", "pause" }, { "Esc", "quit" },
};

static KitDev__Mode *kit_dev__add(const char *name, const char *about, KitDev__Kind kind) {
    if (kdv.mode_count >= KIT_DEV__MAX_MODES) return NULL;
    KitDev__Mode *m = &kdv.modes[kdv.mode_count++];
    memset(m, 0, sizeof *m);
    m->name = name;
    m->about = about;
    m->kind = kind;
    return m;
}

void kit_dev_sounds(const KitDevSound *rows, int count) {
    KitDev__Mode *m = kit_dev__add("sounds", "every sound, the moment that plays it, and what is still missing", KIT_DEV__SOUNDS);
    if (!m) return;
    m->rows = rows;
    m->count = count > KIT_DEV__MAX_ROWS ? KIT_DEV__MAX_ROWS : count;
}

void kit_dev_gallery(const char *mode, const char *about, const KitDevItem *items, int count, KitDevDraw draw, const KitDevKey *keys, int key_count) {
    KitDev__Mode *m = kit_dev__add(mode, about, KIT_DEV__GALLERY);
    if (!m) return;
    m->items = items;
    m->count = count > KIT_DEV__MAX_ITEMS ? KIT_DEV__MAX_ITEMS : count;
    m->draw = draw;
    m->keys = keys;
    m->key_count = key_count;
}

void kit_dev_custom(const char *mode, const char *about, const KitDevKey *keys, int key_count) {
    KitDev__Mode *m = kit_dev__add(mode, about, KIT_DEV__CUSTOM);
    if (!m) return;
    m->keys = keys;
    m->key_count = key_count;
}

static void kit_dev__print_keys(FILE *f, const KitDevKey *keys, int n) {
    for (int i = 0; i < n; i++) fprintf(f, "      %-18s %s\n", keys[i].keys, keys[i].does);
}

// The help is printed from what was registered, so it cannot drift from the code.
static void kit_dev__list(FILE *f, const char *prog) {
    fprintf(f, "Dev modes:  %s --dev <mode> [item] [--state <n>] [--shots <dir>]\n", prog);
    for (int i = 0; i < kdv.mode_count; i++) {
        const KitDev__Mode *m = &kdv.modes[i];
        fprintf(f, "\n  %-10s %s\n", m->name, m->about ? m->about : "");
        if (m->kind == KIT_DEV__SOUNDS) {
            int todo = 0;
            for (int r = 0; r < m->count; r++) todo += m->rows[r].status != KIT_DEV_OK || !m->rows[r].file;
            fprintf(f, "      %d sounds, %d on the shopping list\n", m->count, todo);
            kit_dev__print_keys(f, KIT_DEV__SOUND_KEYS, 6);
        } else if (m->kind == KIT_DEV__GALLERY) {
            fprintf(f, "      items:");
            for (int r = 0; r < m->count; r++) fprintf(f, " %s", m->items[r].name);
            fprintf(f, "\n");
            kit_dev__print_keys(f, m->keys, m->key_count);
            kit_dev__print_keys(f, KIT_DEV__GALLERY_KEYS, 10);
        } else kit_dev__print_keys(f, m->keys, m->key_count);
    }
}

void kit_dev_init(int argc, char **argv) {
    int at = -1;
    for (int i = 1; i < argc; i++) if (!strcmp(argv[i], "--dev")) { at = i; break; }
    if (at < 0) return;
    const char *mode = (at + 1 < argc && argv[at + 1][0] != '-') ? argv[at + 1] : NULL;
    if (!mode || !strcmp(mode, "list") || !strcmp(mode, "help")) { kit_dev__list(stdout, argv[0]); exit(0); }
    for (int i = 0; i < kdv.mode_count; i++) if (!strcmp(kdv.modes[i].name, mode)) kdv.current = i;
    if (kdv.current < 0) {
        fprintf(stderr, "unknown dev mode '%s' -- one of:", mode);
        for (int i = 0; i < kdv.mode_count; i++) fprintf(stderr, " %s", kdv.modes[i].name);
        fprintf(stderr, "\n");
        exit(2);
    }
    const KitDev__Mode *m = &kdv.modes[kdv.current];
    if (at + 2 < argc && argv[at + 2][0] != '-') kdv.item_arg = argv[at + 2];
    for (int i = 1; i + 1 < argc; i++) if (!strcmp(argv[i], "--shots")) kdv.shots = argv[i + 1];
    for (int i = 1; i + 1 < argc; i++) if (!strcmp(argv[i], "--state") && atoi(argv[i + 1]) >= 1) kdv.start_state = atoi(argv[i + 1]);
    if (kdv.item_arg && m->kind == KIT_DEV__GALLERY) {
        bool found = false;
        for (int i = 0; i < m->count; i++) if (!strcmp(m->items[i].name, kdv.item_arg)) { kdv.stage.item = i; found = true; }
        if (!found) {
            fprintf(stderr, "unknown %s item '%s' -- one of:", m->name, kdv.item_arg);
            for (int i = 0; i < m->count; i++) fprintf(stderr, " %s", m->items[i].name);
            fprintf(stderr, "\n");
            exit(2);
        }
    }
}

bool kit_dev_active(void) { return kdv.current >= 0; }
bool kit_dev_is(const char *mode) { return kdv.current >= 0 && !strcmp(kdv.modes[kdv.current].name, mode); }
const char *kit_dev_item(void) { return kdv.item_arg; }

// ---- shared drawing helpers ---------------------------------------------------------------
static const Color KIT_DEV__BG = { 16, 18, 30, 255 }, KIT_DEV__PANEL = { 26, 30, 48, 255 }, KIT_DEV__LINE = { 52, 60, 92, 255 },
                   KIT_DEV__TEXT = { 226, 232, 250, 255 }, KIT_DEV__DIM = { 140, 152, 190, 255 }, KIT_DEV__ACCENT = { 120, 225, 235, 255 };

static float kit_dev__s(void) { float s = GetScreenHeight() / 720.0f; return s < 0.75f ? 0.75f : s; }   // UI scale
static int kit_dev__font(float px) { int f = (int)(px * kit_dev__s()); return f < 10 ? 10 : f; }

static void kit_dev__text(const char *t, float x, float y, float px, Color c) { DrawText(t, (int)x, (int)y, kit_dev__font(px), c); }

// Text cut to a width with "..." so one long cell cannot run into the next column.
static void kit_dev__clip(const char *t, float x, float y, float px, float width, Color c) {
    if (!t) return;
    int font = kit_dev__font(px);
    if (MeasureText(t, font) <= width) { DrawText(t, (int)x, (int)y, font, c); return; }
    char buf[200];
    int n = (int)strlen(t);
    if (n > 190) n = 190;
    while (n > 1) {
        snprintf(buf, sizeof buf, "%.*s...", n, t);
        if (MeasureText(buf, font) <= width) break;
        n--;
    }
    DrawText(buf, (int)x, (int)y, font, c);
}

static void kit_dev__capture(const char *dir, const char *name) {
    MakeDirectory(dir);
    rlDrawRenderBatchActive();
    // Not TakeScreenshot(): on a HiDPI window raylib scales an already-scaled size and saves 4x the window.
    int w = GetRenderWidth(), h = GetRenderHeight();
    Image img = { rlReadScreenPixels(w, h), w, h, 1, PIXELFORMAT_UNCOMPRESSED_R8G8B8A8 };
    ExportImage(img, TextFormat("%s/%s.png", dir, name));
    UnloadImage(img);
}

// ---- the sound board ------------------------------------------------------------------------
static const char *KIT_DEV__TAG[] = { "", "MISSING", "SHARED", "IMPROVE" };
static const Color KIT_DEV__TAGCOL[] = { { 0 }, { 240, 84, 104, 255 }, { 255, 205, 80, 255 }, { 167, 139, 250, 255 } };

static void kit_dev__sounds_load(const KitDev__Mode *m) {
    kdv.playing = -1;
    for (int i = 0; i < m->count; i++) {
        kdv.status[i] = m->rows[i].status;
        if (m->rows[i].file && FileExists(m->rows[i].file)) {
            kdv.sounds[i] = LoadSound(m->rows[i].file);
            if (kdv.sounds[i].stream.sampleRate) kdv.lengths[i] = (float)kdv.sounds[i].frameCount / kdv.sounds[i].stream.sampleRate;
        } else {
            kdv.status[i] = KIT_DEV_MISSING;   // a row whose file is not on disk is missing, whatever the table says
            if (m->rows[i].file) TraceLog(LOG_WARNING, "KIT_DEV: sound '%s': %s not found", m->rows[i].name, m->rows[i].file);
        }
    }
}

static void kit_dev__sounds_step(const KitDev__Mode *m) {
    float s = kit_dev__s(), w = (float)GetScreenWidth(), h = (float)GetScreenHeight();
    float row = 28 * s, top = 64 * s, bottom = h - 44 * s, pad = 16 * s;
    Vector2 mouse = GetMousePosition();

    if (IsKeyPressed(KEY_M)) { kdv.shopping = !kdv.shopping; kdv.scroll = 0; }
    if (IsKeyPressed(KEY_SPACE) && kdv.playing >= 0) { StopSound(kdv.sounds[kdv.playing]); kdv.playing = -1; }
    kdv.scroll -= GetMouseWheelMove() * row * 2;
    if (IsKeyDown(KEY_DOWN)) kdv.scroll += row * 12 * GetFrameTime();
    if (IsKeyDown(KEY_UP)) kdv.scroll -= row * 12 * GetFrameTime();
    if (kdv.playing >= 0 && !IsSoundPlaying(kdv.sounds[kdv.playing])) kdv.playing = -1;

    int counts[4] = { 0 }, shown = 0, hover = -1;
    for (int i = 0; i < m->count; i++) counts[kdv.status[i]]++;

    // columns
    float x_tag = pad, x_name = x_tag + 96 * s, x_trig = x_name + w * 0.20f, x_file = w * 0.66f, x_len = w - 70 * s;

    BeginDrawing();
    ClearBackground(KIT_DEV__BG);
    BeginScissorMode(0, (int)top, (int)w, (int)(bottom - top));
    float y = top - kdv.scroll, next_group_y = -1;
    const char *group = NULL;
    for (int i = 0; i < m->count; i++) {
        const KitDevSound *r = &m->rows[i];
        if (kdv.shopping && kdv.status[i] == KIT_DEV_OK) continue;
        if (!group || strcmp(group, r->group)) {   // a group heading
            group = r->group;
            if (shown) y += row * 0.4f;
            if (next_group_y < 0 && y - top > row * 0.5f) next_group_y = y;
            kit_dev__text(group, pad, y + 5 * s, 20, KIT_DEV__ACCENT);
            DrawLine((int)pad, (int)(y + row - 2), (int)(w - pad), (int)(y + row - 2), KIT_DEV__LINE);
            y += row;
        }
        Rectangle box = { 0, y, w, row };
        bool over = CheckCollisionPointRec(mouse, box) && mouse.y > top && mouse.y < bottom;
        if (over) hover = i;
        if (i == kdv.playing) DrawRectangleRec(box, Fade(KIT_DEV__ACCENT, 0.22f));
        else if (over) DrawRectangleRec(box, KIT_DEV__PANEL);
        if (kdv.status[i] != KIT_DEV_OK) {
            DrawRectangle((int)x_tag, (int)(y + 5 * s), (int)(84 * s), (int)(row - 10 * s), Fade(KIT_DEV__TAGCOL[kdv.status[i]], 0.2f));
            kit_dev__text(KIT_DEV__TAG[kdv.status[i]], x_tag + 6 * s, y + 8 * s, 12, KIT_DEV__TAGCOL[kdv.status[i]]);
        }
        Color main = kdv.status[i] == KIT_DEV_MISSING ? KIT_DEV__DIM : KIT_DEV__TEXT;
        kit_dev__clip(r->name, x_name, y + 6 * s, 16, x_trig - x_name - 12 * s, main);
        kit_dev__clip(r->trigger, x_trig, y + 6 * s, 16, x_file - x_trig - 12 * s, KIT_DEV__DIM);
        kit_dev__clip(r->file ? r->file : "-", x_file, y + 6 * s, 16, x_len - x_file - 12 * s, KIT_DEV__DIM);
        if (kdv.lengths[i] > 0) kit_dev__text(TextFormat("%.2fs", kdv.lengths[i]), x_len, y + 6 * s, 16, KIT_DEV__DIM);
        if (over && IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && kdv.lengths[i] > 0) {
            if (kdv.playing >= 0) StopSound(kdv.sounds[kdv.playing]);
            PlaySound(kdv.sounds[i]);
            kdv.playing = i;
        }
        y += row;
        shown++;
    }
    EndScissorMode();
    float content = y + kdv.scroll - top, room = bottom - top;
    if (IsKeyPressed(KEY_TAB)) kdv.scroll = next_group_y > 0 ? kdv.scroll + (next_group_y - top) : 0;   // past the last group: back to the top
    if (kdv.scroll > content - room) kdv.scroll = content - room;
    if (kdv.scroll < 0) kdv.scroll = 0;

    // header and footer sit over the list
    DrawRectangle(0, 0, (int)w, (int)top, KIT_DEV__PANEL);
    kit_dev__text("SOUND BOARD", pad, 12 * s, 20, KIT_DEV__TEXT);
    const char *sum = TextFormat("%d sounds    %d missing    %d shared    %d to improve%s", m->count, counts[1], counts[2], counts[3], kdv.shopping ? "    [shopping list]" : "");
    kit_dev__text(sum, pad + 190 * s, 15 * s, 16, counts[1] + counts[2] + counts[3] ? KIT_DEV__TAGCOL[2] : KIT_DEV__DIM);
    kit_dev__text("NAME", x_name, 42 * s, 12, KIT_DEV__DIM);
    kit_dev__text("PLAYS WHEN", x_trig, 42 * s, 12, KIT_DEV__DIM);
    kit_dev__text("FILE", x_file, 42 * s, 12, KIT_DEV__DIM);
    kit_dev__text("LENGTH", x_len - 10 * s, 42 * s, 12, KIT_DEV__DIM);
    DrawRectangle(0, (int)bottom, (int)w, (int)(h - bottom), KIT_DEV__PANEL);
    const char *note = hover >= 0 && m->rows[hover].note ? m->rows[hover].note : "click a row: play    Space: stop    Tab: next group    M: shopping list    Esc: quit";
    kit_dev__clip(note, pad, bottom + 13 * s, 16, w - pad * 2, hover >= 0 && m->rows[hover].note ? KIT_DEV__TEXT : KIT_DEV__DIM);
    if (kdv.shots && ++kdv.settle == 3) { kit_dev__capture(kdv.shots, m->name); kdv.done = true; }
    EndDrawing();
}

// ---- galleries --------------------------------------------------------------------------------
#if defined(__EMSCRIPTEN__) || defined(PLATFORM_IOS) || defined(GRAPHICS_API_OPENGL_ES3)
#define KIT_DEV__GLSL "#version 300 es\nprecision mediump float;\n"
#else
#define KIT_DEV__GLSL "#version 330\n"
#endif
// One sun, one fill and some ambient: enough to read a shape. Not the game's lighting.
static const char *KIT_DEV__VS = KIT_DEV__GLSL
    "in vec3 vertexPosition; in vec2 vertexTexCoord; in vec3 vertexNormal; in vec4 vertexColor;\n"
    "uniform mat4 mvp; uniform mat4 matModel; uniform mat4 matNormal;\n"
    "out vec2 uv; out vec4 col; out vec3 nrm; out vec3 wpos;\n"
    "void main() { uv = vertexTexCoord; col = vertexColor; nrm = vec3(matNormal * vec4(vertexNormal, 0.0));\n"
    "  wpos = vec3(matModel * vec4(vertexPosition, 1.0)); gl_Position = mvp * vec4(vertexPosition, 1.0); }\n";
// A mesh with no normals (some generated meshes, some exports) would light as NaN and draw
// black, so the normal falls back to the face's own, worked out from the surface itself.
static const char *KIT_DEV__FS = KIT_DEV__GLSL
    "in vec2 uv; in vec4 col; in vec3 nrm; in vec3 wpos;\n"
    "uniform sampler2D texture0; uniform vec4 colDiffuse;\n"
    "out vec4 finalColor;\n"
    "void main() { vec3 n = dot(nrm, nrm) > 0.0001 ? normalize(nrm) : normalize(cross(dFdx(wpos), dFdy(wpos)));\n"
    "  float sun = max(dot(n, normalize(vec3(0.5, 0.8, 0.35))), 0.0);\n"
    "  float fill = max(dot(n, normalize(vec3(-0.6, 0.2, -0.5))), 0.0);\n"
    "  vec4 base = texture(texture0, uv) * colDiffuse * col;\n"
    "  finalColor = vec4(base.rgb * (0.30 + 0.80 * sun + 0.22 * fill), base.a); }\n";

Shader kit_dev_shader(void) {
    if (!kdv.shader.id) kdv.shader = LoadShaderFromMemory(KIT_DEV__VS, KIT_DEV__FS);
    return kdv.shader;
}

static void kit_dev__view_reset(void) { kdv.yaw = 0.6f; kdv.pitch = 0.35f; kdv.dist = 6.0f; }

static void kit_dev__gallery_load(const KitDev__Mode *m) {
    kit_dev__view_reset();
    kdv.spin = true;
    kdv.stage.state = kdv.start_state;
    for (int i = 0; i < m->count; i++) {
        if (!m->items[i].file) continue;
        if (!FileExists(m->items[i].file)) { TraceLog(LOG_WARNING, "KIT_DEV: %s '%s': %s not found", m->name, m->items[i].name, m->items[i].file); continue; }
        kdv.models[i] = LoadModel(m->items[i].file);
        for (int k = 0; k < kdv.models[i].materialCount; k++) kdv.models[i].materials[k].shader = kit_dev_shader();
        kdv.anims[i] = LoadModelAnimations(m->items[i].file, &kdv.anim_count[i]);
    }
}

static void kit_dev__select(const KitDev__Mode *m, int item) {
    kdv.stage.item = (item % m->count + m->count) % m->count;
    kdv.stage.time = 0;
    kdv.stage.state = kdv.start_state;
    kdv.settle = 0;
}

// A loaded model, stood on the floor and scaled to a common height so every item fills the stage.
static void kit_dev__draw_model(const KitDevStage *st) {
    Model model = st->model;
    if (!model.meshCount) return;
    int i = st->item, clip = st->state - 1;
    if (clip < kdv.anim_count[i] && kdv.anims[i][clip].keyframeCount > 0)
        UpdateModelAnimation(model, kdv.anims[i][clip], fmodf(st->time * 30.0f, (float)kdv.anims[i][clip].keyframeCount));
    BoundingBox b = GetModelBoundingBox(model);
    float height = b.max.y - b.min.y, wide = fmaxf(b.max.x - b.min.x, b.max.z - b.min.z);
    float k = 2.4f / fmaxf(fmaxf(height, wide), 0.0001f);
    Vector3 at = { -(b.min.x + b.max.x) / 2 * k, -b.min.y * k, -(b.min.z + b.max.z) / 2 * k };
    DrawModelEx(model, at, (Vector3){ 0, 1, 0 }, 0, (Vector3){ k, k, k }, WHITE);
}

static void kit_dev__gallery_step(const KitDev__Mode *m) {
    float s = kit_dev__s(), w = (float)GetScreenWidth(), h = (float)GetScreenHeight(), dt = GetFrameTime();
    float list_w = 210 * s, row = 28 * s, pad = 14 * s, foot = 40 * s;
    Vector2 mouse = GetMousePosition();
    KitDevStage *st = &kdv.stage;

    if (IsKeyPressed(KEY_TAB) || IsKeyPressed(KEY_RIGHT_BRACKET)) kit_dev__select(m, st->item + 1);
    if (IsKeyPressed(KEY_LEFT_BRACKET)) kit_dev__select(m, st->item - 1);
    for (int k = 0; k < 9; k++) if (IsKeyPressed(KEY_ONE + k)) { st->state = k + 1; st->time = 0; }
    if (IsKeyPressed(KEY_SPACE)) st->paused = !st->paused;
    if (IsKeyPressed(KEY_R)) kdv.spin = !kdv.spin;
    if (IsKeyPressed(KEY_C)) kit_dev__view_reset();
    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
        if (mouse.x < list_w) { int hit = (int)((mouse.y - 56 * s) / row); if (hit >= 0 && hit < m->count && mouse.y >= 56 * s) kit_dev__select(m, hit); }
        else kdv.dragging = true;
    }
    if (IsMouseButtonReleased(MOUSE_BUTTON_LEFT)) kdv.dragging = false;
    if (kdv.dragging) { Vector2 d = GetMouseDelta(); kdv.yaw -= d.x * 0.008f; kdv.pitch += d.y * 0.008f; }
    if (IsKeyDown(KEY_LEFT)) kdv.yaw += dt * 1.6f;
    if (IsKeyDown(KEY_RIGHT)) kdv.yaw -= dt * 1.6f;
    if (IsKeyDown(KEY_UP)) kdv.pitch += dt;
    if (IsKeyDown(KEY_DOWN)) kdv.pitch -= dt;
    if (mouse.x > list_w) kdv.dist *= 1.0f - GetMouseWheelMove() * 0.08f;
    if (kdv.spin && !kdv.dragging && !st->paused && !kdv.shots) kdv.yaw += dt * 0.35f;
    kdv.pitch = Clamp(kdv.pitch, -0.2f, 1.45f);
    kdv.dist = Clamp(kdv.dist, 2.0f, 30.0f);
    if (!st->paused) st->time += kdv.shots ? 1 / 60.0f : dt;
    st->model = kdv.models[st->item];

    Camera3D cam = { 0 };
    cam.target = (Vector3){ 0, 1.1f, 0 };
    cam.position = (Vector3){ cam.target.x + sinf(kdv.yaw) * cosf(kdv.pitch) * kdv.dist, cam.target.y + sinf(kdv.pitch) * kdv.dist,
                              cam.target.z + cosf(kdv.yaw) * cosf(kdv.pitch) * kdv.dist };
    cam.up = (Vector3){ 0, 1, 0 };
    cam.fovy = 40;
    cam.projection = CAMERA_PERSPECTIVE;

    BeginDrawing();
    ClearBackground(KIT_DEV__BG);   // also clears the depth buffer: a gradient alone leaves last frame's depth behind
    DrawRectangleGradientV(0, 0, (int)w, (int)h, (Color){ 30, 36, 60, 255 }, KIT_DEV__BG);
    // the 3D view gets its own viewport to the right of the list, so the item sits in the middle of the stage
    rlDrawRenderBatchActive();   // the backdrop must be drawn before the viewport changes under it
    rlViewport((int)(list_w * GetRenderWidth() / w), 0, (int)((w - list_w) * GetRenderWidth() / w), GetRenderHeight());
    BeginMode3D(cam);
    rlSetMatrixProjection(MatrixPerspective(cam.fovy * DEG2RAD, (double)(w - list_w) / h, 0.05, 500.0));
    DrawGrid(12, 0.5f);
    if (m->draw) m->draw(st);
    else kit_dev__draw_model(st);
    EndMode3D();
    rlViewport(0, 0, GetRenderWidth(), GetRenderHeight());

    // the list
    DrawRectangle(0, 0, (int)list_w, (int)h, KIT_DEV__PANEL);
    kit_dev__clip(TextFormat("%s", m->name), pad, 14 * s, 20, list_w - pad * 2, KIT_DEV__ACCENT);
    kit_dev__text(TextFormat("%d of %d", st->item + 1, m->count), pad, 38 * s, 12, KIT_DEV__DIM);
    for (int i = 0; i < m->count; i++) {
        float y = 56 * s + i * row;
        if (i == st->item) DrawRectangle(0, (int)y, (int)list_w, (int)row, Fade(KIT_DEV__ACCENT, 0.22f));
        else if (mouse.x < list_w && mouse.y >= y && mouse.y < y + row) DrawRectangle(0, (int)y, (int)list_w, (int)row, KIT_DEV__LINE);
        kit_dev__clip(m->items[i].name, pad, y + 6 * s, 16, list_w - pad * 2, i == st->item ? KIT_DEV__TEXT : KIT_DEV__DIM);
    }

    // what this item is
    const KitDevItem *it = &m->items[st->item];
    float x = list_w + pad, y = 14 * s;
    kit_dev__text(it->name, x, y, 30, KIT_DEV__TEXT);
    y += 38 * s;
    if (it->note) { kit_dev__clip(it->note, x, y, 16, w - x - pad, KIT_DEV__DIM); y += 22 * s; }
    if (it->file) {
        Model model = st->model;
        if (!model.meshCount) kit_dev__text(TextFormat("%s   (not found)", it->file), x, y, 16, KIT_DEV__TAGCOL[1]);
        else {
            int tris = 0, tex = 0;
            for (int k = 0; k < model.meshCount; k++) tris += model.meshes[k].triangleCount;
            for (int k = 0; k < model.materialCount; k++) tex += model.materials[k].maps[MATERIAL_MAP_DIFFUSE].texture.id != rlGetTextureIdDefault();   // the 1x1 white stand-in is not a texture
            kit_dev__text(TextFormat("%s    %d triangles    %d meshes    %d materials    %d textures    %d clips", it->file, tris, model.meshCount,
                                     model.materialCount, tex, kdv.anim_count[st->item]), x, y, 16, KIT_DEV__DIM);
        }
        y += 22 * s;
    }
    kit_dev__text(TextFormat("state %d%s", st->state, st->paused ? "    paused" : ""), x, y, 16, KIT_DEV__ACCENT);

    // keys: the game's own first, then the built-in ones
    DrawRectangle((int)list_w, (int)(h - foot), (int)(w - list_w), (int)foot, Fade(BLACK, 0.45f));
    char line[400] = "";
    for (int k = 0; k < m->key_count; k++) snprintf(line + strlen(line), sizeof line - strlen(line), "%s: %s    ", m->keys[k].keys, m->keys[k].does);
    snprintf(line + strlen(line), sizeof line - strlen(line), "Tab/[ ]: item    drag: turn    wheel: zoom    R: spin    Space: pause");
    kit_dev__clip(line, x, h - foot + 12 * s, 16, w - x - pad, KIT_DEV__DIM);

    if (kdv.shots && ++kdv.settle == 3) {   // one picture per item, then on to the next
        kit_dev__capture(kdv.shots, TextFormat("%s-%s", m->name, it->name));
        if (st->item + 1 >= m->count) kdv.done = true;
        else kit_dev__select(m, st->item + 1);
    }
    EndDrawing();
}

bool kit_dev_step(void) {
    if (kdv.current < 0 || kdv.done) return false;
    const KitDev__Mode *m = &kdv.modes[kdv.current];
    if (m->kind == KIT_DEV__CUSTOM) return false;
    if (!kdv.ready) {
        kdv.ready = true;
        SetExitKey(KEY_ESCAPE);
        if (m->kind == KIT_DEV__SOUNDS) kit_dev__sounds_load(m);
        else kit_dev__gallery_load(m);
    }
    if (m->kind == KIT_DEV__SOUNDS) kit_dev__sounds_step(m);
    else kit_dev__gallery_step(m);
    return !kdv.done;
}

bool kit_dev_run(void) {
    if (kdv.current < 0 || kdv.modes[kdv.current].kind == KIT_DEV__CUSTOM) return false;
    while (!WindowShouldClose() && kit_dev_step()) { }
    return true;
}

#endif // KIT_DEV_IMPLEMENTATION_DONE
#endif // KIT_DEV_IMPLEMENTATION
