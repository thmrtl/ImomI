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

struct Player {
    Vector2 pos;                    // In world unit
    float speed;                    // In world unit
    int hp;
    int hp_max;
    float invincibility_time;
    float invincibility_time_max;
    float cooldown_time;
    float cooldown_time_max;
};

struct Enemy {
    Vector2 spawn_pos;              // In world unit
    Vector2 pos;                    // In world unit
    float speed;                    // In world unit
    int hp;
    int hp_max;
    bool is_hit;
};

struct Bonus {
    Vector2 spawn_pos;              // In world unit
    Vector2 pos;                    // In pixel
    float speed;                    // In world unit
    int hp;
    bool is_hit;
};

struct Bullet {
    bool alive;
    Vector2 pos;                    // In world unit
    float speed;                    // In world unit
    int dir;
};

struct Inputs {
    Vector2 dir;
    bool pause;
    bool fire;
    bool reset;
};

struct GameState {
    Inputs inputs;
    Screen current_screen;
    Screen next_screen;
    Player player;
    float level_progression;
    float level_length;
    float level_speed;
    bool is_paused;
    std::vector<Enemy> enemies;
    std::vector<Bonus> bonuses;
    std::vector<Bullet> bullets;
};

Inputs GetInputs();
void ShowCurrentScreen(GameState& game_state);
void SwitchToScreen(GameState& game_state);
void CreateBullet(std::vector<Bullet>& bullets, Vector2 pos, float speed, int dir);

int main(void) {
    InitWindow(800, 450, "ImomI");
    SetTargetFPS(60);

    GameState game_state;
    game_state.current_screen = Screen::Title;
    game_state.next_screen = Screen::Title;
    game_state.player = Player{
        .pos=Vector2{-3.0f, 0.0f},
        .speed=3.6f,
        .hp=3,
        .hp_max=3,
        .invincibility_time=0.0f,
        .invincibility_time_max=2.0f,
        .cooldown_time=0.4f, // Avoid early fire.
        .cooldown_time_max=0.2f,
    };
    game_state.level_progression = -0.05f; // Delay level start.
    game_state.level_length = 10.0f;
    game_state.level_speed = 1.5f;
    game_state.is_paused = false;
    game_state.enemies = {
        Enemy{Vector2{1, -1}, Vector2{1, -1}, -1.0, 1, 1},
        Enemy{Vector2{2, 0}, Vector2{2, 0}, -1.0, 1, 1},
        Enemy{Vector2{4, 2}, Vector2{4, 2}, -1.0, 1, 1},
    };

    game_state.bonuses = {
        Bonus{Vector2{400, 150}, 0.0, 1},
    };
    game_state.bullets.resize(20, Bullet{false, Vector2{}, 0});

    while (!WindowShouldClose()) {
        game_state.inputs = GetInputs();
        ShowCurrentScreen(game_state);
        SwitchToScreen(game_state);
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

Rectangle GetBoundingBox(Player const &player)
{
    return GetBoundingBox(player.pos.x, player.pos.y, 0.8f, 0.6f);
}

Rectangle GetBoundingBox(Enemy const &enemy)
{
    return GetBoundingBox(enemy.pos.x, enemy.pos.y, 0.3f, 0.3f);
}

Rectangle GetBoundingBox(Bonus const &bonus)
{
    return GetBoundingBox(bonus.pos.x, bonus.pos.y, 20, 20);
}

Rectangle GetBoundingBox(Bullet const &bullet)
{
    return GetBoundingBox(bullet.pos.x, bullet.pos.y, 0.1f, 0.1f);
}

void DrawRectangle(Rectangle rect, Color color)
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

void ShowCurrentScreen(GameState &game_state)
{
    switch (game_state.current_screen) {
    case Screen::Title:
        if (game_state.inputs.fire) {
            game_state.next_screen = Screen::Game;
        }
        BeginDrawing();
            ClearBackground(BEIGE);
            DrawText("ImomI", 190, 170, 42, LIGHTGRAY);
            DrawText("Press START to continune", 190, 230, 25, LIGHTGRAY);
        EndDrawing();
        break;
    case Screen::Game:
    {
        if (game_state.inputs.reset) {
            game_state.next_screen = Screen::Title;
            return; // No draw for 1 frame... it's fine.
        }

        if (game_state.inputs.pause) {
            game_state.is_paused = !game_state.is_paused;
        }

        float elpased_time = GetFrameTime();
        Player& player = game_state.player;
        Rectangle player_rect = GetBoundingBox(player);
        int screen_width = GetScreenWidth();
        int screen_height = GetScreenHeight();
        Rectangle screen(0, 0, (float)screen_width, (float)screen_height);
        Rectangle clip_rect = ScreenToWorld(screen, screen);
        int active_enemies = 0;

        if (!game_state.is_paused) {
            player.invincibility_time -= elpased_time;
            if (player.invincibility_time <= 0.0f)
                player.invincibility_time = 0.0f;
            player.cooldown_time -= elpased_time;
            if (player.cooldown_time <= 0.0f)
                player.cooldown_time = 0.0f;
            
            // Progression tests
            if (IsKeyPressed(KEY_J)) {
                game_state.level_progression += 0.05f;
            }
            if (IsKeyPressed(KEY_L)) {
                game_state.level_progression -= 0.05f;
            }
            if (IsKeyPressed(KEY_K)) {
                if (game_state.level_progression == 0.0f)
                    game_state.level_progression = 1.0f;
                else
                    game_state.level_progression = 0.0f;
            }
            static bool is_progression_paused = true;
            if (IsKeyPressed(KEY_I)) {
                is_progression_paused = !is_progression_paused;
            }
            //

            if (!is_progression_paused && game_state.level_progression <= 1.0f) {
                game_state.level_progression += elpased_time * game_state.level_speed / game_state.level_length;
                if (game_state.level_progression > 1.0f)
                    game_state.level_progression = 1.0f;
            }

            player.pos.x += game_state.inputs.dir.x * elpased_time * player.speed;
            player.pos.y += game_state.inputs.dir.y * elpased_time * player.speed;
            
            if (game_state.inputs.fire && player.cooldown_time <= 0.0f) {
                player.cooldown_time = player.cooldown_time_max;
                CreateBullet(game_state.bullets, Vector2{player_rect.x + player_rect.width, player.pos.y}, 10, 1);
            }

            for (int i = 0; i < game_state.bullets.size(); i++) {
                Bullet& bullet = game_state.bullets[i];
                Rectangle bullet_rect = GetBoundingBox(bullet);
                if (bullet.alive) {
                    bullet.pos.x += bullet.dir * elpased_time * bullet.speed;
                    if (bullet_rect.x >= clip_rect.x + clip_rect.width || bullet_rect.x + bullet_rect.width <= clip_rect.x) {
                    //if (!CheckCollisionRecs(bullet_rect, clip_rect)) {
                        bullet.alive = false;
                    }
                }
            }

            float spawn_x_line = game_state.level_progression * game_state.level_length;
            for (int i = 0; i < game_state.enemies.size(); i++) {
                Enemy& enemy = game_state.enemies[i];
                enemy.is_hit = false;

                Rectangle enemy_rect = GetBoundingBox(enemy);
                enemy.pos.x = enemy.spawn_pos.x;
                // if (enemy.spawn_pos.x - enemy_rect.width / 2 >= spawn_x_line)
                //     continue;
                active_enemies++;

                // TODO: index position on level progression instead of frametime.

                //enemy.pos.x += elpased_time * enemy.speed * PIXEL_PER_UNIT;
                
                // if (enemy_rect.x + enemy_rect.width <= 0.0f) {
                //     enemy.pos.x = screen_width + enemy_rect.width * 0.5f;
                // }
                if (CheckCollisionRecs(player_rect, enemy_rect)) {
                    if (player.invincibility_time <= 0.0f) {
                        player.invincibility_time = player.invincibility_time_max;
                        player.hp--;
                    }
                    enemy.hp = 0;
                    enemy.is_hit = true;
                }
                if (enemy.hp <= 0)
                    continue;
                for (int j = 0; j < game_state.bullets.size(); j++) {
                    Bullet& bullet = game_state.bullets[j];
                    if (bullet.alive) {
                        Rectangle bullet_rect = GetBoundingBox(bullet);
                        if (CheckCollisionRecs(bullet_rect, enemy_rect)) {
                            bullet.alive = false;
                            enemy.hp--;
                            if (enemy.hp <= 0)
                                break;
                        }
                    }
                }
            }

            for (int i = 0; i < game_state.bonuses.size(); i++) {
                Bonus& bonus = game_state.bonuses[i];
                Rectangle bonus_rect = GetBoundingBox(bonus);
                if (CheckCollisionRecs(player_rect, bonus_rect)) {
                    player.hp++;
                    if (player.hp > player.hp_max)
                        player.hp = player.hp_max;
                    bonus.hp = 0;
                    bonus.is_hit = true;
                }
                else
                    bonus.is_hit = false;
            }
        }

        BeginDrawing();
            ClearBackground(BLACK);
            float tick_pos = game_state.level_progression * game_state.level_length * PIXEL_PER_UNIT;
            screen.x = -tick_pos;
            int spacing = (int)(1 * PIXEL_PER_UNIT);
            int offset = (int)tick_pos % screen_width;
            DrawText(dyn_format("{:.3f}", game_state.level_progression).c_str(), 0, 0, 20, WHITE);
            DrawText(dyn_format("{:.2f}", tick_pos).c_str(), 0, 20, 20, WHITE);
            int x_start = screen_width / 2 - (int)tick_pos;
            int x_end = x_start + (int)(game_state.level_length * PIXEL_PER_UNIT);
            DrawLine(x_start, 0, x_start, screen_height, SKYBLUE);
            DrawLine(x_end, 0, x_end, screen_height, BLUE);
            for(int i = 0; i < screen_width; i += spacing) {
                int x = (i - (int)tick_pos) % screen_width;
                if (x < 0)
                    x += screen_width;
                DrawText(std::to_string(x - x_start).c_str(), x, screen_height / 4 - 20, 20, WHITE);
                DrawLine(x, screen_height / 4, x, 3 * screen_height / 4, YELLOW);
            }
            DrawText(dyn_format("{:.3f}", game_state.inputs.dir.x).c_str(), 0, 40, 20, WHITE);
            DrawText(dyn_format("{:.3f}", game_state.inputs.dir.y).c_str(), 0, 60, 20, WHITE);
            DrawText(dyn_format("Spawn: {}", game_state.level_progression * game_state.level_length).c_str(), 0, screen_height - 60, 20, WHITE);
            DrawText(dyn_format("Inactive: {}", game_state.enemies.size() - active_enemies).c_str(), 0, screen_height - 40, 20, WHITE);
            DrawText(dyn_format("Active: {}", active_enemies).c_str(), 0, screen_height - 20, 20, WHITE);
            for (int i = 0; i < game_state.enemies.size(); i++) {
                Enemy& enemy = game_state.enemies[i];
                Rectangle draw_rect = WorldToScreen(GetBoundingBox(enemy), screen);
                DrawText(std::to_string(enemy.hp).c_str(), (int)draw_rect.x, (int)draw_rect.y, 20, WHITE);
                DrawText(dyn_format("{:.2f}", draw_rect.x).c_str(), 0, 150 + 20 * (2 * i + 1), 20, WHITE);
                if (draw_rect.x + draw_rect.width < 0 || draw_rect.x > screen.width)
                    continue;
                DrawRectangle(draw_rect, enemy.is_hit ? BLUE : RED);
                DrawText(dyn_format("{:.2f}", enemy.pos.x).c_str(), 0, 150 + 20 * (2 * i), 20, WHITE);
            }
            for (int i = 0; i < game_state.bonuses.size(); i++) {
                Bonus& bonus = game_state.bonuses[i];
                Rectangle bonus_rect = GetBoundingBox(bonus);
                DrawRectangle(bonus_rect, bonus.is_hit ? BLUE : GREEN);
                DrawText(std::to_string(i + 1).c_str(), (int)bonus_rect.x, (int)bonus_rect.y, 20, WHITE);
                DrawText(std::to_string(bonus.hp).c_str(), (int)bonus_rect.x, (int)bonus_rect.y + 20, 20, WHITE);
            }
            for (int i = 0; i < game_state.bullets.size(); i++) {
                Bullet& bullet = game_state.bullets[i];
                if (i == 0) {
                    DrawText(dyn_format("{:.2f}", bullet.pos.x).c_str(), 160, 0, 20, WHITE);
                    DrawText(dyn_format("{:.2f}", bullet.pos.y).c_str(), 160, 20, 20, WHITE);
                }
                if (bullet.alive) {
                    Rectangle bullet_rect = WorldToScreen(GetBoundingBox(bullet), screen);
                    DrawRectangle(bullet_rect, PINK);
                }
            }
            player_rect = WorldToScreen(GetBoundingBox(player), screen);
            DrawRectangle(player_rect, player.invincibility_time > 0.0f ? ORANGE : PURPLE);
            DrawText(std::to_string(player.hp).c_str(), (int)player_rect.x, (int)player_rect.y, 20, WHITE);
            DrawText(dyn_format("{:.2f}", player.invincibility_time).c_str(), (int)player_rect.x, (int)player_rect.y + 20, 20, WHITE);
            DrawText(dyn_format("{:.2f}", player.cooldown_time).c_str(), (int)player_rect.x, (int)player_rect.y + 40, 20, WHITE);
            DrawText(dyn_format("{:.2f}", player.pos.x).c_str(), 100, 0, 20, WHITE);
            DrawText(dyn_format("{:.2f}", player.pos.y).c_str(), 100, 20, 20, WHITE);
            if (game_state.is_paused) {
                DrawRectangle(0, 0, screen_width, screen_height, Color{0, 0, 0, 125});
                int width = MeasureText("Pause", 24);
                DrawText("Pause", (screen_width - width) / 2, (screen_height - 12) / 2, 25, WHITE);
            }
        EndDrawing();
        break;
    }
    case Screen::Credits:
        BeginDrawing();
            ClearBackground(DARKBLUE);
            DrawText("Credits", 190, 200, 20, LIGHTGRAY);
        EndDrawing();
        break;
    }
}

void SwitchToScreen(GameState& game_state)
{
    if (game_state.current_screen == Screen::Title && game_state.next_screen == Screen::Game) {
        game_state.level_progression = 0.0f;
        game_state.player.hp = game_state.player.hp_max;
        for (int i = 0; i < game_state.enemies.size(); i++) {
            Enemy& enemy = game_state.enemies[i];
            enemy.is_hit = false;
            enemy.hp = enemy.hp_max;
        }
        for (int i = 0; i < game_state.bonuses.size(); i++) {
            Bonus& bonus = game_state.bonuses[i];
            bonus.is_hit = false;
        }
        for (int i = 0; i < game_state.bullets.size(); i++) {
            Bullet& bullet = game_state.bullets[i];
            bullet.alive = false;
        }
    }
    game_state.current_screen = game_state.next_screen;
}

void CreateBullet(std::vector<Bullet> &bullets, Vector2 pos, float speed, int dir)
{
    for (int i = 0; i < bullets.size(); i++) {
        Bullet& bullet = bullets[i];
        if (!bullet.alive) {
            bullet.pos = pos;
            bullet.speed = speed;
            bullet.alive = true;
            bullet.dir = dir;
            break;
        }
    }
}
