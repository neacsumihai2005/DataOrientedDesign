#include <SDL.h>
#include <vector>
#include <iostream>
#include <string>
#include <random>
#include <ctime>

const int WINDOW_WIDTH = 1280;
const int WINDOW_HEIGHT = 720;

const int NUM_PARTICULE = 200000;

const float RAZA_PARTICULA = 3.0f; // Marimea "sprite-ului"

float randomFloat(float min, float max) {
    return min + static_cast <float> (rand()) / (static_cast <float> (RAND_MAX / (max - min)));
}

// ==========================================
// ABORDAREA 1: OOP (Object Oriented)
// ==========================================
class ParticleOOP {
public:
    float x, y;
    float vx, vy;

    // --- JUNK DATA MASIV ---
    // Marim la 4096 bytes (4KB) per obiect.
    // 30.000 obiecte * 4KB = 120 MB RAM.
    // Asta e mult peste cei 16MB Cache ai i9-lui.
    // Procesorul VA FI OBLIGAT sa astepte dupa RAM.
    char padding[4000];
    // -----------------------

    ParticleOOP() {
        x = randomFloat(0, (float)WINDOW_WIDTH);
        y = randomFloat(0, (float)WINDOW_HEIGHT);
        vx = randomFloat(-200.0f, 200.0f);
        vy = randomFloat(-200.0f, 200.0f);
        padding[0] = 'X'; // Scriem ceva sa nu fie optimizat
    }

    void update(float deltaTime) {
        x += vx * deltaTime;
        y += vy * deltaTime;
        if (x <= 0 || x >= WINDOW_WIDTH - 3) vx *= -1;
        if (y <= 0 || y >= WINDOW_HEIGHT - 3) vy *= -1;
    }

    // ... (checkCollision ramane la fel)

    // Coliziune OOP (verificam distanta fata de celelalte)
    void checkCollision(const std::vector<ParticleOOP>& others, int myIndex) {
        for (size_t i = 0; i < others.size(); i++) {
            if (i == myIndex) continue; // Nu ne ciocnim cu noi insine

            float dx = others[i].x - x;
            float dy = others[i].y - y;
            float distSq = dx * dx + dy * dy;

            // Daca distanta e mica, inversam viteza (coliziune simpla)
            if (distSq < (RAZA_PARTICULA * 2) * (RAZA_PARTICULA * 2)) {
                vx *= -1;
                vy *= -1;
                return; // Iesim dupa prima ciocnire pt optimizare
            }
        }
    }
};

// ==========================================
// ABORDAREA 2: DOD (Data Oriented)
// ==========================================
struct ParticleSystemDOD {
    // Vectori separati (Structure of Arrays)
    // Aici NU avem junk data printre float-uri. Procesorul citeste doar X, apoi doar Y.
    std::vector<float> x;
    std::vector<float> y;
    std::vector<float> vx;
    std::vector<float> vy;
    int count = 0;

    void init(int num) {
        count = num;
        x.resize(num);
        y.resize(num);
        vx.resize(num);
        vy.resize(num);

        for (int i = 0; i < num; i++) {
            x[i] = randomFloat(0, (float)WINDOW_WIDTH);
            y[i] = randomFloat(0, (float)WINDOW_HEIGHT);
            vx[i] = randomFloat(-200.0f, 200.0f);
            vy[i] = randomFloat(-200.0f, 200.0f);
        }
    }

    void update(float deltaTime) {
        // Procesare liniara - Procesorul "zboara" prin acesti vectori
        for (int i = 0; i < count; i++) {
            x[i] += vx[i] * deltaTime;
            y[i] += vy[i] * deltaTime;

            if (x[i] <= 0 || x[i] >= WINDOW_WIDTH - RAZA_PARTICULA) vx[i] *= -1;
            if (y[i] <= 0 || y[i] >= WINDOW_HEIGHT - RAZA_PARTICULA) vy[i] *= -1;
        }
    }

    // Coliziune DOD - Implementata pentru corectitudinea comparatiei
    void checkCollisions() {
        for (int i = 0; i < count; i++) {
            for (int j = 0; j < count; j++) {
                if (i == j) continue;

                // Accesam vectorii direct. Chiar si asa, e mai rapid decat OOP cu junk data
                float dx = x[j] - x[i];
                float dy = y[j] - y[i];
                float distSq = dx * dx + dy * dy;

                if (distSq < (RAZA_PARTICULA * 2) * (RAZA_PARTICULA * 2)) {
                    vx[i] *= -1;
                    vy[i] *= -1;
                    break; // Iesim din bucla interioara
                }
            }
        }
    }
};

// ==========================================
// MAIN
// ==========================================
int main(int argc, char* args[]) {
    srand((unsigned int)time(0));

    if (SDL_Init(SDL_INIT_VIDEO) < 0) { return -1; }

    SDL_Window* window = SDL_CreateWindow("Faza 1: OOP vs DOD Analysis", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, WINDOW_WIDTH, WINDOW_HEIGHT, SDL_WINDOW_SHOWN);
    SDL_Renderer* renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);

    // --- CONFIGURARE ---
    bool useDOD = false;        // TAB sa schimbi
    bool runCollision = false;  // 'C' sa activezi (ATENTIE: Doar la putine particule!)
    bool renderEnabled = true;  // 'R' sa opresti desenarea (pt testare CPU pura)

    // OOP Setup
    std::vector<ParticleOOP> particlesOOP(NUM_PARTICULE);

    // DOD Setup
    ParticleSystemDOD particlesDOD;
    particlesDOD.init(NUM_PARTICULE);

    bool isRunning = true;
    SDL_Event ev;

    while (isRunning) {
        // 1. INPUT
        while (SDL_PollEvent(&ev)) {
            if (ev.type == SDL_QUIT) isRunning = false;
            if (ev.type == SDL_KEYDOWN) {
                if (ev.key.keysym.sym == SDLK_TAB) useDOD = !useDOD;
                if (ev.key.keysym.sym == SDLK_c) {
                    runCollision = !runCollision;
                    std::cout << "Collision: " << (runCollision ? "ON" : "OFF") << std::endl;
                }
                if (ev.key.keysym.sym == SDLK_r) renderEnabled = !renderEnabled;
            }
        }

        // 2. UPDATE (LOGIC)
        Uint64 startPerf = SDL_GetPerformanceCounter();

        float dt = 0.016f; // Delta time fix pentru consistenta testului

        if (useDOD) {
            particlesDOD.update(dt);
            if (runCollision) {
                particlesDOD.checkCollisions();
            }
        }
        else {
            for (int i = 0; i < NUM_PARTICULE; i++) {
                particlesOOP[i].update(dt);
                if (runCollision) {
                    particlesOOP[i].checkCollision(particlesOOP, i);
                }
            }
        }

        Uint64 endPerf = SDL_GetPerformanceCounter();
        float timeMs = (float)((endPerf - startPerf) * 1000 / (double)SDL_GetPerformanceFrequency());

        // 3. RENDER
        if (renderEnabled) {
            SDL_SetRenderDrawColor(renderer, 20, 20, 25, 255);
            SDL_RenderClear(renderer);

            // Setam culoarea: Verde (DOD), Rosu (OOP)
            SDL_SetRenderDrawColor(renderer, useDOD ? 0 : 255, useDOD ? 255 : 50, 50, 255);

            SDL_Rect rect;
            rect.w = (int)RAZA_PARTICULA;
            rect.h = (int)RAZA_PARTICULA;

            if (useDOD) {
                for (int i = 0; i < particlesDOD.count; i++) {
                    rect.x = (int)particlesDOD.x[i];
                    rect.y = (int)particlesDOD.y[i];
                    SDL_RenderFillRect(renderer, &rect); // Desenam patratel
                }
            }
            else {
                for (int i = 0; i < NUM_PARTICULE; i++) {
                    rect.x = (int)particlesOOP[i].x;
                    rect.y = (int)particlesOOP[i].y;
                    SDL_RenderFillRect(renderer, &rect);
                }
            }
            SDL_RenderPresent(renderer);
        }

        // 4. TITLU
        static Uint32 lastTitleUpdate = 0;
        Uint32 currentTick = SDL_GetTicks();

        // Actualizam titlul la fiecare 100ms (de 10 ori pe secunda)
        if (currentTick - lastTitleUpdate > 100) {
            lastTitleUpdate = currentTick;

            // Calculam si FPS-ul real (pentru curiozitate)
            // timeMs este timpul strict pentru Update (matematica)
            std::string title = std::string(useDOD ? "Mode: [ DOD ]" : "Mode: [ OOP ]") +
                " | Objects: " + std::to_string(NUM_PARTICULE) +
                " | UPDATE TIME: " + std::to_string(timeMs) + " ms" +
                (runCollision ? " [COLLISION ON]" : "") +
                (renderEnabled ? "" : " [NO RENDER]");

            SDL_SetWindowTitle(window, title.c_str());
        }
    }

    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}