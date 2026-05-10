#pragma once
#include <vector>
#include <string>
#include "Camera.h"

struct P {
    float x = 0.0f;
    float z = 0.0f;
};

struct VertexData {
    float x, y, z;
    float nx, ny, nz;
    float r, g, b;
};

enum class TowerType { MACHINE_GUN = 0, ROCKETS = 1, SNIPER = 2 };

struct TowerGeometry {
    std::vector<VertexData> base_mesh;
    std::vector<VertexData> rotate_mesh;
    std::vector<VertexData> gun_mesh;

    float base_y_offset;
    float gun_y_offset;
    float rotate_y_offset;
};

struct TowerInstance {
    float x = 0.0f;
    float z = 0.0f;
    float current_yaw = 0.0f;
    TowerType tower_variant = TowerType::MACHINE_GUN;
};

struct Troop {
    float x, z;
    int currentWaypoint;
    float speed;
    float health;
    std::string type_name;
};

class Game {
public:
    int lives = 20;
    int gold = 100;
    int wave = 1;
    bool gameOver = false;
    std::vector<Troop> troops;
    std::vector<P> pathWaypoints;
    void spawnTroop();

    TowerType selectedType = TowerType::MACHINE_GUN;
    TowerGeometry tower_assets[3];
    std::vector<TowerInstance> active_defenses;

    void loadTroopWave(std::string path);
    void init();
    void update(float dt);
    void render();
    void tryPlaceTower(int mx, int my, Camera& cam);
    void loadModels(std::string path, std::string baseDir, std::vector<VertexData>& target);
    void selectTowerType(int index);
};