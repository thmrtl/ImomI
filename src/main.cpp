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
    Vector2 pos;
    float speed;
    int hp;
    int hp_max;
    float invincibility_time;
    float invincibility_time_max;
    float cooldown_time;
    float cooldown_time_max;
};

struct Enemy {
    Vector2 pos;
    float speed;
    int hp;
    int hp_max;
    bool is_hit;
};

struct Bonus {
    Vector2 pos;
    float speed;
    int hp;
    bool is_hit;
};

struct Bullet {
    bool alive;
    Vector2 pos;
    float speed;
    int dir;
};

struct GameState {
    Screen current_screen;
    Screen next_screen;
    Player player;
    Vector2 input_dir;
    float level_progression;
    float level_length;
    float level_speed;
    bool is_paused;
    std::vector<Enemy> enemies;
    std::vector<Bonus> bonuses;
    std::vector<Bullet> bullets;
};

Vector2 GetInputDir();
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
        .pos=Vector2{200, 50},
        .speed=3.6f,
        .hp=3,
        .hp_max=3,
        .invincibility_time=0.0f,
        .invincibility_time_max=2.0f,
        .cooldown_time=0.4f, // Avoid early fire.
        .cooldown_time_max=0.2f,
    };
    game_state.input_dir = Vector2{0, 0};
    game_state.level_progression = -0.05f; // Delay level start.
    game_state.level_length = 10.0f;
    game_state.level_speed = 1.5f;
    game_state.is_paused = false;
    game_state.enemies = {
        Enemy{Vector2{400, 180}, -1.0, 1, 1},
        Enemy{Vector2{500, 300}, -2.0, 2, 2},
        Enemy{Vector2{600, 400}, -1.0, 1, 1},
    };
    game_state.bonuses = {
        Bonus{Vector2{400, 150}, 0.0, 1},
    };
    game_state.bullets.resize(50, Bullet{false, Vector2{}, 0});

    while (!WindowShouldClose()) {
        game_state.input_dir = GetInputDir();
        ShowCurrentScreen(game_state);
        SwitchToScreen(game_state);
    }

    CloseWindow();

    return 0;
}

Rectangle GetBoundingBox(float cx, float cy, float width, float height)
{
    float x = cx - width * 0.5f;
    float y = cy - height * 0.5f;
    return Rectangle{x, y, width, height};
}

Rectangle GetBoundingBox(Player const &player)
{
    return GetBoundingBox(player.pos.x, player.pos.y, 80, 60);
}

Rectangle GetBoundingBox(Enemy const &enemy)
{
    return GetBoundingBox(enemy.pos.x, enemy.pos.y, 30, 30);
}

Rectangle GetBoundingBox(Bonus const &bonus)
{
    return GetBoundingBox(bonus.pos.x, bonus.pos.y, 20, 20);
}

Rectangle GetBoundingBox(Bullet const &bullet)
{
    return GetBoundingBox(bullet.pos.x, bullet.pos.y, 10, 10);
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

void ShowCurrentScreen(GameState &game_state)
{
    switch (game_state.current_screen) {
    case Screen::Title:
        if (IsKeyPressed(KEY_SPACE) || (IsGamepadAvailable(0) && IsGamepadButtonPressed(0, GAMEPAD_BUTTON_MIDDLE_RIGHT))) {
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
        if (IsKeyDown(KEY_R) || (IsGamepadAvailable(0) && IsGamepadButtonDown(0, GAMEPAD_BUTTON_RIGHT_FACE_UP))) {
            game_state.next_screen = Screen::Title;
            return; // No draw for 1 frame... it's fine.
        }

        if (IsKeyPressed(KEY_ENTER) || (IsGamepadAvailable(0) && IsGamepadButtonPressed(0, GAMEPAD_BUTTON_MIDDLE_RIGHT))) {
            game_state.is_paused = !game_state.is_paused;
        }

        float elpasedTime = GetFrameTime();
        Player& player = game_state.player;
        Rectangle player_rect = GetBoundingBox(player);
        int screen_width = GetScreenWidth();
        int screen_height = GetScreenHeight();

        if (!game_state.is_paused) {
            player.invincibility_time -= elpasedTime;
            if (player.invincibility_time <= 0.0f)
                player.invincibility_time = 0.0f;
            player.cooldown_time -= elpasedTime;
            if (player.cooldown_time <= 0.0f)
                player.cooldown_time = 0.0f;
            
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
            static bool is_progression_paused = false;
            if (IsKeyPressed(KEY_I)) {
                is_progression_paused = !is_progression_paused;
            }

            if (!is_progression_paused && game_state.level_progression <= 1.0f) {
                game_state.level_progression += elpasedTime * game_state.level_speed / game_state.level_length;
                if (game_state.level_progression > 1.0f)
                    game_state.level_progression = 1.0f;
            }

            player.pos.x += game_state.input_dir.x * elpasedTime * player.speed * PIXEL_PER_UNIT;
            player.pos.y += game_state.input_dir.y * elpasedTime * player.speed * PIXEL_PER_UNIT;
            
            if ((IsKeyDown(KEY_SPACE) || (IsGamepadAvailable(0) && IsGamepadButtonDown(0, GAMEPAD_BUTTON_RIGHT_FACE_RIGHT))) && player.cooldown_time <= 0.0f) {
                player.cooldown_time = player.cooldown_time_max;
                CreateBullet(game_state.bullets, Vector2{player_rect.x + player_rect.width, player.pos.y}, 6.5, 1);
            }

            for (int i = 0; i < game_state.bullets.size(); i++) {
                Bullet& bullet = game_state.bullets[i];
                Rectangle bullet_rect = GetBoundingBox(bullet);
                if (bullet.alive) {
                    bullet.pos.x += bullet.dir * elpasedTime * bullet.speed * PIXEL_PER_UNIT;
                    if (bullet_rect.x >= screen_width || bullet_rect.x + bullet_rect.width <= 0) {
                        bullet.alive = false;
                    }
                }
            }

            for (int i = 0; i < game_state.enemies.size(); i++) {
                Enemy& enemy = game_state.enemies[i];
                enemy.is_hit = false;
                enemy.pos.x += elpasedTime * enemy.speed * PIXEL_PER_UNIT;
                Rectangle enemy_rect = GetBoundingBox(enemy);
                if (enemy_rect.x + enemy_rect.width <= 0.0f) {
                    enemy.pos.x = screen_width + enemy_rect.width * 0.5f;
                }
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
                DrawText(std::to_string(i - x_start).c_str(), x, screen_height / 4 - 20, 20, WHITE);
                DrawLine(x, screen_height / 4, x, 3 * screen_height / 4, YELLOW);
            }
            DrawText(dyn_format("{:.3f}", game_state.input_dir.x).c_str(), 0, 40, 20, WHITE);
            DrawText(dyn_format("{:.3f}", game_state.input_dir.y).c_str(), 0, 60, 20, WHITE);
            for (int i = 0; i < game_state.enemies.size(); i++) {
                Enemy& enemy = game_state.enemies[i];
                Rectangle enemy_rect = GetBoundingBox(enemy);
                DrawRectangle(enemy_rect, enemy.is_hit ? BLUE : RED);
                DrawText(std::to_string(i + 1).c_str(), (int)enemy_rect.x, (int)enemy_rect.y, 20, WHITE);
                DrawText(std::to_string(enemy.hp).c_str(), (int)enemy_rect.x, (int)enemy_rect.y + 20, 20, WHITE);
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
                if (bullet.alive) {
                    Rectangle bullet_rect = GetBoundingBox(bullet);
                    DrawRectangle(bullet_rect, PINK);
                }
            }
            DrawRectangle(player_rect, player.invincibility_time > 0.0f ? ORANGE : PURPLE);
            DrawText(std::to_string(player.hp).c_str(), (int)player_rect.x, (int)player_rect.y, 20, WHITE);
            DrawText(dyn_format("{:.2f}", player.invincibility_time).c_str(), (int)player_rect.x, (int)player_rect.y + 20, 20, WHITE);
            DrawText(dyn_format("{:.2f}", player.cooldown_time).c_str(), (int)player_rect.x, (int)player_rect.y + 40, 20, WHITE);
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
