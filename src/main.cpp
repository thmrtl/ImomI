#include "midi.h"
#include "raylib.h"
#include "raymath.h"
#include <filesystem>
#include <fstream>
#include <print>
#include <string>
#include <vector>

#define PIXEL_PER_UNIT 100
#define MAX_LEVEL_FILE_SIZE 100 * 1024 * 1024

#define WARMUP_TIME_MAX 3.1f
#define INVINCIBILITY_TIME_MAX 1.5f
#define ENEMY_SHIELD_TIME_MAX 1.0f
#define MULTIPLICATOR_MIN 1.0f
#define TAIL_TIME_DEF 0.1f

#define ENEMY_SHIELD 2

struct Entity {
    bool alive;
    bool can_move;
    Vector2 pos;
    Vector2 velocity;
    int type;
    int hp;
    int hp_max;
    float last_hit_time;
};

struct Inputs {
    Vector2 dir;
    bool pause;
    bool fire;
    bool reset;
    float pan;
    bool stop;
    bool debug_overlay;
};

struct Level {
    float length;
    std::vector<Entity> enemies;
};

Inputs GetInputs();
Rectangle GetBoundingBox(float cx, float cy, float width, float height);
void DrawRectangle(Rectangle rect, Color color);
void DrawEntity(Entity const& entity, Vector2 size, Color color);
void CreateBullet(std::vector<Entity>& bullets, Vector2 pos, Vector2 velocity);

int main(void) {
    Level level;
    try {
        std::string working_dir = GetWorkingDirectory();
        std::string filepath = "Assets/level0.mid";
        std::ifstream file(filepath, std::ios::binary);
        if (!file) {
            throw std::runtime_error(std::format("Can't open level file: {}", filepath));
        }
        std::uintmax_t file_size = std::filesystem::file_size(filepath);
        if (file_size > MAX_LEVEL_FILE_SIZE) {
            throw std::runtime_error(std::format("Level file too big: {} ", filepath));
        }
        std::vector<uint8_t> buffer(file_size);
        if (!file.read(reinterpret_cast<char*>(buffer.data()), file_size)) {
            throw std::runtime_error(std::format("Error reading file: {}", filepath));
        }
        auto midi = LoadMidi(std::span<uint8_t const>(buffer));
        for (int i = 0; i < midi.tracks.size(); i++) {
            Track& track = midi.tracks[i];
            for (int j = 0; j < track.events.size(); j++) {
                Event& event = track.events[j];
                Entity enemy;
                enemy.pos.x = (float)event.start_ticks / midi.tickdiv;
                enemy.pos.y = (float)event.note - MIDI_NOTE_DEF;
                enemy.alive = false;
                enemy.can_move = false;
                enemy.type = i;
                enemy.hp = 1;
                enemy.hp_max = 1;
                enemy.last_hit_time = 0.0f;
                level.enemies.push_back(std::move(enemy));
            }
        }
        level.length = (float)midi.ticklen / midi.tickdiv;
        std::println("Found {} enemies.", level.enemies.size());
        for (auto& enemy : level.enemies) {
            std::println("Enemy: ({},{}), {}", enemy.pos.x, enemy.pos.y, enemy.hp);
        }
    }
    catch(std::exception& e) {
        std::println("{}", e.what());
    }

    InitWindow(800, 450, "ImomI");
    SetTargetFPS(60);

    InitAudioDevice();

    Music music = LoadMusicStream("Assets/clockbnt_normal.xvag.wav");
    music.looping = true;
    
    float screen_width = (float)GetScreenWidth();
    float screen_height = (float)GetScreenHeight();
    Vector2 resolution{screen_width, screen_height};

    RenderTexture2D target = LoadRenderTexture((int)screen_width, (int)screen_height);
    RenderTexture2D bufferA_target = LoadRenderTexture((int)screen_width, (int)screen_height);
    RenderTexture2D bufferB_target = LoadRenderTexture((int)screen_width, (int)screen_height);
    Shader threshold_shader = LoadShader(nullptr, "Assets/threshold.fs");
    Shader blur_shader = LoadShader(nullptr, "Assets/blur.fs");
    Shader crt_shader = LoadShader(nullptr, "Assets/crt.fs");
    int blur_direction_loc = GetShaderLocation(blur_shader, "direction");
    int crt_resolution_loc = GetShaderLocation(crt_shader, "resolution");
    SetShaderValue(crt_shader, crt_resolution_loc, &resolution, SHADER_UNIFORM_VEC2);

    PlayMusicStream(music);

    Camera2D camera = {
        .offset = { 0.0f, 0.0f },
        .target = { -screen_width, -screen_height * 0.5f },
        .rotation = 0.0f,
        .zoom = 1.0f,
    };

    Entity player = {
        .alive = true,
        .can_move = true,
        .pos = { -screen_width * 0.5f, screen_height * 0.15f },
        .velocity = { 360.0f, 360.0f },
    };

    std::vector<Entity> enemies = level.enemies;
    std::vector<Vector2> spawn_pos(enemies.size());
    for (int i = 0; i < spawn_pos.size(); i++) {
        Vector2& pos = spawn_pos[i];
        Entity& enemy = enemies[i];
        pos.x = enemy.pos.x * PIXEL_PER_UNIT;
        pos.y = enemy.pos.y * 0.1f * PIXEL_PER_UNIT;
        enemy.alive = true;
        enemy.can_move = false;
        enemy.pos = pos;
    }

    std::vector<Entity> bullets(20);
    for (int i = 0; i < bullets.size(); i++) {
        Entity& entity = bullets[i];
        entity.alive = false;
        entity.pos = { 0.0f, -999.0f };
    }

    Vector2 tail[4]{player.pos};
    int itail = 0;
    float tail_time = TAIL_TIME_DEF;

    struct {
        float x0;
        float x;
        float v;
        int dir;
        float limit;
    } bkg_markers[] = {
        { screen_width * 0.2f, screen_width * 0.2f, 20.0f, 1, 50.0f },
        { screen_width * 0.5f, screen_width * 0.5f, 20.0f, -1, 50.0f },
        { screen_width * 0.85f, screen_width * 0.85f, 20.0f, 1, 50.0f },
    };
    
    bool just_booted = true;
    bool is_paused = false;
    bool show_debug_overlay = false;
    bool can_progress = false;
    float cooldown_time = 0.4f;
    int alive_entities = 0;
    int active_entities = 0;
    float invincibility_time = INVINCIBILITY_TIME_MAX;
    float warmup_time = WARMUP_TIME_MAX;
    int score = 0;
    float multiplicator = MULTIPLICATOR_MIN;
    float strike_time = 0.0f;

    auto RestartLevel = [&]{
        is_paused = false;
        show_debug_overlay = false;
        can_progress = false;
        cooldown_time = 0.4f;
        alive_entities = 0;
        active_entities = 0;
        invincibility_time = INVINCIBILITY_TIME_MAX;
        warmup_time = WARMUP_TIME_MAX;
        score = 0;
        multiplicator = MULTIPLICATOR_MIN;
        strike_time = 0.0f;
        player = {
            .alive = true,
            .can_move = false,
            .pos = { -screen_width * 0.75f, 0.0f },
            .velocity = { 360.0f, 360.0f },
        };
        for (auto& enemy : enemies) {
            enemy.alive = true;
            enemy.can_move = false;
            enemy.pos = { 0.0f, -999.0f};
            enemy.hp = enemy.hp_max;
            enemy.last_hit_time = 0.0f;
        }
        for (Entity& entity : bullets) {
            entity.alive = false;
            entity.pos = { 0.0f, -999.0f };
        }
        camera = {
            .offset = { 0.0f, 0.0f },
            .target = { -screen_width - 0.5f * PIXEL_PER_UNIT, -screen_height * 0.5f },
            .rotation = 0.0f,
            .zoom = 1.0f,
        };
    };

    float elapsed_time = 0.0f;
    while (!WindowShouldClose()) {
        if (is_paused) {
            SetMusicVolume(music, 0.2f);
        }
        else {
            SetMusicVolume(music, 1.0f);
        }
        UpdateMusicStream(music);
        
        Inputs inputs = GetInputs();
        
        if (inputs.pause) {
            is_paused = !is_paused;
            if (!is_paused) {
                warmup_time = 3.0f;
            }
        }
        if (inputs.debug_overlay) {
            show_debug_overlay = !show_debug_overlay;
        }

        screen_width = (float)GetScreenWidth();
        screen_height = (float)GetScreenHeight();
        float frame_time = GetFrameTime();

        if (just_booted) {
            if (inputs.fire) {
                just_booted = false;
                RestartLevel();
            }
            float progression = frame_time * 300.0f;
            camera.target.x += progression;
            player.pos.x += progression;
            if (tail_time > 0.0f) {
                tail_time -= frame_time;
                if (tail_time <= 0.0f) {
                    tail_time = TAIL_TIME_DEF;
                    tail[itail] = player.pos;
                    itail = (itail + 1) % 4;
                }
            }
        }
        else if (!is_paused)
        {
            elapsed_time += frame_time;
            Vector2 screen_center = { screen_width * 0.5f, screen_height * 0.5f };

            if (inputs.reset) {
                RestartLevel();
            }

            if (warmup_time > 0.0f) {
                player.can_move = false;
                warmup_time -= frame_time;
                if (warmup_time < 0.0f) {
                    warmup_time = 0.0f;
                    can_progress = true;
                    player.can_move = true;
                }
            }

            if (cooldown_time > 0.0f && warmup_time <= 0.0f) {
                cooldown_time -= frame_time;
                if (cooldown_time <= 0.0f) {
                    cooldown_time = 0.0f;
                }
            }

            if (invincibility_time > 0.0f && warmup_time <= 0.0f) {
                invincibility_time -= frame_time;
                if (invincibility_time <= 0.0f) {
                    invincibility_time = 0.0f;
                }
            }

            if (strike_time > 0.0f && warmup_time <= 0.0f) {
                strike_time -= frame_time;
                if (strike_time <= 0.0f) {
                    strike_time = 0.0f;
                }
            }

            if (inputs.stop) {
                can_progress = !can_progress;
            }
            
            float progression = 0.0f;
            if (can_progress && warmup_time <= 0.0f) {
                progression = frame_time * 100.0f;
            }
            progression = std::roundf(progression);

            camera.offset.x += inputs.pan;
            camera.target.x += progression;
            
            player.pos.x += progression;

            if (player.can_move) {
                player.pos.x += frame_time * player.velocity.x * inputs.dir.x;
                player.pos.y += frame_time * player.velocity.y * inputs.dir.y;
            }
            player.pos.x = Clamp(player.pos.x, camera.target.x, camera.target.x + screen_width);
            player.pos.y = Clamp(player.pos.y, camera.target.y, camera.target.y + screen_height);
            
            if (tail_time > 0.0f) {
                tail_time -= frame_time;
                if (tail_time <= 0.0f) {
                    tail_time = TAIL_TIME_DEF;
                    tail[itail] = player.pos;
                    itail = (itail + 1) % 4;
                }
            }

            for (int i = 0; i < 4; i++) {
                tail[i].x += progression;
            }

            Rectangle player_rect = GetBoundingBox(player.pos.x, player.pos.y, 30.0f, 30.0f);

            if (inputs.fire && cooldown_time <= 0.0f) {
                cooldown_time = 0.12f;
                CreateBullet(bullets, { player.pos.x + 10, player.pos.y }, { 1000.0f, 0.0f });
            }

            alive_entities = 0;
            active_entities = 0;
            for (int i = 0; i < enemies.size(); i++) {
                Entity& enemy = enemies[i];
                if (!enemy.alive)
                    continue;
                alive_entities++;
                if (!enemy.can_move) {
                    Vector2& pos = spawn_pos[i];
                    if (pos.x - 10.0f >= camera.target.x + screen_width)
                        continue;
                    enemy.can_move = true;
                    enemy.pos = pos;
                }
                active_entities++;

                if (enemy.pos.x <= camera.target.x) {
                    enemy.can_move = false;
                    active_entities--;
                    continue;
                }

                Rectangle enemy_rect = GetBoundingBox(enemy.pos.x, enemy.pos.y, 20.0f, 20.0f);
                if (invincibility_time <= 0.0f && CheckCollisionRecs(player_rect, enemy_rect)) {
                    invincibility_time = INVINCIBILITY_TIME_MAX;
                    player.hp--;
                    multiplicator = MULTIPLICATOR_MIN;
                    strike_time = 0.3f;
                }

                for (int j = 0; j < bullets.size(); j++) {
                    Entity& bullet = bullets[j];
                    if (bullet.alive) {
                        Rectangle bullet_rect = GetBoundingBox(bullet.pos.x, bullet.pos.y, 10.0f, 5.0f);
                        if (CheckCollisionRecs(bullet_rect, enemy_rect)) {
                            bullet.alive = false;
                            if (enemy.type == ENEMY_SHIELD && elapsed_time - enemy.last_hit_time >= ENEMY_SHIELD_TIME_MAX) {
                                enemy.last_hit_time = elapsed_time;
                            }
                            else {
                                enemy.hp--;
                                if (enemy.hp <= 0) {
                                    enemy.alive = false;
                                    alive_entities--;
                                    score += int(multiplicator * enemy.hp_max * 100);
                                    multiplicator += 0.1f;
                                    strike_time = 0.3f;
                                    break;
                                }
                            }
                        }
                    }
                }

            }

            for (int i = 0; i < bullets.size(); i++) {
                Entity& bullet = bullets[i];
                if (bullet.alive) {
                    bullet.pos.x += frame_time * bullet.velocity.x;
                    bullet.pos.y += frame_time * bullet.velocity.y;
                    if (bullet.pos.x - 5 >= camera.target.x + screen_width || bullet.pos.x + 5 <= camera.target.x) {
                        bullet.alive = false;
                    }
                }
            }
        }

        BeginTextureMode(target);
            ClearBackground(BLANK);
            BeginMode2D(camera);
                if (just_booted) {

                }
                else {
                    for (int i = 0; i < bullets.size(); i++) {
                        Entity& bullet = bullets[i];
                        Vector2 pos = GetWorldToScreen2D(bullet.pos, camera);
                        if (pos.x + 5 <= 0 || pos.x - 5 >= screen_width) {
                            continue;
                        }
                        if (bullet.alive) {
                            DrawEntity(bullet, { 10.0f, 5.0f }, PINK);
                        }
                        else if (show_debug_overlay) {
                            Rectangle rect = GetBoundingBox(bullet.pos.x, bullet.pos.y, 10.0f, 5.0f);
                            DrawRectangleLines((int)rect.x, (int)rect.y, (int)rect.width, (int)rect.height, PURPLE);
                        }
                    }
                    for (int i = 0; i < enemies.size(); i++) {
                        Entity& enemy = enemies[i];
                        Vector2 pos = GetWorldToScreen2D(enemy.pos, camera);
                        if (pos.x + 10 <= 0 || pos.x - 10 >= screen_width) {
                            continue;
                        }
                        if (enemy.alive && enemy.can_move) { // Alive in bounds
                            if (enemy.type == ENEMY_SHIELD) {
                                float shield_time = (elapsed_time - enemy.last_hit_time) / ENEMY_SHIELD_TIME_MAX;
                                if (shield_time <= 1.0f) {
                                    float shield_size = shield_time * 20.0f;
                                    DrawEntity(enemy, { 20.0f , 20.0f }, RED);
                                    DrawEntity(enemy, { shield_size , shield_size }, SKYBLUE);
                                }
                                else {
                                    DrawEntity(enemy, { 20.0f , 20.0f }, SKYBLUE);
                                    DrawEntity(enemy, { 16.0f , 16.0f }, RED);
                                }
                            }
                            else {
                                DrawEntity(enemy, { 20.0f , 20.0f }, RED);
                            }
                        }
                        else if (show_debug_overlay) {
                            Color color;
                            if (enemy.alive && !enemy.can_move) { // Alive OOB
                                color = GREEN;
                            }
                            else if (!enemy.alive && enemy.can_move) { // Dead in bound
                                color = ORANGE;
                            }
                            else if (!enemy.alive && enemy.can_move) { // Dead OOB
                                color = RED;
                            }
                            Rectangle rect = GetBoundingBox(enemy.pos.x, enemy.pos.y, 20.0f, 20.0f);
                            DrawRectangleLines((int)rect.x, (int)rect.y, (int)rect.width, (int)rect.height, color);
                        }
                    }
                    if (show_debug_overlay){
                        DrawRectangle(Rectangle(camera.target.x, camera.target.y, screen_width, screen_height), RED);
                        DrawLine(0, (int)camera.target.y, 0, (int)(screen_height + camera.target.y), WHITE);
                        DrawLine(int(level.length * PIXEL_PER_UNIT), (int)camera.target.y, int(level.length * PIXEL_PER_UNIT), (int)(screen_height + camera.target.y), WHITE);
                    }
                }
                for (int i = 0; i < 4; i++) {
                    auto j = (i + itail) % 4;
                    auto size = 14.0f + (i + 1) * 4.0f;
                    Rectangle rect = GetBoundingBox(tail[j].x, tail[j].y, size, size);
                    DrawRectangle((int)rect.x, (int)rect.y, (int)rect.width, (int)rect.height, Color{255, 255, 255, 125});
                }
                if (invincibility_time > 0.0f) {
                    auto blink_period = INVINCIBILITY_TIME_MAX / 5;
                    float shield_size = invincibility_time / INVINCIBILITY_TIME_MAX * 30.0f;
                    auto blink_up = std::fmodf(invincibility_time, blink_period) < blink_period * 0.5f;
                    DrawEntity(player, { 30.0f , 30.0f }, DARKGRAY);
                    DrawEntity(player, { shield_size , shield_size }, blink_up ? DARKGRAY : GRAY);
                }
                else {
                    DrawEntity(player, { 30.0f , 30.0f }, GRAY);
                }
            EndMode2D();
            if (just_booted) {
                std::string press_start = "PRESS START";
                auto width = MeasureText(press_start.c_str(), 50);
                DrawText(press_start.c_str(), int(bkg_markers[3].x - width * 0.5f), int((screen_height - 50) * 0.5f), 50, WHITE);
            }
            else {
                DrawText(std::format("{}", score).c_str(), 2, 0, 50, WHITE);
                int multi_font_size = int(std::round(10 * (strike_time / 0.3f) + 30));
                Color score_color;
                if (multiplicator < 4.0f) {
                    score_color = ColorLerp(WHITE, YELLOW, (multiplicator - 1.0f) / 3.0f);
                }
                else {
                    score_color = ColorLerp(YELLOW, RED, (multiplicator - 4.0f) / 3.0f);
                }
                DrawText(std::format("x{:.1f}", multiplicator).c_str(), 2, 50, multi_font_size, score_color);
                if (show_debug_overlay) {
                    DrawText(std::format("cTime: {:.2f}", cooldown_time).c_str(), (int)screen_width / 2, 0, 20, WHITE);
                    DrawText(std::format("iTime: {:.2f}", invincibility_time).c_str(), (int)screen_width / 2, 20, 20, WHITE);
                    DrawText(std::format("Player: {:.2f},   {:.2f}", player.pos.x, player.pos.y).c_str(), 0, 0, 20, WHITE);
                    DrawText(std::format("Offset: {:.2f},   {:.2f}", camera.offset.x, camera.offset.y).c_str(), 0, 20, 20, WHITE);
                    DrawText(std::format("Target: {:.2f},   {:.2f}", camera.target.x, camera.target.y).c_str(), 0, 40, 20, WHITE);
                    DrawText(std::format("Rotation: {:.2f}", camera.rotation).c_str(), 0, 60, 20, WHITE);
                    DrawText(std::format("Zoom: {:.2f}", camera.zoom).c_str(), 0, 80, 20, WHITE);
                    DrawText(std::format("Alive: {}", alive_entities).c_str(), 0, (int)screen_height - 80, 20, WHITE);
                    DrawText(std::format("Active: {}", active_entities).c_str(), 0, (int)screen_height - 60, 20, WHITE);
                    DrawText(std::format("Dead: {}", enemies.size() - alive_entities).c_str(), 0, (int)screen_height - 40, 20, WHITE);
                    DrawText(std::format("Inactive: {}", alive_entities - active_entities).c_str(), 0, (int)screen_height - 20, 20, WHITE);
                }
                if (warmup_time > 0.0f) {
                    auto rounded_time = (int)warmup_time;
                    auto text = rounded_time ? std::to_string(rounded_time) : "GO";
                    auto subtime = Wrap(warmup_time, 0.0f, 1.0f);
                    auto font_size = int(std::round(20 * subtime + 50));
                    int width = MeasureText(text.c_str(), font_size);
                    DrawText(text.c_str(), ((int)screen_width - width) / 2, (int)screen_height / 4, font_size, WHITE);
                    auto subtime_text = std::format("{:.2f}", subtime);
                    width = MeasureText(subtime_text.c_str(), 30);
                    DrawText(subtime_text.c_str(), ((int)screen_width - width) / 2, (int)screen_height / 4 - 30, 30, WHITE);
                }
            }
        EndTextureMode();

        BeginTextureMode(bufferA_target);
            ClearBackground(BLANK);
            BeginShaderMode(threshold_shader);
                DrawTexturePro(
                    target.texture,
                    Rectangle{0, 0, screen_width, -screen_height},
                    Rectangle{0, 0, screen_width, screen_height},
                    Vector2{0.0f, 0.0f},
                    0.0f,
                    WHITE
                );
            EndShaderMode();
        EndTextureMode();
        
        for (int i = 0; i < 5; i++) {
            BeginTextureMode(bufferB_target);
                BeginShaderMode(blur_shader);
                    Vector2 blur_direction{1.5f / screen_width, 0.0f};
                    SetShaderValue(blur_shader, blur_direction_loc, &blur_direction, SHADER_UNIFORM_VEC2);
                    DrawTexturePro(
                        bufferA_target.texture,
                        Rectangle{0, 0, screen_width, -screen_height},
                        Rectangle{0, 0, screen_width, screen_height},
                        Vector2{0.0f, 0.0f},
                        0.0f,
                        WHITE
                    );
                EndShaderMode();
            EndTextureMode();

            BeginTextureMode(bufferA_target);
                BeginShaderMode(blur_shader);
                    blur_direction = {0.0f, 1.5f / screen_height};
                    SetShaderValue(blur_shader, blur_direction_loc, &blur_direction, SHADER_UNIFORM_VEC2);
                    DrawTexturePro(
                        bufferB_target.texture,
                        Rectangle{0, 0, screen_width, -screen_height},
                        Rectangle{0, 0, screen_width, screen_height},
                        Vector2{0.0f, 0.0f},
                        0.0f,
                        WHITE
                    );
                EndShaderMode();
            EndTextureMode();
        }

        for (int i = 0; i < 3; i++) {
            auto& marker = bkg_markers[i];
            marker.x += marker.dir * marker.v * frame_time;
            if (abs(marker.x - marker.x0) > marker.limit) {
                marker.dir *= -1;
                marker.limit = float(GetRandomValue(20, 50));
                marker.v = float(GetRandomValue(10, 20));
                Clamp(marker.x, marker.x0 - marker.limit, marker.x0 + marker.limit);
            }
        }

        BeginTextureMode(bufferB_target);
            ClearBackground(BLANK);
            DrawRectangleGradientH(0, 0, int(bkg_markers[0].x), int(screen_height), DARKPURPLE, BLACK);
            DrawRectangleGradientH(int(bkg_markers[0].x), 0, int(bkg_markers[1].x - bkg_markers[0].x + 1), int(screen_height), BLACK, DARKPURPLE);
            DrawRectangle(int(bkg_markers[1].x), 0, int(bkg_markers[2].x - bkg_markers[1].x + 1), int(screen_height), DARKPURPLE);
            DrawRectangleGradientH(int(bkg_markers[2].x), 0, int(screen_width - bkg_markers[2].x + 1), int(screen_height), DARKPURPLE, PURPLE);
            if (show_debug_overlay) {
                DrawLine(int(bkg_markers[0].x), 0, int(bkg_markers[0].x), int(screen_height), PINK);
                DrawLine(int(bkg_markers[1].x), 0, int(bkg_markers[1].x), int(screen_height), PINK);
                DrawLine(int(bkg_markers[2].x), 0, int(bkg_markers[2].x), int(screen_height), PINK);
            }
            DrawTexturePro(
                target.texture,
                Rectangle{0, 0, screen_width, -screen_height},
                Rectangle{0, 0, screen_width, screen_height},
                Vector2{0.0f, 0.0f},
                0.0f,
                WHITE
            );
            BeginBlendMode(BLEND_ADDITIVE);
                DrawTexturePro(
                    bufferA_target.texture,
                    Rectangle{0, 0, screen_width, -screen_height},
                    Rectangle{0, 0, screen_width, screen_height},
                    Vector2{0.0f, 0.0f},
                    0.0f,
                    WHITE
                );
            EndBlendMode();

            if (is_paused) {
                DrawRectangle(0, 0, (int)screen_width, (int)screen_height, Color{0, 0, 0, 125});
                int width = MeasureText("Pause", 24);
                DrawText("Pause", ((int)screen_width - width) / 2, ((int)screen_height - 12) / 2, 25, WHITE);
            }

            DrawFPS((int)screen_width - 100, (int)screen_height - 50);
        EndTextureMode();

        BeginDrawing();
            ClearBackground(BLACK);
            BeginShaderMode(crt_shader);
                DrawTexturePro(
                    bufferB_target.texture,
                    Rectangle{0, 0, screen_width, -screen_height},
                    Rectangle{0, 0, screen_width, screen_height},
                    Vector2{0.0f, 0.0f},
                    0.0f,
                    WHITE
                );
            EndShaderMode();
        EndDrawing();
    }

    UnloadMusicStream(music);
    UnloadRenderTexture(target);
    UnloadRenderTexture(bufferA_target);
    UnloadRenderTexture(bufferB_target);
    UnloadShader(threshold_shader);
    UnloadShader(blur_shader);

    CloseAudioDevice();

    CloseWindow();

    return 0;
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

void DrawEntity(Entity const& entity, Vector2 size, Color color)
{
    Rectangle rect = GetBoundingBox(entity.pos.x, entity.pos.y, size.x, size.y);
    DrawRectangle((int)rect.x, (int)rect.y, (int)rect.width, (int)rect.height, color);
}

void CreateBullet(std::vector<Entity>& bullets, Vector2 pos, Vector2 velocity)
{
    for (int i = 0; i < bullets.size(); i++) {
        Entity& bullet = bullets[i];
        if (!bullet.alive) {
            bullet.alive = true;
            bullet.pos = pos;
            bullet.velocity = velocity;
            break;
        }
    }
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
        if (dir.x || dir.y)
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
    inputs.stop = IsKeyPressed(KEY_I);
    inputs.debug_overlay = IsKeyPressed(KEY_O);
    inputs.pan = 0.0f;
    if (IsKeyPressed(KEY_J) || IsKeyPressedRepeat(KEY_J))
        inputs.pan = -50.0f;
    if (IsKeyPressed(KEY_L) || IsKeyPressedRepeat(KEY_L))
        inputs.pan = 50.0f;
    if (IsGamepadAvailable(0)) {
        inputs.pause |= IsGamepadButtonPressed(0, GAMEPAD_BUTTON_MIDDLE_RIGHT);
        inputs.fire |= IsGamepadButtonDown(0, GAMEPAD_BUTTON_RIGHT_FACE_RIGHT);
        inputs.reset |= IsGamepadButtonPressed(0, GAMEPAD_BUTTON_RIGHT_FACE_UP);
        inputs.stop |= IsGamepadButtonPressed(0, GAMEPAD_BUTTON_LEFT_FACE_DOWN);
        inputs.debug_overlay |= IsGamepadButtonPressed(0, GAMEPAD_BUTTON_MIDDLE_LEFT);
        if (IsGamepadButtonPressed(0, GAMEPAD_BUTTON_LEFT_FACE_LEFT))
            inputs.pan = -50.0f;
        if (IsGamepadButtonPressed(0, GAMEPAD_BUTTON_LEFT_FACE_RIGHT))
            inputs.pan = 50.0f;
    }
    return inputs;
}