#pragma once
#define GLEW_STATIC
#include <vector>
#include "Camera.h"

struct VertexData {
    float x, y, z;       // Pozycja
    float nx, ny, nz;    // Normalna
    float r, g, b;       // Kolor
};

struct Tower {
    float x, z;
};

class Game {
public:
    
    int lives = 20;
    int gold = 100;
    int wave = 1;
    bool gameOver = false;
    std::vector<Tower> towers;
    std::vector<VertexData> towerModel;

    
    void init();
    void update(float dt);
    void render();
    void tryPlaceTower(int mx, int my, Camera& cam);
    void loadModels();
};
