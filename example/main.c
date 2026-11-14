// A stand-in "game" with three dev modes registered: the sound board, two galleries (one
// drawn by code, one loaded from files) and a sandbox of its own.
//
//   ./dev.sh                 list them          ./dev.sh sounds
//   ./dev.sh props lamp      start on one       ./dev.sh models --shots out/
#define KIT_DEV_IMPLEMENTATION
#include "kit_dev.h"
#include "rlgl.h"
#include <math.h>
#ifdef __EMSCRIPTEN__
#include <emscripten/emscripten.h>
#endif

// ---- the sound table: one row per MOMENT in the game, whether or not it has a sound yet ----
static const KitDevSound SOUNDS[] = {
    { "player", "jump", "leaving the ground", "assets/jump.wav", KIT_DEV_OK, NULL },
    { "player", "land", "touching down after a fall", "assets/land.wav", KIT_DEV_OK, NULL },
    { "player", "hurt", "taking damage", "assets/land.wav", KIT_DEV_SHARED, "Borrowing the landing thud. Needs its own voice: a short grunt." },
    { "player", "footstep", "every other stride on stone", NULL, KIT_DEV_MISSING, "Walking is silent. Two or three variations to avoid a machine-gun loop." },
    { "pickups", "coin", "collecting a coin", "assets/coin.wav", KIT_DEV_OK, NULL },
    { "pickups", "gem", "collecting a gem", "assets/coin.wav", KIT_DEV_SHARED, "Same as the coin, so a gem does not feel like more." },
    { "pickups", "key", "collecting a key", "assets/key.wav", KIT_DEV_OK, "Listed as done, but the file is not in assets/: the board marks it MISSING by itself." },
    { "world", "door open", "a door finishing its swing", "assets/door_open.wav", KIT_DEV_IMPROVE, "Too long: it is still playing when the player is through the door." },
    { "world", "door locked", "pushing a locked door", NULL, KIT_DEV_MISSING, NULL },
    { "world", "alarm", "the beacon is lit", "assets/alarm.wav", KIT_DEV_OK, NULL },
    { "world", "wind", "outdoors, looping", "assets/wind.wav", KIT_DEV_IMPROVE, "The loop point clicks." },
    { "interface", "click", "any button", "assets/click.wav", KIT_DEV_OK, NULL },
    { "interface", "back", "leaving a menu", NULL, KIT_DEV_MISSING, NULL },
};
#define COUNT(a) ((int)(sizeof(a) / sizeof(a)[0]))

// ---- a gallery drawn by the game's own code, with states ---------------------------------
static const KitDevItem PROPS[] = {
    { "crate", NULL, "Breakable. State 2 is the frame after the first hit." },
    { "barrel", NULL, "Explodes. State 3 is armed." },
    { "lamp", NULL, "State 3 lights it." },
    { "pine", NULL, "Three stacked cones." },
    { "beacon", NULL, "The level goal. State 3 is lit; the alarm sound belongs to it." },
};
static const KitDevKey PROP_KEYS[] = { { "1", "as placed" }, { "2", "damaged" }, { "3", "active / lit" } };

static Model m_cube, m_cyl, m_cone, m_ball;

static void part(Model m, Vector3 at, Vector3 size, Color c) { DrawModelEx(m, at, (Vector3){ 0, 1, 0 }, 0, size, c); }

static void draw_prop(const KitDevStage *st) {
    if (!m_cube.meshCount) {  // built once, on first use, with the gallery's lit shader
        m_cube = LoadModelFromMesh(GenMeshCube(1, 1, 1));
        m_cyl = LoadModelFromMesh(GenMeshCylinder(1, 1, 24));
        m_cone = LoadModelFromMesh(GenMeshCone(1, 1, 24));
        m_ball = LoadModelFromMesh(GenMeshSphere(1, 16, 24));
        m_cube.materials[0].shader = m_cyl.materials[0].shader = m_cone.materials[0].shader = m_ball.materials[0].shader = kit_dev_shader();
    }
    bool hurt = st->state == 2, on = st->state == 3;
    float pulse = 0.5f + 0.5f * sinf(st->time * 6);
    const Color WOOD = { 176, 122, 70, 255 }, DARKWOOD = { 110, 74, 44, 255 }, STEEL = { 120, 130, 160, 255 }, RED_ = { 240, 84, 104, 255 },
                GLOW = { 255, 205, 80, 255 }, GREEN_ = { 61, 160, 110, 255 }, CYAN = { 120, 225, 235, 255 };
    switch (st->item) {
    case 0:  // crate
        part(m_cube, (Vector3){ 0, 0.75f, 0 }, (Vector3){ 1.5f, 1.5f, 1.5f }, hurt ? DARKWOOD : WOOD);
        for (int i = 0; i < 2; i++) part(m_cube, (Vector3){ 0, 0.25f + i * 1.0f, 0 }, (Vector3){ 1.58f, 0.16f, 1.58f }, DARKWOOD);
        if (hurt) part(m_cube, (Vector3){ 0.5f, 1.2f, 0.72f }, (Vector3){ 0.5f, 0.7f, 0.12f }, BLACK);
        break;
    case 1:  // barrel
        part(m_cyl, (Vector3){ 0, 0, 0 }, (Vector3){ 0.7f, 1.8f, 0.7f }, hurt ? (Color){ 150, 60, 70, 255 } : RED_);
        for (int i = 0; i < 3; i++) part(m_cyl, (Vector3){ 0, 0.25f + i * 0.62f, 0 }, (Vector3){ 0.74f, 0.1f, 0.74f }, STEEL);
        if (on) part(m_ball, (Vector3){ 0, 1.95f, 0 }, (Vector3){ 0.14f, 0.14f, 0.14f }, ColorLerp(RED_, GLOW, pulse));
        break;
    case 2:  // lamp
        part(m_cyl, (Vector3){ 0, 0, 0 }, (Vector3){ 0.3f, 0.12f, 0.3f }, STEEL);
        part(m_cyl, (Vector3){ 0, 0, 0 }, (Vector3){ 0.07f, 2.3f, 0.07f }, STEEL);
        part(m_ball, (Vector3){ 0, 2.45f, 0 }, (Vector3){ 0.28f, 0.28f, 0.28f }, on ? GLOW : (hurt ? DARKGRAY : (Color){ 200, 205, 230, 255 }));
        if (on) DrawSphere((Vector3){ 0, 2.45f, 0 }, 0.5f + 0.05f * pulse, Fade(GLOW, 0.18f));
        break;
    case 3:  // pine
        part(m_cyl, (Vector3){ 0, 0, 0 }, (Vector3){ 0.16f, 0.7f, 0.16f }, DARKWOOD);
        for (int i = 0; i < 3; i++) part(m_cone, (Vector3){ 0, 0.5f + i * 0.62f, 0 }, (Vector3){ 0.95f - i * 0.22f, 1.0f, 0.95f - i * 0.22f }, hurt ? (Color){ 120, 110, 70, 255 } : GREEN_);
        break;
    default:  // beacon
        part(m_cyl, (Vector3){ 0, 0, 0 }, (Vector3){ 0.9f, 0.3f, 0.9f }, STEEL);
        part(m_cyl, (Vector3){ 0, 0.3f, 0 }, (Vector3){ 0.5f, 1.5f, 0.5f }, hurt ? DARKGRAY : (Color){ 60, 66, 100, 255 });
        part(m_ball, (Vector3){ 0, 2.15f, 0 }, (Vector3){ 0.42f, 0.42f, 0.42f }, on ? ColorLerp(CYAN, WHITE, pulse) : (Color){ 70, 110, 120, 255 });
        if (on) DrawSphere((Vector3){ 0, 2.15f, 0 }, 0.7f + 0.15f * pulse, Fade(CYAN, 0.16f));
        break;
    }
}

// ---- a gallery loaded from files: no draw function needed --------------------------------
static const KitDevItem MODELS[] = {
    { "rocket", "assets/rocket.obj", "The player's ship." },
    { "lighthouse", "assets/lighthouse.obj", "Level 2 landmark." },
    { "vase", "assets/vase.obj", "Breakable decoration." },
    { "anchor", "assets/anchor.obj", "On the list, not modelled yet." },
};

// ---- a mode of the game's own: listed and documented by the kit, run by the game ---------
static const KitDevKey ORBIT_KEYS[] = { { "left / right", "faster / slower" }, { "Esc", "quit" } };

static float t, speed = 0.4f;
static bool orbit;

static void game_step(void) {
    if (kit_dev_step()) return;   // a built-in dev mode drew this frame
    if (IsKeyDown(KEY_RIGHT)) speed += GetFrameTime();
    if (IsKeyDown(KEY_LEFT)) speed -= GetFrameTime();
    t += GetFrameTime() * speed;
    Camera3D cam = { { sinf(t) * 9, 4.5f, cosf(t) * 9 }, { 0, 1, 0 }, { 0, 1, 0 }, 40, CAMERA_PERSPECTIVE };
    BeginDrawing();
    ClearBackground(BLACK);
    DrawRectangleGradientV(0, 0, GetScreenWidth(), GetScreenHeight(), (Color){ 30, 36, 60, 255 }, (Color){ 16, 18, 30, 255 });
    BeginMode3D(cam);
    DrawGrid(20, 0.5f);
    for (int i = 0; i < COUNT(PROPS); i++) {
        rlPushMatrix();
        rlTranslatef((i - 2) * 2.2f, 0, 0);
        draw_prop(&(KitDevStage){ .item = i, .state = 3, .time = t * 3 });
        rlPopMatrix();
    }
    EndMode3D();
    if (!orbit) {
        DrawText("This is the game.", 24, 22, 30, RAYWHITE);
        DrawText("Its dev modes: add ?--dev&sounds , ?--dev&props or ?--dev&models to the address,", 24, 62, 20, (Color){ 140, 152, 190, 255 });
        DrawText("or run ./dev.sh from a checkout.", 24, 86, 20, (Color){ 140, 152, 190, 255 });
    }
    EndDrawing();
}

int main(int argc, char **argv) {
    kit_dev_sounds(SOUNDS, COUNT(SOUNDS));
    kit_dev_gallery("props", "every prop alone on a stage, in each of its states", PROPS, COUNT(PROPS), draw_prop, PROP_KEYS, COUNT(PROP_KEYS));
    kit_dev_gallery("models", "every model file: triangles, materials, textures, clips", MODELS, COUNT(MODELS), NULL, NULL, 0);
    kit_dev_custom("orbit", "the scene with no text over it, for looking at the lighting", ORBIT_KEYS, COUNT(ORBIT_KEYS));
    kit_dev_init(argc, argv);
    orbit = kit_dev_is("orbit");

    SetConfigFlags(FLAG_WINDOW_RESIZABLE | FLAG_VSYNC_HINT | FLAG_MSAA_4X_HINT | FLAG_WINDOW_HIGHDPI);
    InitWindow(1280, 720, "kit_dev");
    InitAudioDevice();
#ifdef EXAMPLE_DIR
    ChangeDirectory(EXAMPLE_DIR);   // so "assets/..." is found from a checkout
#endif
#ifdef __EMSCRIPTEN__
    EM_ASM({ window.dispatchEvent(new Event('resize')); });
    emscripten_set_main_loop(game_step, 0, 1);
#else
    SetTargetFPS(60);
    while (!WindowShouldClose()) {
        if (kit_dev_active() && !orbit && !kit_dev_step()) break;   // a built-in mode finished (e.g. --shots)
        if (!kit_dev_active() || orbit) game_step();
    }
    CloseAudioDevice();
    CloseWindow();
#endif
    return 0;
}
