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

class GameWorld {
public:
    Ball ball;
    std::vector<Obstacle> obstacles;
    Ball* selectedBall = nullptr;
    float xStart = 200, yStart = 0;
    bool launched = false;
    float launchAngle = 0;
    double relativeAngle = 0;
    int launchDistance = 0;
    int xOffset = 0, yOffset = 0;
    Texture2D staringTexture{}, surprisedTexture{}, launchedTexture{};
    bool initialized = false;

    void Init() {
        if (initialized) return;

        staringTexture = LoadTexture("resources/meStaring.png");
        surprisedTexture = LoadTexture("resources/meSurprised.png");
        launchedTexture = LoadTexture("resources/meLaunched.png");

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

        // Clear any existing obstacles
        obstacles.clear();

        // 1. Base level: wider foundation
        for (int i = 0; i < 7; ++i) {
            obstacles.push_back({ { 750.0f + i * 40.0f, groundY - 100.0f, 30.0f, 100.0f }, true, GREEN, DARKGREEN });
        }

        // 2. Middle level: vertical blocks
        for (int i = 0; i < 3; ++i) {
            obstacles.push_back({ { 770.0f + i * 40.0f, groundY - 100.0f, 30.0f, 80.0f }, true, YELLOW, GOLD });
        }

        // 3. Horizontal planks
        obstacles.push_back({ { 750, groundY - 100, 250, 20 }, true, RED, MAROON });
        obstacles.push_back({ { 770, groundY - 210, 120, 20 }, true, RED, MAROON });

        // 4. Top vertical blocks
        obstacles.push_back({ { 790, groundY - 260, 30, 60 }, true, BLUE, DARKBLUE });
        obstacles.push_back({ { 840, groundY - 260, 30, 60 }, true, BLUE, DARKBLUE });

        // 5. Roof plank
        obstacles.push_back({ { 790, groundY - 280, 80, 20 }, true, PURPLE, DARKPURPLE });

        Reset();
        initialized = true;
    }

    void Destroy() {
        UnloadTexture(staringTexture);
        UnloadTexture(surprisedTexture);
        UnloadTexture(launchedTexture);
        initialized = false;
    }

    void Update() {
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

        // Slingshot stand
        DrawRectangle(xStart - 10, yStart - ball.radius - 10, 20, ball.radius * 2 + 130, { 100, 100, 100, 200 });

        // Ball and obstacles
        ball.Draw(launched, selectedBall, xStart, yStart);
        for (const auto& obs : obstacles) obs.Draw();
    }
};

enum GameState {
    MENU,
    PLAYING,
    EXIT_GAME
};

int main()
{
    const int screenWidth = 1280;
    const int screenHeight = 720;

    InitWindow(screenWidth, screenHeight, "Angry Me :D - Prof. Dr. David Buzatto");
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

            // Display instructions
            DrawText("Right-Click or SPACE to Reset | ESC to Menu", 10, 10, 20, WHITE);
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