#include "raylib.h"
#include "raymath.h"
#include <fstream>
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
    int type;
};

struct Inputs {
    Vector2 dir;
    bool pause;
    bool fire;
    bool reset;
    float pan;
    bool stop;
};


Inputs GetInputs();
Rectangle GetBoundingBox(float cx, float cy, float width, float height);
void DrawRectangle(Rectangle rect, Color color);
void DrawEntity(Entity const& entity, Vector2 size, Color color);
void CreateBullet(std::vector<Entity>& bullets, Vector2 pos, Vector2 velocity);


template<typename... Args>
std::string dyn_format(std::string_view rt_fmt_str, Args&&... args);


struct Level {
    std::vector<Entity> enemies;
};

Level LoadLevelPackage(std::string const& name);


int main(void) {
    Level level;
    try {
        level = LoadLevelPackage("Assets/level0.mid");
        std::println("Found {} enemies.", level.enemies.size());
        for (auto& enemy : level.enemies) {
            std::println("Enemy: ({},{})", enemy.pos.x, enemy.pos.y);
        }
    }
    catch(std::exception& e) {
        std::println("{}", e.what());
    }
    // return 0;

    InitWindow(800, 450, "ImomI");
    SetTargetFPS(60);

    InitAudioDevice();

    Music music = LoadMusicStream("Assets/clocksuv_normal.xvag.wav");
    music.looping = true;

    PlayMusicStream(music);

    bool is_paused = false;
    Camera2D camera;
    camera.offset = { 0.0f, 0.0f };
    camera.rotation = 0.0f;
    camera.target = { -400.0f, 0.0f };
    camera.zoom = 1.0f;

    Entity player = {
        .alive = true,
        .can_move = true,
        .pos = { -300.0f, 225.0f },
        .velocity = { 360.0f, 360.0f },
    };

    // std::vector<Vector2> spawn_pos(50);
    // for (int i = 0; i < spawn_pos.size(); i++) {
    //     Vector2& pos = spawn_pos[i];
    //     pos.x = i * 100.0f;
    //     pos.y = (i * 100) % 350 + 50.0f;
    // }

    // std::vector<Entity> enemies(50);
    // for (int i = 0; i < enemies.size(); i++) {
    //     Entity& entity = enemies[i];
    //     entity.alive = true;
    //     entity.can_move = false;
    //     entity.pos = spawn_pos[i];
    // }
    std::vector<Entity> enemies = level.enemies;
    std::vector<Vector2> spawn_pos(enemies.size());
    for (int i = 0; i < spawn_pos.size(); i++) {
        Vector2& pos = spawn_pos[i];
        Entity& enemy = enemies[i];
        pos.x = enemy.pos.x * PIXEL_PER_UNIT;
        pos.y = enemy.pos.y * 0.1f * PIXEL_PER_UNIT - 590 + GetScreenHeight() / 2;
        enemy.alive = true;
        enemy.can_move = false;
        enemy.pos = pos;
    }

    std::vector<Entity> bullets(20);
    for (int i = 0; i < bullets.size(); i++) {
        Entity& entity = bullets[i];
        entity.alive = false;
        entity.pos = { 0.0f, -50.0f };
    }
    
    bool can_progress = false;
    float cooldown_time = 0.4f;
    int alive_entities = 0;
    int active_entities = 0;
    while (!WindowShouldClose()) {
        if (is_paused) {
            SetMusicVolume(music, 0.2f);
        }
        else {
            SetMusicVolume(music, 1.0f);
        }
        UpdateMusicStream(music);
        
        Inputs inputs = GetInputs();
        
        if (inputs.pause)
            is_paused = !is_paused;

        float screen_width = (float)GetScreenWidth();
        float screen_height = (float)GetScreenHeight();
        if (!is_paused)
        {
            float frame_time = GetFrameTime();
            Vector2 screen_center = { screen_width * 0.5f, screen_height * 0.5f };
            
            cooldown_time -= frame_time;
            if (cooldown_time <= 0.0f)
                cooldown_time = 0.0f;

            if (inputs.stop)
                can_progress = !can_progress;
            
            float progression = 0.0f;
            if (can_progress)
                progression = frame_time * 100.0f;

            camera.offset.x += inputs.pan;
            camera.target.x += progression;
            
            player.pos.x += progression;
            player.pos.x += frame_time * player.velocity.x * inputs.dir.x;
            player.pos.y += frame_time * player.velocity.y * inputs.dir.y;

            player.pos.x = Clamp(player.pos.x, camera.target.x + 50, camera.target.x + screen_width - 50);
            player.pos.y = Clamp(player.pos.y, camera.target.y + 50, camera.target.y + screen_height - 50);

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
                    if (pos.x - 10.0f >= camera.target.x + screen_width - 50)
                        continue;
                    enemy.can_move = true;
                    enemy.pos = pos;
                }
                active_entities++;

                if (enemy.pos.x <= camera.target.x + 50) {
                    enemy.can_move = false;
                    active_entities--;
                    continue;
                }

                for (int j = 0; j < bullets.size(); j++) {
                    Entity& bullet = bullets[j];
                    if (bullet.alive) {
                        Rectangle enemy_rect = GetBoundingBox(enemy.pos.x, enemy.pos.y, 20.0f, 20.0f);
                        Rectangle bullet_rect = GetBoundingBox(bullet.pos.x, bullet.pos.y, 10.0f, 5.0f);
                        if (CheckCollisionRecs(bullet_rect, enemy_rect)) {
                            bullet.alive = false;
                            enemy.alive = false;
                            alive_entities--;
                            break;
                        }
                    }
                }
            }

            for (int i = 0; i < bullets.size(); i++) {
                Entity& bullet = bullets[i];
                if (bullet.alive) {
                    bullet.pos.x += frame_time * bullet.velocity.x;
                    bullet.pos.y += frame_time * bullet.velocity.y;
                    if (bullet.pos.x - 5 >= camera.target.x + screen_width - 50 || bullet.pos.x + 5 <= camera.target.x + 50) {
                        bullet.alive = false;
                    }
                }
            }
        }

        BeginDrawing();
            ClearBackground(BLACK);
            BeginMode2D(camera);
                for (int i = 0; i < enemies.size(); i++) {
                    Entity& enemy = enemies[i];
                    Vector2 pos = GetWorldToScreen2D(enemy.pos, camera);
                    if (pos.x + 10 <= 0 || pos.x - 10 >= screen_width)
                        continue;
                    Color color;
                    if (enemy.alive && enemy.can_move) { // Alive in bounds
                        if (enemy.type == 1) {
                            color = YELLOW;
                        }
                        else {
                            color = LIME;
                        }
                    }
                    else if (enemy.alive && !enemy.can_move) // Alive OOB
                        color = BLUE;
                    else if (!enemy.alive && enemy.can_move) // Dead in bound
                        color = ORANGE;
                    else if (!enemy.alive && enemy.can_move) // Dead OOB
                        color = RED;
                    DrawEntity(enemy, { 20.0f, 20.0f }, color);
                }
                for (int i = 0; i < bullets.size(); i++) {
                    Entity& bullet = bullets[i];
                    Vector2 pos = GetWorldToScreen2D(bullet.pos, camera);
                    if (pos.x + 5 <= 0 || pos.x - 5 >= screen_width)
                        continue;
                    Color color;
                    if (bullet.alive)
                        color = PINK;
                    else
                        color = PURPLE;
                    DrawEntity(bullet, { 10.0f, 5.0f }, color);
                }
                DrawEntity(player, { 30.0f , 30.0f }, SKYBLUE);
                DrawText(dyn_format("{:.2f}", cooldown_time).c_str(), (int)player.pos.x - 15, (int)player.pos.y - 15, 20, WHITE);
            EndMode2D();
            DrawRectangle(Rectangle(50, 50, screen_width - 100, screen_height - 100), RED);
            DrawText(dyn_format("Offset: {:.2f},   {:.2f}", camera.offset.x, camera.offset.y).c_str(), 0, 0, 20, WHITE);
            DrawText(dyn_format("Target: {:.2f},   {:.2f}", camera.target.x, camera.target.y).c_str(), 0, 20, 20, WHITE);
            DrawText(dyn_format("Rotation: {:.2f}", camera.rotation).c_str(), 0, 40, 20, WHITE);
            DrawText(dyn_format("Zoom: {:.2f}", camera.zoom).c_str(), 0, 60, 20, WHITE);
            DrawText(dyn_format("Player: {:.2f},   {:.2f}", player.pos.x, player.pos.y).c_str(), 0, 80, 20, WHITE);
            DrawText(dyn_format("Alive: {}", alive_entities).c_str(), 0, (int)screen_height - 80, 20, WHITE);
            DrawText(dyn_format("Active: {}", active_entities).c_str(), 0, (int)screen_height - 60, 20, WHITE);
            DrawText(dyn_format("Dead: {}", enemies.size() - alive_entities).c_str(), 0, (int)screen_height - 40, 20, WHITE);
            DrawText(dyn_format("Inactive: {}", alive_entities - active_entities).c_str(), 0, (int)screen_height - 20, 20, WHITE);
            if (is_paused) {
                DrawRectangle(0, 0, (int)screen_width, (int)screen_height, Color{0, 0, 0, 125});
                int width = MeasureText("Pause", 24);
                DrawText("Pause", ((int)screen_width - width) / 2, ((int)screen_height - 12) / 2, 25, WHITE);
            }
        EndDrawing();
    }

    UnloadMusicStream(music);

    CloseAudioDevice();

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

void DrawEntity(Entity const& entity, Vector2 size, Color color)
{
    Rectangle rect = GetBoundingBox(entity.pos.x, entity.pos.y, size.x, size.y);
    DrawRectangleLines((int)rect.x, (int)rect.y, (int)rect.width, (int)rect.height, color);
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

std::string GetSystemMessageName(uint8_t byte) {
    switch (byte)
    {
    case 0xf0: return "";
    default: return "???";
    }
}

std::string GetVoiceMessageName(uint8_t byte) {
    switch (byte & 0xf0)
    {
    case 0x80: return "Note Off";
    case 0x90: return "Note On";
    case 0xA0: return "Polyphonic Pressure";
    case 0xB0: return "Controller";
    case 0xC0: return "Program Change";
    case 0xD0: return "Channel Pressure";
    case 0xE0: return "Pitch Bend";
    default: return "???";
    }
}

Level LoadLevelPackage(std::string const &name)
{
    struct Event {
        uint8_t channel;
        uint8_t note;
        uint8_t velocity;
        int start_ticks;
    };

    struct Track {
        std::string name;
        std::vector<Event> events;
    };

    std::string working_dir = GetWorkingDirectory();
    std::println("{}", working_dir);
    std::ifstream pkg(name, std::ios::binary);
    if (!pkg) {
        throw std::runtime_error(std::format("Can't open package file {}", name));
    }

    std::vector<Track> tracks;

    std::println("#=== MIDI file: {}", name);
    
    // File header
    int constexpr header_size = 8;
    std::string header(header_size, '\0');
    pkg.read(header.data(), header_size);
    std::println("#--- Header ---");

    auto identifier = header.substr(0, 4);
    std::println("Identifier: {}", identifier);

    uint32_t chunklen = 0;
    for (int i = 0; i < 4; i++) {
        chunklen |= uint32_t(uint8_t(header[4 + i])) << (8 * (3 - i));
    }
    std::println("Chunk length: {}", chunklen);

    std::vector<uint8_t> header_buffer(chunklen, 0);
    pkg.read(reinterpret_cast<char*>(header_buffer.data()), chunklen);
    int16_t format = (header_buffer[0] << 8) | header_buffer[1];
    int16_t ntracks = (header_buffer[2] << 8) | header_buffer[3];
    int16_t tickdiv = (header_buffer[4] << 8) | header_buffer[5];
    std::println("Format: {}", format);
    std::println("NTracks: {}", ntracks);
    std::println("Tickdiv: {}", tickdiv);

    for (int ic = 0; ic < ntracks; ic++) {
        Track track;

        std::vector<uint8_t> header(header_size, 0);
        pkg.read(reinterpret_cast<char*>(header.data()), header_size);
        std::println("#--- Track ---");
        
        std::string identifier(reinterpret_cast<char*>(header.data()), 4);
        std::println("Identifier: {}", identifier);
        
        uint32_t chunklen = 0;
        for (int i = 0; i < 4; i++) {
            chunklen |= uint32_t(uint8_t(header[4 + i])) << (8 * (3 - i));
        }
        std::println("Chunk length: {}", chunklen);

        std::vector<uint8_t> buffer(chunklen, 0);
        pkg.read(reinterpret_cast<char*>(buffer.data()), chunklen);

        std::vector<uint8_t> sysex_buffer;
        bool sysex = false;
        uint32_t ticks = 0;
        auto it = buffer.begin();
        while (it != buffer.end()) {
            std::println("#-- Event --");
            uint32_t delta_time = 0;
            uint8_t byte = 0;
            auto ita = it;
            do {
                byte = *it;
                delta_time = (delta_time << 7) | (byte & 0x7f);
                it++;
            } while (byte & 0x80 && it != buffer.end());
            std::print("Delta time: {}", delta_time);
            std::print(" | ");
            for (auto itb = ita; itb != it; itb++) {
                std::print("{:02X} ", *itb);
            }
            std::println();

            ticks += delta_time;

            byte = *it;
            ita = it;
            if (byte == 0xff) {
                std::println("Type: Meta | {:02X}", byte);
                it++;
                uint8_t msg = *it;
                std::println("Message: {:02X}", msg);
                it++;
    
                uint32_t length = 0;
                do {
                    byte = *it;
                    auto b = (byte & 0x7f);
                    length = (length << 7) | (byte & 0x7f);
                    it++;
                } while (byte & 0x80 && it != buffer.end());
                std::println("Length: {}", length);
                
                if (msg == 0x03) {

                }
                else {
                    // skip data for now
                    it += length;
                }

            }
            if (byte == 0xf0) {
                std::println("Type: SysEx | {:02X}", byte);

                uint32_t length = 0;
                do {
                    byte = *it;
                    auto b = (byte & 0x7f);
                    length = (length << 7) | (byte & 0x7f);
                    it++;
                } while (byte & 0x80 && it != buffer.end());
                std::println("Length: {}", length);

                it += length;
                sysex_buffer.insert(sysex_buffer.end(), ita, it);
                if (length && *(it - 1) != 0xf7) {
                    sysex = true;
                }
            }
            if (byte == 0xf7) {
                std::println("Type: SysEx | {:02X}", byte);

                uint32_t length = 0;
                do {
                    byte = *it;
                    auto b = (byte & 0x7f);
                    length = (length << 7) | (byte & 0x7f);
                    it++;
                } while (byte & 0x80 && it != buffer.end());
                std::println("Length: {}", length);

                it += length;
                if (sysex) {
                    sysex_buffer.insert(sysex_buffer.end(), ita, it);
                }
                if (length && *(it - 1) == 0xf7) {
                    sysex = false;
                    std::print("SysEx buffer: ");
                    for (int i = 0; i < sysex_buffer.size(); i++) {
                        std::print("{:02X} ", sysex_buffer[i]);
                    }
                    std::println();
                    sysex_buffer.clear();
                }
            }
            if ((byte & 0xf0) >= 0x80) {
                std::println("Type: MIDI | {:02X}", byte);
                std::println("Message: {}", GetVoiceMessageName(byte));
                uint8_t channel = byte & 0x0f;
                std::println("Channel: {}", channel);
                uint32_t length = ((byte & 0xf0) >= 0xc0 && (byte & 0xf0) < 0xe0) ? 2 : 3;
                std::println("Length: {}", length);
                std::fflush(stdout);

                if ((byte & 0xf0) == 0x90) { // Note On
                    it++;
                    uint8_t note = *it;
                    it++;
                    uint8_t velocity = *it;
                    it++;
                    std::println("Note: {}", note);
                    std::println("velocity: {}", velocity);
                    Event event;
                    event.channel = channel;
                    event.start_ticks = ticks;
                    event.note = note;
                    event.velocity = velocity;
                    track.events.push_back(std::move(event));
                }
                else {
                    it += length;
                }
            }
            std::print(" | ");
            for (auto itb = ita; itb != it; itb++) {
                std::print("{:02X} ", *itb);
            }
            std::println();
            std::fflush(stdout);
        }
        tracks.push_back(std::move(track));
    }

    Level level;
    for (int i = 0; i < tracks.size(); i++) {
        Track& track = tracks[i];
        for (int j = 0; j < track.events.size(); j++) {
            Event& event = track.events[j];
            Entity enemy;
            enemy.pos.x = (float)event.start_ticks / tickdiv;
            enemy.pos.y = (float)event.note;
            enemy.alive = false;
            enemy.can_move = false;
            enemy.type = i;
            level.enemies.push_back(std::move(enemy));
        }
    }
    return level;
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
        if (IsGamepadButtonPressed(0, GAMEPAD_BUTTON_LEFT_FACE_LEFT))
            inputs.pan = -50.0f;
        if (IsGamepadButtonPressed(0, GAMEPAD_BUTTON_LEFT_FACE_RIGHT))
            inputs.pan = 50.0f;
    }
    return inputs;
}