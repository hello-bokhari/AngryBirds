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
    Texture2D splitTexture{}; // New texture for split birds
    Color fillColor = BLUE;
    Color strokeColor = DARKBLUE;
    bool isSplit = false; // Flag to indicate if this is a split ball
    bool isActive = true; // Flag to indicate if the ball is active

    void Draw(bool launched, Ball* selectedBall, float xStart, float yStart) const {
        if (!isActive) return; // Don't draw if not active

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
        else if (launched) {
            textureToDraw = isSplit ? splitTexture : launchedTexture;
        }

        // Adjust size if it's a split ball
        float drawRadius = isSplit ? radius * 0.7f : radius;

        DrawTexturePro(
            textureToDraw,
            { 0, 0, 420, 420 },
            { pos.x, pos.y, drawRadius * 2, drawRadius * 2 },
            { (float)drawRadius, (float)drawRadius },
            rotationAngle,
            WHITE
        );
    }

    float GetProbePosition(bool forX, int probeIndex) {
        float p = forX ? pos.x : pos.y;
        float angleRad = toRadians(collisionProbes[probeIndex % PROBE_QUANTITY]);
        float t = forX ? cosf(angleRad) : sinf(angleRad);
        float probeRadius = isSplit ? radius * 0.7f : radius; // Adjust collision radius if split
        return p + probeRadius * ((probeIndex / PROBE_QUANTITY) > 0 ? 0.5f : 1.0f) * t;
    }

    bool CollidesWith(const Obstacle& obs) {
        if (!obs.visible || !isActive) return false; // Don't check collision if not active

        for (int i = 0; i < PROBE_QUANTITY * 2; ++i) {
            Vector2 point = { GetProbePosition(true, i), GetProbePosition(false, i) };
            if (CheckCollisionPointRec(point, obs.rect)) {
                return true;
            }
        }

        return false;
    }

    // Create a split version of this ball
    Ball CreateSplitBall(float angleOffset) const {
        Ball splitBall = *this; // Copy current ball properties

        // Adjust properties for split ball
        splitBall.isSplit = true;
        splitBall.radius *= 0.7f; // Make split balls smaller

        // Adjust velocity based on the angle offset
        float currentAngle = atan2f(vel.y, vel.x);
        float newAngle = currentAngle + toRadians(angleOffset);
        float speed = sqrtf(vel.x * vel.x + vel.y * vel.y);

        splitBall.vel.x = cosf(newAngle) * speed;
        splitBall.vel.y = sinf(newAngle) * speed;

        return splitBall;
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
    float scale;
    bool wasPressed;

public:
    Vector2 position;  // Made public for easier access in the level selection menu

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
        this->scale = scale;
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

    int getWidth() const {
        return texture.width;
    }

    int getHeight() const {
        return texture.height;
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
        for (int i = 0; i < 5; ++i) {
            obstacles.push_back({ { 760.0f + i * 35.0f, groundY - 80.0f, 30.0f, 40.0f }, true, YELLOW, GOLD });
        }

        // Third row - narrower
        for (int i = 0; i < 4; ++i) {
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
    Level3() : Level("Stronghold", 250) {}  // Increased target score from 200 to 300

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

class Level4 : public Level {
public:
    Level4() : Level("Ultimate Challenge", 250) {}

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
    std::vector<Ball> splitBalls; // Vector to store split balls
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
    Texture2D staringTexture{}, surprisedTexture{}, launchedTexture{}, splitTexture{};
    Texture2D levelBackgroundTexture{};
    Texture2D powerupButtonTexture{}; // Texture for power-up button
    bool initialized = false;
    int currentLevelIndex = 1;
    int totalScore = 0;
    int attempts = 3; // Number of attempts per level

    // Power-up system
    int powerupCost = 50; // Cost of using the split power-up
    bool canUsePowerup = false; // Flag to indicate if power-up can be used
    bool powerupActive = false; // Flag to indicate if power-up is active
    Rectangle powerupButton; // Power-up button rectangle

    void Init() {
        if (initialized) return;

        staringTexture = LoadTexture("resources/meStaring.png");
        surprisedTexture = LoadTexture("resources/meSurprised.png");
        launchedTexture = LoadTexture("resources/meLaunched.png");
        splitTexture = LoadTexture("resources/meSplit.png"); // Load new texture for split birds
        levelBackgroundTexture = LoadTexture("graphics/level_image.png");
        powerupButtonTexture = LoadTexture("graphics/powerup_button.png"); // Load power-up button texture

        // If the splitTexture fails to load, use the launchedTexture as fallback
        if (splitTexture.id == 0) {
            splitTexture = launchedTexture;
        }

        // Initialize power-up button position and size
        powerupButton = { GetScreenWidth() - 150.0f, 60.0f, 100.0f, 40.0f };

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
        ball.splitTexture = splitTexture;

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
        UnloadTexture(splitTexture);
        UnloadTexture(levelBackgroundTexture);
        UnloadTexture(powerupButtonTexture);
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

        // Update total score
        totalScore = level1.GetCurrentScore() + level2.GetCurrentScore() + level3.GetCurrentScore() + level4.GetCurrentScore();

        // Check if player can afford the power-up
        canUsePowerup = totalScore >= powerupCost;

        // Check for power-up button click
        if (launched && canUsePowerup && !powerupActive && !ball.isSplit) {
            Vector2 mousePos = GetMousePosition();
            if (CheckCollisionPointRec(mousePos, powerupButton) && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
                // Activate power-up
                ActivateSplitPowerup();
            }
        }

        if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
            Vector2 mousePos = GetMousePosition();
            if (CheckCollisionPointCircle(mousePos, ball.pos, ball.radius) && !launched) {
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
                powerupActive = false;
            }
            selectedBall = nullptr;
        }

        if (launched) {
            // Update main ball
            if (ball.isActive) {
                UpdateBall(ball);
            }

            // Update split balls
            for (auto& splitBall : splitBalls) {
                if (splitBall.isActive) {
                    UpdateBall(splitBall);
                }
            }

            // Check if all balls have stopped
            bool allBallsStopped = !ball.isActive;
            for (const auto& splitBall : splitBalls) {
                if (splitBall.isActive) {
                    allBallsStopped = false;
                    break;
                }
            }

            if (allBallsStopped) {
                // Reset if no more attempts left
                if (attempts <= 0) {
                    // Check if level is completed
                    if (currentLevel->state != LevelState::COMPLETED) {
                        currentLevel->state = LevelState::FAILED;
                    }
                }

                // Only reset the ball position, not the obstacles
                ResetBalls();
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

    void UpdateBall(Ball& currentBall) {
        bool hitAny = false;

        for (auto& obs : currentLevel->obstacles) {
            if (currentBall.CollidesWith(obs)) {
                if (obs.visible) {
                    hitAny = true;
                    obs.visible = false;
                    currentBall.vel.x *= currentBall.elasticity;
                }
            }
        }

        if (hitAny) {
            // Update the score and check if level is completed
            currentLevel->Update();
        }

        if (currentBall.pos.y + currentBall.radius > GetScreenHeight()) {
            currentBall.pos.y = GetScreenHeight() - currentBall.radius;
            currentBall.vel.y *= -currentBall.elasticity;
        }

        currentBall.pos.x += currentBall.vel.x;
        currentBall.pos.y += currentBall.vel.y;
        currentBall.vel.y += GRAVITY;

        currentBall.rotationAngle += 5;
        currentBall.vel.x *= currentBall.friction;
        currentBall.vel.y *= currentBall.friction;

        // Check if ball has stopped moving
        if (fabs(currentBall.vel.x) < 0.1f && fabs(currentBall.vel.y) < 0.1f &&
            currentBall.pos.y > GetScreenHeight() - currentBall.radius - 1) {
            currentBall.isActive = false;
        }
    }

    void ActivateSplitPowerup() {
        if (!canUsePowerup || !launched || ball.isSplit || splitBalls.size() > 0) return;

        // Deduct the cost
        totalScore -= powerupCost;
        powerupActive = true;

        // Create two split balls with different angles
        splitBalls.push_back(ball.CreateSplitBall(-30.0f)); // Split at -30 degrees
        splitBalls.push_back(ball.CreateSplitBall(30.0f));  // Split at +30 degrees

        // Make the main ball a split ball too
        ball.isSplit = true;
        ball.radius *= 0.7f; // Make the main ball smaller too
    }

    void Reset() {
        ResetBalls();
        splitBalls.clear();
        powerupActive = false;
    }

    void ResetBalls() {
        ball.pos = { xStart, yStart };
        ball.vel = { 50, -50 };
        ball.rotationAngle = 0;
        ball.isSplit = false;
        ball.isActive = true;
        ball.radius = 40; // Reset to original size
        launched = false;
        selectedBall = nullptr;
        splitBalls.clear();
    }

    void Draw() {
        // Draw background
        if (levelBackgroundTexture.id > 0) {
            DrawTexturePro(levelBackgroundTexture,
                { 0.0f, 0.0f, (float)levelBackgroundTexture.width, (float)levelBackgroundTexture.height },
                { 0.0f, 0.0f, (float)GetScreenWidth(), (float)GetScreenHeight() },
                { 0, 0 },
                0.0f,
                WHITE);
        }
        else {
            ClearBackground(DARKGRAY);
            DrawText("Failed to load background texture!", 10, GetScreenHeight() / 2, 20, RED);
        }

        // Slingshot stand
        DrawRectangle(xStart - 10, yStart - ball.radius - 10, 20, ball.radius * 2 + 130, { 100, 100, 100, 200 });

        // Draw obstacles
        for (const auto& obs : currentLevel->obstacles) obs.Draw();

        // Draw split balls
        for (const auto& splitBall : splitBalls) {
            if (splitBall.isActive) {
                splitBall.Draw(launched, nullptr, xStart, yStart);
            }
        }

        // Draw main ball
        if (ball.isActive) {
            ball.Draw(launched, selectedBall, xStart, yStart);
        }

        // Draw HUD
        DrawRectangle(0, 0, GetScreenWidth(), 50, { 0, 0, 0, 120 });

        // Draw level info
        DrawText(TextFormat("Level %d: %s", currentLevelIndex, currentLevel->name.c_str()), 10, 10, 20, WHITE);
        DrawText(TextFormat("Score: %d/%d", currentLevel->GetCurrentScore(), currentLevel->targetScore), 400, 10, 20, WHITE);
        DrawText(TextFormat("Total Score: %d", totalScore), 600, 10, 20, WHITE);
        DrawText(TextFormat("Attempts: %d", attempts), 800, 10, 20, WHITE);

        // Draw power-up button
        if (powerupButtonTexture.id > 0) {
            DrawTexturePro(powerupButtonTexture,
                { 0.0f, 0.0f, (float)powerupButtonTexture.width, (float)powerupButtonTexture.height },
                powerupButton,
                { 0, 0 },
                0.0f,
                canUsePowerup && launched && !powerupActive && !ball.isSplit ? WHITE : GRAY);
        }
        else {
            DrawRectangleRec(powerupButton, canUsePowerup && launched && !powerupActive && !ball.isSplit ? BLUE : DARKGRAY);
            DrawRectangleLinesEx(powerupButton, 2, BLACK);
            DrawText("SPLIT", powerupButton.x + 10, powerupButton.y + 10, 20, WHITE);
        }
        DrawText(TextFormat("Cost: %d", powerupCost), powerupButton.x, powerupButton.y + powerupButton.height + 5, 16, WHITE);

        // Draw level status message
        if (currentLevel->state == LevelState::COMPLETED) {
            const char* message = "LEVEL COMPLETED!";
            int fontSize = 40;
            int textWidth = MeasureText(message, fontSize);
            DrawRectangle((GetScreenWidth() - textWidth) / 2 - 10, GetScreenHeight() / 2 - 30, textWidth + 20, 60, { 0, 0, 0, 200 });
            DrawText(message, (GetScreenWidth() - textWidth) / 2, GetScreenHeight() / 2 - 20, fontSize, GREEN);

            if (currentLevelIndex < 4) {
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

        // Draw controls and power-up instructions
        DrawText("Controls: 1,2,3,4 - Select Level | SPACE - Reset | ESC - Menu", 10, GetScreenHeight() - 30, 20, WHITE);
        DrawText("Click the SPLIT button during flight to activate power-up!", 10, GetScreenHeight() - 60, 20, YELLOW);
    }
};

enum GameState {
    MENU,
    PLAYING,
    LEVEL_SELECT,  // New state for level selection
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
    Texture2D levelSelectBackground = LoadTexture("graphics/level_select_bg.png");  // Add a background for level select

    float buttonScale = 0.65f;

    Image startImage = LoadImage("graphics/start_button.png");
    Image exitImage = LoadImage("graphics/exit_button.png");
    Image backImage = LoadImage("graphics/back_button.png");  // New back button

    // Level button images
    Image level1Image = LoadImage("graphics/level1_button.png");
    Image level2Image = LoadImage("graphics/level2_button.png");
    Image level3Image = LoadImage("graphics/level3_button.png");
    Image level4Image = LoadImage("graphics/level4_button.png");

    int startButtonWidth = static_cast<int>(startImage.width * buttonScale);
    int startButtonHeight = static_cast<int>(startImage.height * buttonScale);

    int exitButtonWidth = static_cast<int>(exitImage.width * buttonScale);
    int exitButtonHeight = static_cast<int>(exitImage.height * buttonScale);

    int backButtonWidth = static_cast<int>(backImage.width * buttonScale);
    int backButtonHeight = static_cast<int>(backImage.height * buttonScale);

    // Level button dimensions
    int levelButtonWidth = static_cast<int>(level1Image.width * buttonScale);
    int levelButtonHeight = static_cast<int>(level1Image.height * buttonScale);

    UnloadImage(startImage);
    UnloadImage(exitImage);
    UnloadImage(backImage);
    UnloadImage(level1Image);
    UnloadImage(level2Image);
    UnloadImage(level3Image);
    UnloadImage(level4Image);

    float centerX_start = (screenWidth - startButtonWidth) / 2.0f;
    float centerX_exit = (screenWidth - exitButtonWidth) / 2.0f;
    float centerX_back = 50.0f;  // Position back button at left side

    float startButtonY = screenHeight / 2.0f - 50;
    float exitButtonY = startButtonY + startButtonHeight + 20;
    float backButtonY = screenHeight - backButtonHeight - 30;  // Position back button at bottom

    // Calculate positions for level buttons
    float levelButtonSpacing = 50.0f;
    float totalLevelButtonsWidth = 4 * levelButtonWidth + 3 * levelButtonSpacing;
    float levelButtonsStartX = (screenWidth - totalLevelButtonsWidth) / 2.0f;
    float levelButtonY = screenHeight / 2.0f - levelButtonHeight / 2.0f;

    Button startButton("graphics/start_button.png", { centerX_start, startButtonY }, buttonScale);
    Button exitButton("graphics/exit_button.png", { centerX_exit, exitButtonY }, buttonScale);
    Button backButton("graphics/back_button.png", { centerX_back, backButtonY }, buttonScale);

    // Level selection buttons
    Button level1Button("graphics/level1_button.png",
        { levelButtonsStartX, levelButtonY }, buttonScale);
    Button level2Button("graphics/level2_button.png",
        { levelButtonsStartX + levelButtonWidth + levelButtonSpacing, levelButtonY }, buttonScale);
    Button level3Button("graphics/level3_button.png",
        { levelButtonsStartX + 2 * (levelButtonWidth + levelButtonSpacing), levelButtonY }, buttonScale);
    Button level4Button("graphics/level4_button.png",
        { levelButtonsStartX + 3 * (levelButtonWidth + levelButtonSpacing), levelButtonY }, buttonScale);

    // --- Game World ---
    GameWorld game;
    GameState state = MENU;

    // Define title variables outside the switch
    const char* title = "Angry Birds";
    const char* levelSelectTitle = "Select Level";
    int fontSize = 60;
    int levelFontSize = 50;

    while (!WindowShouldClose())
    {
        SetExitKey(KEY_NULL);
        Vector2 mousePosition = GetMousePosition();

        // --- Update depending on state ---
        switch (state) {
        case MENU: {
            // Check if the Start button is clicked
            if (startButton.isClicked(mousePosition)) {
                state = LEVEL_SELECT;  // Go to level select instead of playing
            }

            // Check if the Exit button is clicked
            if (exitButton.isClicked(mousePosition)) {
                state = EXIT_GAME;
            }
            break;
        }
        case LEVEL_SELECT: {
            // Initialize game if not already initialized
            if (!game.initialized) {
                game.Init();
            }

            // Check if level buttons are clicked
            if (level1Button.isClicked(mousePosition)) {
                game.SetLevel(1);
                state = PLAYING;
            }
            else if (level2Button.isClicked(mousePosition)) {
                game.SetLevel(2);
                state = PLAYING;
            }
            else if (level3Button.isClicked(mousePosition)) {
                game.SetLevel(3);
                state = PLAYING;
            }
            else if (level4Button.isClicked(mousePosition)) {
                game.SetLevel(4);
                state = PLAYING;
            }

            // Check if the Back button is clicked
            if (backButton.isClicked(mousePosition)) {
                state = MENU;
            }
            break;
        }
        case PLAYING: {
            game.Update();

            // Add option to return to level select menu
            if (IsKeyPressed(KEY_ESCAPE)) {
                state = LEVEL_SELECT;
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
        case LEVEL_SELECT: {
            // Draw level selection background
            if (levelSelectBackground.id > 0) {
                DrawTexturePro(levelSelectBackground,
                    { 0.0f, 0.0f, (float)levelSelectBackground.width, (float)levelSelectBackground.height },
                    { 0.0f, 0.0f, (float)screenWidth, (float)screenHeight },
                    { 0, 0 },
                    0.0f,
                    WHITE);
            }
            else {
                ClearBackground(RAYWHITE);
            }

            // Draw level selection title
            int levelTextWidth = MeasureText(levelSelectTitle, levelFontSize);
            int levelTitleX = (screenWidth - levelTextWidth) / 2;
            int levelTitleY = 100;

            DrawText(levelSelectTitle, levelTitleX + 2, levelTitleY + 2, levelFontSize, DARKGRAY); // shadow
            DrawText(levelSelectTitle, levelTitleX, levelTitleY, levelFontSize, BLACK);            // main text

            // Draw level descriptions
            int descFontSize = 18;
            DrawText("Level 1: Starter Tower",
                (int)level1Button.position.x, (int)(level1Button.position.y + levelButtonHeight + 10),
                descFontSize, BLACK);
            DrawText("Level 2: Fortified Castle",
                (int)level2Button.position.x, (int)(level2Button.position.y + levelButtonHeight + 10),
                descFontSize, BLACK);
            DrawText("Level 3: Stronghold",
                (int)level3Button.position.x, (int)(level3Button.position.y + levelButtonHeight + 10),
                descFontSize, BLACK);
            DrawText("Level 4: Ultimate Challenge",
                (int)level4Button.position.x, (int)(level4Button.position.y + levelButtonHeight + 10),
                descFontSize, BLACK);

            // Draw level selection buttons
            level1Button.Draw();
            level2Button.Draw();
            level3Button.Draw();
            level4Button.Draw();
            backButton.Draw();
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

    // Unload textures
    UnloadTexture(background);
    UnloadTexture(levelSelectBackground);
    CloseWindow();
    return 0;
}