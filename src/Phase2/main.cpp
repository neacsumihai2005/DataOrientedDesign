#include <SDL.h>
#include <vector>
#include <iostream>
#include <string>
#include <random>
#include <ctime>
#include <algorithm>
#include <thread>
#include <functional>

// --- CONSTANTE JOC ---
const int WINDOW_WIDTH = 1280;
const int WINDOW_HEIGHT = 720;
const int MAX_ENTITIES = 20000;

// --- CONSTANTE GRID ---
const int CELL_SIZE = 64;
const int GRID_COLS = (WINDOW_WIDTH / CELL_SIZE) + 1;
const int GRID_ROWS = (WINDOW_HEIGHT / CELL_SIZE) + 1;
const int MAX_CELLS = GRID_COLS * GRID_ROWS;

extern "C" {
    __declspec(dllexport) unsigned long NvOptimusEnablement = 0x00000001;
    __declspec(dllexport) int AmdPowerXpressRequestHighPerformance = 1;
}

float randomFloat(float min, float max) {
    return min + static_cast <float> (rand()) / (static_cast <float> (RAND_MAX / (max - min)));
}

// ==========================================
// 1. COMPONENTE
// ==========================================
struct TransformComponent { float x, y; };
struct VelocityComponent { float vx, vy; };
struct SpriteComponent { bool isVisible; Uint8 r, g, b; int w, h; };
enum EntityType { TYPE_NONE, TYPE_PLAYER, TYPE_ENEMY, TYPE_COIN };
struct ColliderComponent { bool isActive; float radius; EntityType type; };

// ==========================================
// 2. REGISTRY
// ==========================================
class Registry {
public:
    std::vector<TransformComponent> transforms;
    std::vector<VelocityComponent> velocities;
    std::vector<SpriteComponent> sprites;
    std::vector<ColliderComponent> colliders;

    // Lista inlantuita intrusiva (Next Pointer)
    std::vector<int> nextEntity;

    int entityCount = 0;

    void init(int maxEntities) {
        transforms.resize(maxEntities);
        velocities.resize(maxEntities);
        sprites.resize(maxEntities);
        colliders.resize(maxEntities);
        nextEntity.resize(maxEntities);
        entityCount = 0;
    }

    int createEntity() {
        if (entityCount >= transforms.size()) return -1;
        int id = entityCount++;
        colliders[id].isActive = false;
        sprites[id].isVisible = true;
        velocities[id] = { 0, 0 };
        nextEntity[id] = -1;
        return id;
    }

    void destroyEntity(int id) {
        sprites[id].isVisible = false;
        colliders[id].isActive = false;
        transforms[id].x = -10000;
    }
};

// ==========================================
// 3. SISTEME
// ==========================================

class InputSystem {
public:
    void update(Registry& reg, const Uint8* keys, int playerID) {
        float speed = 350.0f;
        reg.velocities[playerID].vx = 0;
        reg.velocities[playerID].vy = 0;

        if (keys[SDL_SCANCODE_W] || keys[SDL_SCANCODE_UP])    reg.velocities[playerID].vy = -speed;
        if (keys[SDL_SCANCODE_S] || keys[SDL_SCANCODE_DOWN])  reg.velocities[playerID].vy = speed;
        if (keys[SDL_SCANCODE_A] || keys[SDL_SCANCODE_LEFT])  reg.velocities[playerID].vx = -speed;
        if (keys[SDL_SCANCODE_D] || keys[SDL_SCANCODE_RIGHT]) reg.velocities[playerID].vx = speed;
    }
};

// --- PHYSICS SYSTEM MULTI-THREADED ---
class PhysicsSystem {
public:
    void processChunk(Registry& reg, float dt, int start, int end) {
        for (int i = start; i < end; i++) {
            if (!reg.sprites[i].isVisible) continue;

            // Jiggle (Tremurat) - doar la monede
            if (reg.colliders[i].type == TYPE_COIN) {
                reg.transforms[i].x += randomFloat(-1.0f, 1.0f);
                reg.transforms[i].y += randomFloat(-1.0f, 1.0f);
            }

            // Move
            reg.transforms[i].x += reg.velocities[i].vx * dt;
            reg.transforms[i].y += reg.velocities[i].vy * dt;

            // Bounce (Pereti)
            if (reg.velocities[i].vx != 0 || reg.velocities[i].vy != 0) {
                if (reg.transforms[i].x <= 0 || reg.transforms[i].x >= WINDOW_WIDTH - reg.sprites[i].w) reg.velocities[i].vx *= -1;
                if (reg.transforms[i].y <= 0 || reg.transforms[i].y >= WINDOW_HEIGHT - reg.sprites[i].h) reg.velocities[i].vy *= -1;
            }
        }
    }

    void update(Registry& reg, float dt) {
        unsigned int numThreads = std::thread::hardware_concurrency();
        if (numThreads == 0) numThreads = 2;

        std::vector<std::thread> threads;
        int count = reg.entityCount;
        int chunkSize = count / numThreads;

        for (unsigned int i = 0; i < numThreads; i++) {
            int start = i * chunkSize;
            int end = (i == numThreads - 1) ? count : (i + 1) * chunkSize;
            threads.emplace_back(&PhysicsSystem::processChunk, this, std::ref(reg), dt, start, end);
        }

        for (auto& t : threads) t.join();
    }
};

// --- GAMEPLAY SYSTEM (Grid + Coin Physics + Heatmap Data) ---
class GameplaySystem {
private:
    int gridHead[MAX_CELLS];
    int cellCounts[MAX_CELLS]; // Pentru vizualizare (Heatmap)

public:
    int score = 0;
    bool gameOver = false;

    // Getter pentru RenderSystem
    int getCountInCell(int col, int row) const {
        if (col < 0 || col >= GRID_COLS || row < 0 || row >= GRID_ROWS) return 0;
        return cellCounts[row * GRID_COLS + col];
    }

    void update(Registry& reg, int playerID) {
        if (gameOver) return;

        // 1. CLEAR
        std::fill(std::begin(gridHead), std::end(gridHead), -1);
        std::fill(std::begin(cellCounts), std::end(cellCounts), 0);

        // 2. POPULATE
        for (int i = 0; i < reg.entityCount; i++) {
            if (!reg.colliders[i].isActive) continue;
            if (i == playerID) continue;

            int cx = (int)(reg.transforms[i].x / CELL_SIZE);
            int cy = (int)(reg.transforms[i].y / CELL_SIZE);

            if (cx >= 0 && cx < GRID_COLS && cy >= 0 && cy < GRID_ROWS) {
                int cellIndex = cy * GRID_COLS + cx;
                reg.nextEntity[i] = gridHead[cellIndex];
                gridHead[cellIndex] = i;

                cellCounts[cellIndex]++; // Numaram entitatile pt heatmap
            }
        }

        // 3. COIN SEPARATION PHYSICS
        for (int c = 0; c < MAX_CELLS; c++) {
            int i = gridHead[c];
            while (i != -1) {
                int j = reg.nextEntity[i];
                while (j != -1) {
                    if (reg.colliders[i].type == TYPE_COIN && reg.colliders[j].type == TYPE_COIN) {
                        float dx = reg.transforms[i].x - reg.transforms[j].x;
                        float dy = reg.transforms[i].y - reg.transforms[j].y;

                        if (abs(dx) < 20 && abs(dy) < 20) {
                            float distSq = dx * dx + dy * dy;
                            float rTotal = reg.colliders[i].radius + reg.colliders[j].radius;

                            if (distSq < rTotal * rTotal && distSq > 0.0001f) {
                                float dist = sqrt(distSq);
                                float overlap = rTotal - dist;
                                float nx = dx / dist;
                                float ny = dy / dist;
                                float separationForce = overlap * 0.5f;

                                reg.transforms[i].x += nx * separationForce;
                                reg.transforms[i].y += ny * separationForce;
                                reg.transforms[j].x -= nx * separationForce;
                                reg.transforms[j].y -= ny * separationForce;
                            }
                        }
                    }
                    j = reg.nextEntity[j];
                }
                i = reg.nextEntity[i];
            }
        }

        // 4. CHECK PLAYER COLLISION
        float px = reg.transforms[playerID].x;
        float py = reg.transforms[playerID].y;
        float pr = reg.colliders[playerID].radius;

        int pcx = (int)(px / CELL_SIZE);
        int pcy = (int)(py / CELL_SIZE);

        for (int x = std::max(0, pcx - 1); x <= std::min(GRID_COLS - 1, pcx + 1); x++) {
            for (int y = std::max(0, pcy - 1); y <= std::min(GRID_ROWS - 1, pcy + 1); y++) {
                int cellIndex = y * GRID_COLS + x;
                int currentEntityID = gridHead[cellIndex];

                while (currentEntityID != -1) {
                    float dx = px - reg.transforms[currentEntityID].x;
                    float dy = py - reg.transforms[currentEntityID].y;

                    if (abs(dx) < 40 && abs(dy) < 40) {
                        float distSq = dx * dx + dy * dy;
                        float rTotal = pr + reg.colliders[currentEntityID].radius;

                        if (distSq < rTotal * rTotal) {
                            EntityType type = reg.colliders[currentEntityID].type;
                            if (type == TYPE_COIN) {
                                score++;
                                reg.destroyEntity(currentEntityID);
                            }
                            else if (type == TYPE_ENEMY) {
                                gameOver = true;
                                reg.velocities[playerID] = { 0,0 };
                                reg.sprites[playerID].r = 100;
                            }
                        }
                    }
                    currentEntityID = reg.nextEntity[currentEntityID];
                }
            }
        }
    }
};

// --- RENDER SYSTEM CU VISUALIZARE GRID (HEATMAP) ---
class RenderSystem {
public:
    void render(Registry& reg, SDL_Renderer* renderer, const GameplaySystem& gameplaySys) {
        SDL_SetRenderDrawColor(renderer, 20, 20, 30, 255);
        SDL_RenderClear(renderer);

        // --- DRAW GRID HEATMAP ---
        SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND); // Transparenta

        for (int y = 0; y < GRID_ROWS; y++) {
            for (int x = 0; x < GRID_COLS; x++) {
                int count = gameplaySys.getCountInCell(x, y);
                if (count > 0) {
                    // Gradient Verde (putine) -> Rosu (multe)
                    Uint8 r = (count > 10) ? 255 : 0;
                    Uint8 g = (count < 10) ? 255 : 0;
                    Uint8 b = 0;
                    Uint8 a = (Uint8)std::min(150, count * 20 + 20); // Transparenta dinamica

                    SDL_SetRenderDrawColor(renderer, r, g, b, a);
                    SDL_Rect cellRect = { x * CELL_SIZE, y * CELL_SIZE, CELL_SIZE, CELL_SIZE };
                    SDL_RenderFillRect(renderer, &cellRect);
                }
            }
        }

        // Linii grid
        SDL_SetRenderDrawColor(renderer, 50, 50, 50, 255);
        for (int x = 0; x <= GRID_COLS; x++) SDL_RenderDrawLine(renderer, x * CELL_SIZE, 0, x * CELL_SIZE, WINDOW_HEIGHT);
        for (int y = 0; y <= GRID_ROWS; y++) SDL_RenderDrawLine(renderer, 0, y * CELL_SIZE, WINDOW_WIDTH, y * CELL_SIZE);

        SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE);
        // ------------------------

        SDL_Rect rect;
        for (int i = 0; i < reg.entityCount; i++) {
            if (!reg.sprites[i].isVisible) continue;

            SDL_SetRenderDrawColor(renderer, reg.sprites[i].r, reg.sprites[i].g, reg.sprites[i].b, 255);
            rect.x = (int)reg.transforms[i].x;
            rect.y = (int)reg.transforms[i].y;
            rect.w = reg.sprites[i].w;
            rect.h = reg.sprites[i].h;
            SDL_RenderFillRect(renderer, &rect);
        }
        SDL_RenderPresent(renderer);
    }
};

// ==========================================
// 4. MAIN ENGINE
// ==========================================
class GameEngine {
private:
    SDL_Window* window = nullptr;
    SDL_Renderer* renderer = nullptr;
    bool isRunning = true;
    Registry registry;
    InputSystem inputSystem;
    PhysicsSystem physicsSystem;
    RenderSystem renderSystem;
    GameplaySystem gameplaySystem;
    int playerID = 0;

public:
    bool init() {
        if (SDL_Init(SDL_INIT_VIDEO) < 0) return false;
        window = SDL_CreateWindow("Phase 2 Final: Multi-Threaded + Smart Grid + Physics", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, WINDOW_WIDTH, WINDOW_HEIGHT, SDL_WINDOW_SHOWN);
        renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);

        registry.init(MAX_ENTITIES);
        initLevel();
        return true;
    }

    void initLevel() {
        // Player
        playerID = registry.createEntity();
        registry.transforms[playerID] = { WINDOW_WIDTH / 2, WINDOW_HEIGHT / 2 };
        registry.velocities[playerID] = { 0, 0 };
        registry.sprites[playerID] = { true, 0, 255, 0, 30, 30 };
        registry.colliders[playerID] = { true, 15, TYPE_PLAYER };

        // Inamici (30)
        for (int i = 0; i < 30; i++) {
            int id = registry.createEntity();
            registry.transforms[id] = { randomFloat(0, WINDOW_WIDTH), randomFloat(0, WINDOW_HEIGHT) };
            registry.velocities[id] = { randomFloat(-250, 250), randomFloat(-250, 250) };
            registry.sprites[id] = { true, 255, 50, 50, 25, 25 };
            registry.colliders[id] = { true, 12, TYPE_ENEMY };
        }

        // Coins (1000 - Pentru Heatmap si Fizica)
        for (int i = 0; i < 1000; i++) {
            int id = registry.createEntity();
            registry.transforms[id] = { randomFloat(50, WINDOW_WIDTH - 50), randomFloat(50, WINDOW_HEIGHT - 50) };
            registry.sprites[id] = { true, 255, 215, 0, 15, 15 };
            registry.colliders[id] = { true, 8, TYPE_COIN };
        }
    }

    void run() {
        SDL_Event ev;
        Uint64 lastTime = SDL_GetPerformanceCounter();
        unsigned int threads = std::thread::hardware_concurrency();

        while (isRunning) {
            while (SDL_PollEvent(&ev)) {
                if (ev.type == SDL_QUIT) isRunning = false;
                if (ev.type == SDL_KEYDOWN && ev.key.keysym.sym == SDLK_ESCAPE) isRunning = false;
                if (ev.type == SDL_KEYDOWN && ev.key.keysym.sym == SDLK_r && gameplaySystem.gameOver) {
                    gameplaySystem.gameOver = false;
                    gameplaySystem.score = 0;
                    registry.sprites[playerID].r = 0;
                    registry.velocities[playerID] = { 0,0 };
                }
            }
            const Uint8* keys = SDL_GetKeyboardState(NULL);

            Uint64 currentTime = SDL_GetPerformanceCounter();
            float dt = (float)((currentTime - lastTime) * 1000 / (double)SDL_GetPerformanceFrequency()) / 1000.0f;
            lastTime = currentTime;

            if (!gameplaySystem.gameOver) {
                inputSystem.update(registry, keys, playerID);
                physicsSystem.update(registry, dt); // Multi-threaded Movement
                gameplaySystem.update(registry, playerID); // Main-thread Grid + Collision
            }

            renderSystem.render(registry, renderer, gameplaySystem); // Heatmap Render

            static Uint32 lastTitle = 0;
            if (SDL_GetTicks() - lastTitle > 100) {
                lastTitle = SDL_GetTicks();
                std::string title = "Engine MT (" + std::to_string(threads) + " cores) | FPS: " + std::to_string((int)(1.0f / dt)) +
                    " | Score: " + std::to_string(gameplaySystem.score);
                SDL_SetWindowTitle(window, title.c_str());
            }
        }
        SDL_DestroyRenderer(renderer);
        SDL_DestroyWindow(window);
        SDL_Quit();
    }
};

int main(int argc, char* args[]) {
    srand((unsigned int)time(0));
    GameEngine game;
    if (game.init()) {
        game.run();
    }
    return 0;
}
