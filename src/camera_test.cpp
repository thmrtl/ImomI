#include "raylib.h"
#include "raymath.h"
#include <print>
#include <string>
#include <vector>

#define PIXEL_PER_UNIT 100

enum class Screen {
    Title,
    Game,
    Credits,
};

struct Entity {
    bool alive;
    bool can_move;
    Vector2 pos;
    Vector2 velocity;
};

struct Inputs {
    Vector2 dir;
    bool pause;
    bool fire;
    bool reset;
};

Inputs GetInputs();
void DrawRectangle(Rectangle rect, Color color);

template<typename... Args>
std::string dyn_format(std::string_view rt_fmt_str, Args&&... args);

int main(void) {
    InitWindow(800, 450, "ImomI");
    SetTargetFPS(60);

    bool is_paused = false;
    Camera2D camera;
    camera.offset = { 0.0f, 0.0f };
    camera.rotation = 0.0f;
    camera.target = { 0.0f, 0.0f };
    camera.zoom = 1.0f;

    std::vector<Entity> entities = {
        Entity{
            .alive = false,
            .can_move = true,
            .pos = { 250.0f, 300.0f },
        },
        Entity{
            .alive = false,
            .can_move = true,
            .pos = { 200.0f, 350.0f },
        },
        Entity{
            .alive = false,
            .can_move = true,
            .pos = { 300.0f, 250.0f },
        },
    };

    float camera_offset_offset = 0.0f;
    int camera_offset_dir = 1;
    float camera_target_offset = 0.0f;
    int camera_target_dir = 1;

    while (!WindowShouldClose()) {
        Inputs inputs = GetInputs();
        
        if (inputs.pause) {
            is_paused = !is_paused;
        }

        float frame_time = GetFrameTime();
        float screen_width = (float)GetScreenWidth();
        float screen_height = (float)GetScreenHeight();

        camera_offset_offset += camera_offset_dir * frame_time * 0.0f;
        camera_target_offset += camera_target_dir * frame_time * 20.0f;
        if (camera_offset_offset < -50.0f || camera_offset_offset >= 50.0f) {
            camera_offset_dir *= -1;
            Clamp(camera_offset_offset, -50.0f, 50.0f);
        }
        if (camera_target_offset < -50.0f || camera_target_offset >= 50.0f) {
            camera_target_dir *= -1;
            Clamp(camera_target_offset, -50.0f, 50.0f);
        }

        camera.offset.x = camera_offset_offset;
        camera.target.x = camera_target_offset + 200.0f;
        
        Vector2 screen_center = { screen_width * 0.5f, screen_height * 0.5f};

        BeginDrawing();
            ClearBackground(BLACK);
            BeginMode2D(camera);
                for (int i = 0; i < entities.size(); i++) {
                    Entity& entity = entities[i];
                    DrawRectangle(Rectangle(entity.pos.x, entity.pos.y, 10, 10), PINK);
                }
            EndMode2D();
            DrawRectangle(Rectangle(50, 50, screen_width - 100, screen_height - 100), RED);
            DrawText(dyn_format("Offset: {:.2f},{:.2f}", camera.offset.x, camera.offset.y).c_str(), 0, 0, 20, WHITE);
            DrawText(dyn_format("Target: {:.2f},{:.2f}", camera.target.x, camera.target.y).c_str(), 0, 20, 20, WHITE);
            DrawText(dyn_format("Rotation: {:.2f}", camera.rotation).c_str(), 0, 40, 20, WHITE);
            DrawText(dyn_format("Zoom: {:.2f}", camera.zoom).c_str(), 0, 60, 20, WHITE);
            // camera.target = screen_center;
            // camera.offset = screen_center;
            // BeginMode2D(camera);
            //     DrawRectangle(Rectangle(screen_center.x - 50, screen_center.y - 50, 100, 100), RED);
            // EndMode2D();
        EndDrawing();
    }

    CloseWindow();

    return 0;
}

Rectangle WorldToScreen(Rectangle rect, Rectangle screen)
{
    float x = rect.x * PIXEL_PER_UNIT + screen.x + screen.width / 2;
    float y = rect.y * PIXEL_PER_UNIT + screen.y + screen.height / 2;
    float width = rect.width * PIXEL_PER_UNIT;
    float height = rect.height * PIXEL_PER_UNIT;
    return Rectangle(x, y, width, height);
}

Rectangle ScreenToWorld(Rectangle rect, Rectangle screen)
{
    float x = (rect.x - screen.x - screen.width / 2) / PIXEL_PER_UNIT;
    float y = (rect.y - screen.y - screen.height / 2) / PIXEL_PER_UNIT;
    float width = rect.width / PIXEL_PER_UNIT;
    float height = rect.height / PIXEL_PER_UNIT;
    return Rectangle(x, y, width, height);
}

Rectangle GetBoundingBox(float cx, float cy, float width, float height)
{
    float x = cx - width * 0.5f;
    float y = cy - height * 0.5f;
    return Rectangle{x, y, width, height};
}

void DrawRectangle(Rectangle rect, Color color)
{
    DrawRectangleLines((int)rect.x, (int)rect.y, (int)rect.width, (int)rect.height, color);
}

void DrawEntity(Entity const& entity, Color color)
{
    DrawRectangleLines((int)rect.x, (int)rect.y, (int)rect.width, (int)rect.height, color);
}

template<typename... Args>
std::string dyn_format(std::string_view rt_fmt_str, Args&&... args)
{
    return std::vformat(rt_fmt_str, std::make_format_args(args...));
}

// TODO: Verify that IsGamepadAvailable true doesn't prevent keyboard inputs.
Vector2 GetInputDir()
{
    Vector2 dir{0, 0};
    if (IsGamepadAvailable(0)) {
        dir.x = GetGamepadAxisMovement(0, GAMEPAD_AXIS_LEFT_X);
        if (abs(dir.x) < 0.1f)
            dir.x = 0.0f;
        dir.y = GetGamepadAxisMovement(0, GAMEPAD_AXIS_LEFT_Y);
        if (abs(dir.y) < 0.1f)
            dir.y = 0.0f;
        return dir;
    }
    if (IsKeyDown(KEY_W))
        dir.y = -1;
    if (IsKeyDown(KEY_S))
        dir.y = 1;
    if (IsKeyDown(KEY_A))
        dir.x = -1;
    if (IsKeyDown(KEY_D))
        dir.x = 1;
    dir = Vector2Normalize(Vector2{dir.x, dir.y});
    return dir;
}

Inputs GetInputs()
{
    Inputs inputs;
    inputs.dir = GetInputDir();
    inputs.pause = IsKeyPressed(KEY_P);
    inputs.fire = IsKeyDown(KEY_SPACE);
    inputs.reset = IsKeyPressed(KEY_R);
    if (IsGamepadAvailable(0)) {
        inputs.pause |= IsGamepadButtonPressed(0, GAMEPAD_BUTTON_MIDDLE_RIGHT);
        inputs.fire |= IsGamepadButtonDown(0, GAMEPAD_BUTTON_RIGHT_FACE_RIGHT);
        inputs.reset |= IsGamepadButtonPressed(0, GAMEPAD_BUTTON_RIGHT_FACE_UP);
    }
    return inputs;
}