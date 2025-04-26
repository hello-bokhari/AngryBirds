#include "raylib.h"
#include <cmath>
#include <array>
#include <vector>
#include <string>

constexpr int MAX_OBSTACLES = 30;
constexpr int PROBE_QUANTITY = 10;
constexpr int VELOCITY_MULTIPLIER = 40;
constexpr int LAUNCH_MAX_DISTANCE = 100;
constexpr float GRAVITY = 1.0f;

float toRadians(float degrees) {
    return degrees * (PI / 180.0f);
}

class Obstacle {
public:
    Rectangle rect;
    bool visible;
    Color fillColor;
    Color strokeColor;

    Obstacle(Rectangle r, bool v, Color fill, Color stroke)
        : rect(r), visible(v), fillColor(fill), strokeColor(stroke) {
    }

    void Draw() const {
        if (visible) {
            DrawRectangleRec(rect, fillColor);
            DrawRectangleLinesEx(rect, 2, strokeColor);
        }
    }
};

class Ball {
public:
    Vector2 pos{};
    Vector2 vel{};
    float radius = 40;
    float friction = 0.99f;
    float elasticity = 0.9f;
    float rotationAngle = 0;
    std::array<float, PROBE_QUANTITY> collisionProbes{ 0, 45, 90, 135, 180, 225, 270, 315 };
    Texture2D staringTexture{};
    Texture2D surprisedTexture{};
    Texture2D launchedTexture{};
    Color fillColor = BLUE;
    Color strokeColor = DARKBLUE;

    void Draw(bool launched, Ball* selectedBall, float xStart, float yStart) {
        if (!launched) {
            DrawLine(xStart, yStart, pos.x, pos.y, BLACK);
            float px = pos.x, py = pos.y;
            float vx = vel.x, vy = vel.y;

            for (int i = 0; i < 50; ++i) {
                if (i != 0) {
                    DrawLine(px - vx, py - vy + GRAVITY, px, py, strokeColor);
                }
                px += vx;
                py += vy;
                vy += GRAVITY;
            }
        }

        Texture2D textureToDraw = staringTexture;
        if (selectedBall != nullptr)
            textureToDraw = surprisedTexture;
        else if (launched)
            textureToDraw = launchedTexture;

        DrawTexturePro(
            textureToDraw,
            { 0, 0, 420, 420 },
            { pos.x, pos.y, radius * 2, radius * 2 },
            { (float)radius, (float)radius },
            rotationAngle,
            WHITE
        );
    }

    float GetProbePosition(bool forX, int probeIndex) {
        float p = forX ? pos.x : pos.y;
        float angleRad = toRadians(collisionProbes[probeIndex % PROBE_QUANTITY]);
        float t = forX ? cosf(angleRad) : sinf(angleRad);
        return p + radius * ((probeIndex / PROBE_QUANTITY) > 0 ? 0.5f : 1.0f) * t;
    }

    bool CollidesWith(const Obstacle& obs) {
        if (!obs.visible) return false;

        for (int i = 0; i < PROBE_QUANTITY * 2; ++i) {
            Vector2 point = { GetProbePosition(true, i), GetProbePosition(false, i) };
            if (CheckCollisionPointRec(point, obs.rect)) {
                return true;
            }
        }

        return false;
    }
};

void DrawCloud(int x, int y, int scale = 1) {
    Color cloudColor = { 255, 255, 255, 240 }; // Soft white

    DrawEllipse(x, y, 30 * scale, 20 * scale, cloudColor);
    DrawEllipse(x + 20 * scale, y - 10 * scale, 25 * scale, 18 * scale, cloudColor);
    DrawEllipse(x + 40 * scale, y, 30 * scale, 20 * scale, cloudColor);
}

class Button {
private:
    Texture2D texture;
    Vector2 position;
    float scale;
    bool wasPressed;

public:
    Button(const char* imagePath, Vector2 imagePosition, float scale) : wasPressed(false) {
        Image image = LoadImage(imagePath);
        int originalwidth = image.width;
        int originalheight = image.height;

        int newWidth = static_cast<int>(originalwidth * scale);
        int newheight = static_cast<int>(originalheight * scale);

        ImageResize(&image, newWidth, newheight);

        texture = LoadTextureFromImage(image);
        UnloadImage(image);

        position = imagePosition;
    }

    ~Button() {
        UnloadTexture(texture);
    }

    void Draw() {
        DrawTextureV(texture, position, WHITE);
    }

    bool isClicked(Vector2 mousePos) {
        Rectangle rect = { position.x, position.y, static_cast<float>(texture.width), static_cast<float>(texture.height) };

        // Check if mouse is over button
        bool isOver = CheckCollisionPointRec(mousePos, rect);

        // Check if button was just clicked (pressed and released while over the button)
        if (isOver && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
            wasPressed = true;
            return false;
        }

        // Check if button was released while over it and previously pressed
        if (isOver && wasPressed && IsMouseButtonReleased(MOUSE_BUTTON_LEFT)) {
            wasPressed = false;
            return true;
        }

        // If mouse button was released but not over the button, reset wasPressed
        if (IsMouseButtonReleased(MOUSE_BUTTON_LEFT)) {
            wasPressed = false;
        }

        return false;
    }
};

enum class LevelState {
    PLAYING,
    COMPLETED,
    FAILED
};

class Level {
public:
    std::vector<Obstacle> obstacles;
    std::string name;
    int targetScore;
    bool initialized = false;
    LevelState state = LevelState::PLAYING;

    Level(const std::string& levelName, int requiredScore) : name(levelName), targetScore(requiredScore) {}

    virtual void Initialize(float groundY) = 0;

    void Reset() {
        for (auto& obs : obstacles) {
            obs.visible = true;
        }
        state = LevelState::PLAYING;
    }

    int GetCurrentScore() {
        int score = 0;
        for (const auto& obs : obstacles) {
            if (!obs.visible) {
                score += 10; // 10 points per destroyed obstacle
            }
        }
        return score;
    }

    void Update() {
        // Check if level is completed based on score
        if (GetCurrentScore() >= targetScore) {
            state = LevelState::COMPLETED;
        }
    }

    virtual ~Level() {}
};

class Level1 : public Level {
public:
    Level1() : Level("Starter Tower", 100) {}  // Increased target score from 70 to 100

    void Initialize(float groundY) override {
        obstacles.clear();

        // Base blocks - wider foundation
        for (int i = 0; i < 6; ++i) {
            obstacles.push_back({ { 750.0f + i * 35.0f, groundY - 40.0f, 30.0f, 40.0f }, true, GREEN, DARKGREEN });
        }

        // Middle row - staggered for stability
        for (int i = 0; i < 4; ++i) {
            obstacles.push_back({ { 760.0f + i * 35.0f, groundY - 80.0f, 30.0f, 40.0f }, true, YELLOW, GOLD });
        }

        // Third row - narrower
        for (int i = 0; i < 3; ++i) {
            obstacles.push_back({ { 770.0f + i * 35.0f, groundY - 120.0f, 30.0f, 40.0f }, true, ORANGE, BROWN });
        }

        // Top blocks
        obstacles.push_back({ { 785.0f, groundY - 160.0f, 30.0f, 40.0f }, true, RED, MAROON });
        obstacles.push_back({ { 825.0f, groundY - 160.0f, 30.0f, 40.0f }, true, RED, MAROON });

        // Challenge: Floating platform with obstacles
        obstacles.push_back({ { 650.0f, groundY - 140.0f, 80.0f, 15.0f }, true, GRAY, BLACK });  // Platform
        obstacles.push_back({ { 670.0f, groundY - 170.0f, 30.0f, 30.0f }, true, BLUE, DARKBLUE });  // Block on platform

        initialized = true;
    }
};

class Level2 : public Level {
public:
    Level2() : Level("Fortified Castle", 150) {}  // Increased target score from 120 to 150

    void Initialize(float groundY) override {
        obstacles.clear();

        // Castle-like structure with defensive elements
        // Main structure - base level with thicker walls
        for (int i = 0; i < 8; ++i) {
            obstacles.push_back({ { 740.0f + i * 40.0f, groundY - 100.0f, 30.0f, 100.0f }, true, GREEN, DARKGREEN });
        }

        // Second level wall sections
        for (int i = 0; i < 7; ++i) {
            if (i == 3) continue; // gap in the middle
            obstacles.push_back({ { 750.0f + i * 40.0f, groundY - 160.0f, 30.0f, 60.0f }, true, YELLOW, GOLD });
        }

        // Horizontal reinforcement beams
        obstacles.push_back({ { 740.0f, groundY - 110.0f, 300.0f, 20.0f }, true, RED, MAROON });
        obstacles.push_back({ { 750.0f, groundY - 170.0f, 280.0f, 20.0f }, true, RED, MAROON });
        obstacles.push_back({ { 760.0f, groundY - 230.0f, 260.0f, 20.0f }, true, RED, MAROON });

        // Defensive towers
        obstacles.push_back({ { 760.0f, groundY - 280.0f, 30.0f, 60.0f }, true, BLUE, DARKBLUE });
        obstacles.push_back({ { 830.0f, groundY - 280.0f, 30.0f, 60.0f }, true, BLUE, DARKBLUE });
        obstacles.push_back({ { 900.0f, groundY - 280.0f, 30.0f, 60.0f }, true, BLUE, DARKBLUE });
        obstacles.push_back({ { 970.0f, groundY - 280.0f, 30.0f, 60.0f }, true, BLUE, DARKBLUE });

        // Top roof structure
        obstacles.push_back({ { 750.0f, groundY - 290.0f, 250.0f, 20.0f }, true, PURPLE, DARKPURPLE });

        // Obstacle in the center - harder to reach
        obstacles.push_back({ { 830.0f, groundY - 200.0f, 40.0f, 40.0f }, true, SKYBLUE, BLUE });

        // External defensive wall
        obstacles.push_back({ { 700.0f, groundY - 70.0f, 20.0f, 70.0f }, true, DARKGRAY, BLACK });
        obstacles.push_back({ { 1020.0f, groundY - 70.0f, 20.0f, 70.0f }, true, DARKGRAY, BLACK });

        initialized = true;
    }
};

class Level3 : public Level {
public:
    Level3() : Level("Impenetrable Stronghold", 300) {}  // Increased target score from 200 to 300

    void Initialize(float groundY) override {
        obstacles.clear();

        // Heavily fortified multi-layer complex

        // Outer defensive walls - thicker and taller
        for (int i = 0; i < 7; ++i) {
            obstacles.push_back({ { 650.0f, groundY - 40.0f - i * 40.0f, 40.0f, 40.0f }, true, GRAY, BLACK });
            obstacles.push_back({ { 950.0f, groundY - 40.0f - i * 40.0f, 40.0f, 40.0f }, true, GRAY, BLACK });
        }

        // Connecting outer walls bottom
        for (int i = 0; i < 6; ++i) {
            obstacles.push_back({ { 690.0f + i * 40.0f, groundY - 40.0f, 40.0f, 40.0f }, true, DARKGRAY, BLACK });
        }

        // Connecting outer walls top
        for (int i = 0; i < 6; ++i) {
            obstacles.push_back({ { 690.0f + i * 40.0f, groundY - 280.0f, 40.0f, 40.0f }, true, DARKGRAY, BLACK });
        }

        // Inner fortress - main towers
        for (int i = 0; i < 5; ++i) {
            obstacles.push_back({ { 700.0f, groundY - 80.0f - i * 40.0f, 40.0f, 40.0f }, true, BLUE, DARKBLUE });
            obstacles.push_back({ { 900.0f, groundY - 80.0f - i * 40.0f, 40.0f, 40.0f }, true, BLUE, DARKBLUE });
        }

        // Inner fortress - connecting walls
        for (int i = 0; i < 4; ++i) {
            obstacles.push_back({ { 740.0f + i * 40.0f, groundY - 80.0f, 40.0f, 40.0f }, true, RED, MAROON });
            obstacles.push_back({ { 740.0f + i * 40.0f, groundY - 240.0f, 40.0f, 40.0f }, true, RED, MAROON });
        }

        // Central structure - layered and reinforced
        obstacles.push_back({ { 780.0f, groundY - 120.0f, 40.0f, 40.0f }, true, GREEN, DARKGREEN });
        obstacles.push_back({ { 820.0f, groundY - 120.0f, 40.0f, 40.0f }, true, GREEN, DARKGREEN });
        obstacles.push_back({ { 780.0f, groundY - 160.0f, 40.0f, 40.0f }, true, YELLOW, GOLD });
        obstacles.push_back({ { 820.0f, groundY - 160.0f, 40.0f, 40.0f }, true, YELLOW, GOLD });
        obstacles.push_back({ { 780.0f, groundY - 200.0f, 80.0f, 40.0f }, true, PURPLE, DARKPURPLE });

        // Diagonal reinforcements
        obstacles.push_back({ { 740.0f, groundY - 120.0f, 40.0f, 20.0f }, true, ORANGE, BROWN });
        obstacles.push_back({ { 860.0f, groundY - 120.0f, 40.0f, 20.0f }, true, ORANGE, BROWN });
        obstacles.push_back({ { 740.0f, groundY - 200.0f, 40.0f, 20.0f }, true, ORANGE, BROWN });
        obstacles.push_back({ { 860.0f, groundY - 200.0f, 40.0f, 20.0f }, true, ORANGE, BROWN });

        // Floating defensive platforms
        obstacles.push_back({ { 650.0f, groundY - 140.0f, 50.0f, 15.0f }, true, SKYBLUE, BLUE });
        obstacles.push_back({ { 650.0f, groundY - 170.0f, 30.0f, 30.0f }, true, SKYBLUE, BLUE });

        obstacles.push_back({ { 950.0f, groundY - 140.0f, 50.0f, 15.0f }, true, SKYBLUE, BLUE });
        obstacles.push_back({ { 960.0f, groundY - 170.0f, 30.0f, 30.0f }, true, SKYBLUE, BLUE });

        // High-value targets in difficult positions
        obstacles.push_back({ { 800.0f, groundY - 320.0f, 40.0f, 40.0f }, true, GOLD, ORANGE });

        initialized = true;
    }
};

// --- Additional difficulty modifications ---

// Add a new, even more challenging level
class Level4 : public Level {
public:
    Level4() : Level("Ultimate Challenge", 400) {}

    void Initialize(float groundY) override {
        obstacles.clear();

        // Heavily protected multi-layered structure with maze-like components

        // Outer defensive perimeter - thick walls
        for (int i = 0; i < 10; ++i) {
            obstacles.push_back({ { 600.0f + i * 80.0f, groundY - 40.0f, 40.0f, 40.0f }, true, GRAY, BLACK });
        }

        // Defensive towers
        for (int i = 0; i < 6; ++i) {
            obstacles.push_back({ { 600.0f, groundY - 40.0f - i * 40.0f, 40.0f, 40.0f }, true, BLUE, DARKBLUE });
            obstacles.push_back({ { 1000.0f, groundY - 40.0f - i * 40.0f, 40.0f, 40.0f }, true, BLUE, DARKBLUE });
        }

        // First inner wall
        for (int i = 0; i < 8; ++i) {
            obstacles.push_back({ { 640.0f + i * 40.0f, groundY - 80.0f, 40.0f, 40.0f }, true, RED, MAROON });
        }

        // Second inner wall (offset)
        for (int i = 0; i < 8; ++i) {
            if (i == 3 || i == 4) continue; // gap in the middle
            obstacles.push_back({ { 640.0f + i * 40.0f, groundY - 160.0f, 40.0f, 40.0f }, true, GREEN, DARKGREEN });
        }

        // Third inner wall (alternating pattern)
        for (int i = 0; i < 8; ++i) {
            if (i % 2 == 0) continue; // skip even indices for alternating pattern
            obstacles.push_back({ { 640.0f + i * 40.0f, groundY - 240.0f, 40.0f, 40.0f }, true, YELLOW, GOLD });
        }

        // Internal structure - box within a box
        for (int i = 0; i < 6; ++i) {
            // Outer box
            if (i < 5) obstacles.push_back({ { 680.0f + i * 40.0f, groundY - 120.0f, 40.0f, 40.0f }, true, PURPLE, DARKPURPLE });
            if (i < 5) obstacles.push_back({ { 680.0f + i * 40.0f, groundY - 280.0f, 40.0f, 40.0f }, true, PURPLE, DARKPURPLE });
            if (i > 0 && i < 4) obstacles.push_back({ { 680.0f, groundY - 120.0f - i * 40.0f, 40.0f, 40.0f }, true, PURPLE, DARKPURPLE });
            if (i > 0 && i < 4) obstacles.push_back({ { 840.0f, groundY - 120.0f - i * 40.0f, 40.0f, 40.0f }, true, PURPLE, DARKPURPLE });

            // Inner box
            if (i < 3) obstacles.push_back({ { 720.0f + i * 40.0f, groundY - 160.0f, 40.0f, 40.0f }, true, SKYBLUE, BLUE });
            if (i < 3) obstacles.push_back({ { 720.0f + i * 40.0f, groundY - 240.0f, 40.0f, 40.0f }, true, SKYBLUE, BLUE });
            if (i > 0 && i < 2) obstacles.push_back({ { 720.0f, groundY - 160.0f - i * 40.0f, 40.0f, 40.0f }, true, SKYBLUE, BLUE });
            if (i > 0 && i < 2) obstacles.push_back({ { 800.0f, groundY - 160.0f - i * 40.0f, 40.0f, 40.0f }, true, SKYBLUE, BLUE });
        }

        // High value target in the center
        obstacles.push_back({ { 760.0f, groundY - 200.0f, 40.0f, 40.0f }, true, GOLD, ORANGE });

        // Floating and suspended obstacles
        obstacles.push_back({ { 600.0f, groundY - 160.0f, 40.0f, 10.0f }, true, ORANGE, BROWN });
        obstacles.push_back({ { 600.0f, groundY - 190.0f, 30.0f, 30.0f }, true, ORANGE, BROWN });

        obstacles.push_back({ { 1000.0f, groundY - 160.0f, 40.0f, 10.0f }, true, ORANGE, BROWN });
        obstacles.push_back({ { 1000.0f, groundY - 190.0f, 30.0f, 30.0f }, true, ORANGE, BROWN });

        // Suspended platform with high-value target
        obstacles.push_back({ { 760.0f, groundY - 320.0f, 120.0f, 10.0f }, true, DARKGRAY, BLACK });
        obstacles.push_back({ { 800.0f, groundY - 350.0f, 40.0f, 30.0f }, true, GOLD, ORANGE });

        initialized = true;
    }
};

class GameWorld {
public:
    Ball ball;
    Level* currentLevel = nullptr;
    Level1 level1;
    Level2 level2;
    Level3 level3;
    Level4 level4;
    Ball* selectedBall = nullptr;
    float xStart = 200, yStart = 0;
    bool launched = false;
    float launchAngle = 0;
    double relativeAngle = 0;
    int launchDistance = 0;
    int xOffset = 0, yOffset = 0;
    Texture2D staringTexture{}, surprisedTexture{}, launchedTexture{};
    Texture2D levelBackgroundTexture{};
    bool initialized = false;
    int currentLevelIndex = 1;
    int totalScore = 0;
    int attempts = 3; // Number of attempts per level

    void Init() {
        if (initialized) return;

        staringTexture = LoadTexture("resources/meStaring.png");
        surprisedTexture = LoadTexture("resources/meSurprised.png");
        launchedTexture = LoadTexture("resources/meLaunched.png");
        levelBackgroundTexture = LoadTexture("graphics/level_image.png");

        yStart = GetScreenHeight() - 200;

        ball.pos = { xStart, yStart };
        ball.vel = { 50, -50 };
        ball.radius = 40;
        ball.friction = 0.99f;
        ball.elasticity = 0.9f;
        ball.rotationAngle = 0;
        ball.staringTexture = staringTexture;
        ball.surprisedTexture = surprisedTexture;
        ball.launchedTexture = launchedTexture;

        float groundY = GetScreenHeight() - 40;

        // Initialize all levels
        level1.Initialize(groundY);
        level2.Initialize(groundY);
        level3.Initialize(groundY);
        level4.Initialize(groundY);

        // Set the current level
        SetLevel(1);

        initialized = true;
    }

    void SetLevel(int levelNum) {
        Reset(); // Reset ball position and other game state

        switch (levelNum) {
        case 1:
            currentLevel = &level1;
            break;
        case 2:
            currentLevel = &level2;
            break;
        case 3:
            currentLevel = &level3;
            break;
        case 4:
            currentLevel = &level4;
            break;
        default:
            currentLevel = &level1;
            break;
        }

        currentLevelIndex = levelNum;
        attempts = 3; // Reset attempts for the new level
        currentLevel->Reset();
    }

    void Destroy() {
        UnloadTexture(staringTexture);
        UnloadTexture(surprisedTexture);
        UnloadTexture(launchedTexture);
        UnloadTexture(levelBackgroundTexture);
        initialized = false;
    }

    void Update() {
        if (currentLevel->state == LevelState::COMPLETED) {
            // Level completed logic
            if (currentLevelIndex < 4) {
                // Advance to next level after 2 seconds
                static float completionTime = 0;
                completionTime += GetFrameTime();

                if (completionTime > 2.0f) {
                    SetLevel(currentLevelIndex + 1);
                    completionTime = 0;
                }
            }
            return;
        }

        if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
            Vector2 mousePos = GetMousePosition();
            if (CheckCollisionPointCircle(mousePos, ball.pos, ball.radius)) {
                selectedBall = &ball;
                xOffset = mousePos.x - ball.pos.x;
                yOffset = mousePos.y - ball.pos.y;
            }
        }

        if (IsMouseButtonDown(MOUSE_BUTTON_LEFT) && selectedBall) {
            Vector2 mousePos = GetMousePosition();
            ball.pos.x = mousePos.x - xOffset;
            ball.pos.y = mousePos.y - yOffset;

            int dx = xStart - ball.pos.x;
            int dy = yStart - ball.pos.y;

            launchDistance = std::sqrt(dx * dx + dy * dy);
            relativeAngle = atan2(dy, dx) + PI;
            launchAngle = PI - relativeAngle;

            if (launchDistance > LAUNCH_MAX_DISTANCE) {
                ball.pos.x = xStart + std::cos(relativeAngle) * LAUNCH_MAX_DISTANCE;
                ball.pos.y = yStart + std::sin(relativeAngle) * LAUNCH_MAX_DISTANCE;
            }

            float vx = std::abs(ball.pos.x - xStart) / LAUNCH_MAX_DISTANCE;
            float vy = -std::abs(ball.pos.y - yStart) / LAUNCH_MAX_DISTANCE;

            ball.vel.x = vx * std::cos(launchAngle) * VELOCITY_MULTIPLIER;
            ball.vel.y = vy * std::sin(launchAngle) * VELOCITY_MULTIPLIER;
        }

        if (IsMouseButtonReleased(MOUSE_BUTTON_LEFT)) {
            if (selectedBall && (ball.pos.x != xStart || ball.pos.y != yStart)) {
                launched = true;
                attempts--;
            }
            selectedBall = nullptr;
        }

        if (launched) {
            bool hitAny = false;

            for (auto& obs : currentLevel->obstacles) {
                if (ball.CollidesWith(obs)) {
                    if (obs.visible) {
                        hitAny = true;
                        obs.visible = false;
                        ball.vel.x *= ball.elasticity;
                    }
                }
            }

            if (hitAny) {
                // Update the score and check if level is completed
                currentLevel->Update();
                totalScore = level1.GetCurrentScore() + level2.GetCurrentScore() + level3.GetCurrentScore() + level4.GetCurrentScore();
            }

            if (ball.pos.y + ball.radius > GetScreenHeight()) {
                ball.pos.y = GetScreenHeight() - ball.radius;
                ball.vel.y *= -ball.elasticity;
            }

            ball.pos.x += ball.vel.x;
            ball.pos.y += ball.vel.y;
            ball.vel.y += GRAVITY;

            ball.rotationAngle += 5;
            ball.vel.x *= ball.friction;
            ball.vel.y *= ball.friction;

            // Check if ball has stopped moving
            if (fabs(ball.vel.x) < 0.1f && fabs(ball.vel.y) < 0.1f && ball.pos.y > GetScreenHeight() - ball.radius - 1) {
                // Reset ball if no more attempts left
                if (attempts <= 0) {
                    // Check if level is completed
                    if (currentLevel->state != LevelState::COMPLETED) {
                        currentLevel->state = LevelState::FAILED;
                    }
                }

                // Only reset the ball position, not the obstacles
                ball.pos = { xStart, yStart };
                ball.vel = { 50, -50 };
                ball.rotationAngle = 0;
                launched = false;
            }
        }

        if (IsMouseButtonPressed(MOUSE_BUTTON_RIGHT) || IsKeyPressed(KEY_SPACE)) {
            Reset();
            currentLevel->Reset();
            attempts = 3;
        }

        // Level selection keys
        if (IsKeyPressed(KEY_ONE)) {
            SetLevel(1);
        }
        else if (IsKeyPressed(KEY_TWO)) {
            SetLevel(2);
        }
        else if (IsKeyPressed(KEY_THREE)) {
            SetLevel(3);
        }
        else if (IsKeyPressed(KEY_FOUR)) {
            SetLevel(4);
        }
    }

    void Reset() {
        ball.pos = { xStart, yStart };
        ball.vel = { 50, -50 };
        ball.rotationAngle = 0;
        launched = false;
        selectedBall = nullptr;
    }

    void Draw() {
        /*
        ClearBackground({ 135, 206, 235, 255 }); // Sky blue

        // Sun
        DrawCircle(100, 100, 50, YELLOW);

        // Clouds
        DrawCloud(300, 80);
        DrawCloud(600, 120, 2);
        DrawCloud(950, 60);

        // Distant hills
        DrawCircle(GetScreenWidth() / 3, GetScreenHeight(), 200, GREEN);
        DrawCircle(GetScreenWidth() * 2 / 3, GetScreenHeight(), 250, DARKGREEN);

        // Ground platform
        DrawRectangle(0, GetScreenHeight() - 40, GetScreenWidth(), 40, { 34, 139, 34, 255 }); // Forest green

        */
        // ---- START: CORRECTED BACKGROUND DRAWING ----
        ClearBackground(RAYWHITE); // Clear to a default color first (optional)

        // Check if the texture (loaded in Init) is valid before drawing
        if (levelBackgroundTexture.id > 0) {
            // Draw the texture that was loaded ONCE in the Init() function
            // Use DrawTexturePro to stretch it to the screen size
            DrawTexturePro(levelBackgroundTexture,
                { 0.0f, 0.0f, (float)levelBackgroundTexture.width, (float)levelBackgroundTexture.height }, // Source rect (entire texture)
                { 0.0f, 0.0f, (float)GetScreenWidth(), (float)GetScreenHeight() }, // Destination rect (entire screen)
                { 0, 0 }, // Origin (top-left)
                0.0f,     // Rotation
                WHITE);   // Tint color

            // OR, if you don't want stretching, use this instead:
            // DrawTexture(levelBackgroundTexture, 0, 0, WHITE);

        }
        else {
            // Fallback if texture failed to load in Init(): Draw a simple background
            ClearBackground(DARKGRAY); // Or any other color
            DrawText("Failed to load background texture!", 10, GetScreenHeight() / 2, 20, RED);
        }
        // ---- END: CORRECTED BACKGROUND DRAWING ----


        // Slingshot stand
        DrawRectangle(xStart - 10, yStart - ball.radius - 10, 20, ball.radius * 2 + 130, { 100, 100, 100, 200 });

        // Ball and obstacles
        ball.Draw(launched, selectedBall, xStart, yStart);
        for (const auto& obs : currentLevel->obstacles) obs.Draw();

        // Draw HUD
        DrawRectangle(0, 0, GetScreenWidth(), 50, { 0, 0, 0, 120 });

        // Draw level info
        DrawText(TextFormat("Level %d: %s", currentLevelIndex, currentLevel->name.c_str()), 10, 10, 20, WHITE);
        DrawText(TextFormat("Score: %d/%d", currentLevel->GetCurrentScore(), currentLevel->targetScore), 400, 10, 20, WHITE);
        DrawText(TextFormat("Total Score: %d", totalScore), 600, 10, 20, WHITE);
        DrawText(TextFormat("Attempts: %d", attempts), 800, 10, 20, WHITE);

        // Draw level status message
        if (currentLevel->state == LevelState::COMPLETED) {
            const char* message = "LEVEL COMPLETED!";
            int fontSize = 40;
            int textWidth = MeasureText(message, fontSize);
            DrawRectangle((GetScreenWidth() - textWidth) / 2 - 10, GetScreenHeight() / 2 - 30, textWidth + 20, 60, { 0, 0, 0, 200 });
            DrawText(message, (GetScreenWidth() - textWidth) / 2, GetScreenHeight() / 2 - 20, fontSize, GREEN);

            if (currentLevelIndex < 3) {
                const char* nextMessage = "Next level loading...";
                int nextFontSize = 20;
                int nextTextWidth = MeasureText(nextMessage, nextFontSize);
                DrawText(nextMessage, (GetScreenWidth() - nextTextWidth) / 2, GetScreenHeight() / 2 + 30, nextFontSize, WHITE);
            }
            else {
                const char* finalMessage = "Congratulations! You completed all levels!";
                int finalFontSize = 20;
                int finalTextWidth = MeasureText(finalMessage, finalFontSize);
                DrawText(finalMessage, (GetScreenWidth() - finalTextWidth) / 2, GetScreenHeight() / 2 + 30, finalFontSize, WHITE);
            }
        }
        else if (currentLevel->state == LevelState::FAILED && attempts <= 0) {
            const char* message = "NO ATTEMPTS LEFT!";
            int fontSize = 40;
            int textWidth = MeasureText(message, fontSize);
            DrawRectangle((GetScreenWidth() - textWidth) / 2 - 10, GetScreenHeight() / 2 - 30, textWidth + 20, 60, { 0, 0, 0, 200 });
            DrawText(message, (GetScreenWidth() - textWidth) / 2, GetScreenHeight() / 2 - 20, fontSize, RED);

            const char* retryMessage = "Press SPACE to retry";
            int retryFontSize = 20;
            int retryTextWidth = MeasureText(retryMessage, retryFontSize);
            DrawText(retryMessage, (GetScreenWidth() - retryTextWidth) / 2, GetScreenHeight() / 2 + 30, retryFontSize, WHITE);
        }

        // Draw controls
        DrawText("Controls: 1,2,3,4 - Select Level | SPACE - Reset | ESC - Menu", 10, GetScreenHeight() - 30, 20, WHITE);
    }
};

enum GameState {
    MENU,
    PLAYING,
    LEVEL_SELECT,
    EXIT_GAME
};

int main()
{
    const int screenWidth = 1280;
    const int screenHeight = 720;

    InitWindow(screenWidth, screenHeight, "Angry Me :D - Multi-Level Edition");
    SetTargetFPS(60);

    // --- Menu Assets ---
    Texture2D background = LoadTexture("graphics/start_image.png");

    float buttonScale = 0.65f;

    Image startImage = LoadImage("graphics/start_button.png");
    Image exitImage = LoadImage("graphics/exit_button.png");

    int startButtonWidth = static_cast<int>(startImage.width * buttonScale);
    int startButtonHeight = static_cast<int>(startImage.height * buttonScale);

    int exitButtonWidth = static_cast<int>(exitImage.width * buttonScale);
    int exitButtonHeight = static_cast<int>(exitImage.height * buttonScale);

    UnloadImage(startImage);
    UnloadImage(exitImage);

    float centerX_start = (screenWidth - startButtonWidth) / 2.0f;
    float centerX_exit = (screenWidth - exitButtonWidth) / 2.0f;

    float startButtonY = screenHeight / 2.0f - 50;
    float exitButtonY = startButtonY + startButtonHeight + 20;

    Button startButton("graphics/start_button.png", { centerX_start, startButtonY }, buttonScale);
    Button exitButton("graphics/exit_button.png", { centerX_exit, exitButtonY }, buttonScale);

    // --- Game World ---
    GameWorld game;
    GameState state = MENU;

    // Define title variables outside the switch
    const char* title = "Angry Birds";
    int fontSize = 60;

    while (!WindowShouldClose())
    {
        Vector2 mousePosition = GetMousePosition();

        // --- Update depending on state ---
        switch (state) {
        case MENU: {
            // Check if the Start button is clicked
            if (startButton.isClicked(mousePosition)) {
                game.Init();
                state = PLAYING;
            }

            // Check if the Exit button is clicked
            if (exitButton.isClicked(mousePosition)) {
                state = EXIT_GAME;
            }
            break;
        }

        case PLAYING: {
            game.Update();

            // Add option to return to menu
            if (IsKeyPressed(KEY_ESCAPE)) {
                state = MENU;
            }
            break;
        }

        case EXIT_GAME:
            // Just to ensure proper exit
            break;
        }

        // --- Drawing ---
        BeginDrawing();

        switch (state) {
        case MENU: {
            DrawTexture(background, 0, 0, WHITE);

            // --- Draw "Angry Birds" title with bouncing effect ---
            int textWidth = MeasureText(title, fontSize);
            int titleX = (screenWidth - textWidth) / 2;

            // Bouncing effect
            float time = GetTime(); // returns seconds since start
            float bounce = sinf(time * 2.0f) * 10.0f;
            int titleY = static_cast<int>(startButtonY) - 100 + static_cast<int>(bounce);

            DrawText(title, titleX + 2, titleY + 2, fontSize, DARKGRAY); // shadow
            DrawText(title, titleX, titleY, fontSize, BLACK);            // main text

            startButton.Draw();
            exitButton.Draw();
            break;
        }

        case PLAYING: {
            game.Draw();
            break;
        }

        case EXIT_GAME:
            // No drawing needed for exit
            break;
        }

        EndDrawing();

        // Exit the game if the Exit button is pressed
        if (state == EXIT_GAME) break;
    }

    // Cleanup game resources
    if (game.initialized) {
        game.Destroy();
    }

    UnloadTexture(background);
    CloseWindow();
    return 0;
}