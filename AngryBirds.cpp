#include "raylib.h"
#include <cmath>
#include <array>
#include <vector>

constexpr int MAX_OBSTACLES = 10;
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

class GameWorld {
public:
    Ball ball;
    std::vector<Obstacle> obstacles;
    Ball* selectedBall = nullptr;
    float xStart = 200, yStart = 200;
    bool launched = false;
    float launchAngle = 0;
    double relativeAngle = 0;
    int launchDistance = 0;
    int xOffset = 0, yOffset = 0;
    Texture2D staringTexture{}, surprisedTexture{}, launchedTexture{};

    void Init() {
        staringTexture = LoadTexture("resources/meStaring.png");
        surprisedTexture = LoadTexture("resources/meSurprised.png");
        launchedTexture = LoadTexture("resources/meLaunched.png");

        ball.pos = { xStart, yStart };
        ball.vel = { 50, -50 };
        ball.radius = 40;
        ball.friction = 0.99f;
        ball.elasticity = 0.9f;
        ball.rotationAngle = 0;
        ball.staringTexture = staringTexture;
        ball.surprisedTexture = surprisedTexture;
        ball.launchedTexture = launchedTexture;

        for (int i = 0; i < 3; ++i) {
            obstacles.push_back({ { 550.0f, 100.0f + 130.0f * i, 30.0f, 100.0f }, true, GREEN, DARKGREEN });
            obstacles.push_back({ { 700.0f, 100.0f + 130 * i, 30, 100 }, true, YELLOW, GOLD });
            obstacles.push_back({ { 550.0f, 70.0f + 130 * i, 180, 30 }, true, RED, MAROON });
        }
    }

    void Destroy() {
        UnloadTexture(staringTexture);
        UnloadTexture(surprisedTexture);
        UnloadTexture(launchedTexture);
    }

    void Update() {
        if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
            if (CheckCollisionPointCircle(GetMousePosition(), ball.pos, ball.radius)) {
                selectedBall = &ball;
                xOffset = GetMouseX() - ball.pos.x;
                yOffset = GetMouseY() - ball.pos.y;
            }
        }

        if (IsMouseButtonDown(MOUSE_BUTTON_LEFT) && selectedBall) {
            ball.pos.x = GetMouseX() - xOffset;
            ball.pos.y = GetMouseY() - yOffset;

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
            }
            selectedBall = nullptr;
        }

        if (launched) {
            for (auto& obs : obstacles) {
                if (ball.CollidesWith(obs)) {
                    obs.visible = false;
                    ball.vel.x *= ball.elasticity;
                }
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
        }

        if (IsMouseButtonPressed(MOUSE_BUTTON_RIGHT) || IsKeyPressed(KEY_SPACE)) {
            Reset();
        }
    }

    void Reset() {
        for (auto& obs : obstacles) obs.visible = true;
        ball.pos = { xStart, yStart };
        ball.rotationAngle = 0;
        launched = false;
        selectedBall = nullptr;
    }

    void Draw() {
        BeginDrawing();
        ClearBackground(WHITE);

        ball.Draw(launched, selectedBall, xStart, yStart);
        for (const auto& obs : obstacles) obs.Draw();

        DrawRectangle(xStart - 10, yStart - ball.radius - 10, 20, 400, { 100, 100, 100, 200 });

        EndDrawing();
    }
};

int main() {
    const int screenWidth = 800;
    const int screenHeight = 450;

    SetConfigFlags(FLAG_MSAA_4X_HINT);
    InitWindow(screenWidth, screenHeight, "Angry Me :D - Prof. Dr. David Buzatto");
    SetTargetFPS(60);

    GameWorld game;
    game.Init();

    while (!WindowShouldClose()) {
        game.Update();
        game.Draw();
    }

    game.Destroy();
    CloseWindow();
    return 0;
}
