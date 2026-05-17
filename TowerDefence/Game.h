#pragma once
#include <vector>
#include <string>
#include <GL/glew.h>
#include "Camera.h"
#include "Road.h"

enum TileType { TILE_GRASS, TILE_PATH, TILE_TREE, TILE_MOUNTAIN };
enum class TowerType { MACHINE_GUN = 0, ROCKETS = 1, SNIPER = 2 };
enum TroopType { CAR, TANK, HELICOPTER };
enum GameState { START_MENU, PLAYING, GAME_OVER };

struct P {
    float x = 0.0f;
    float z = 0.0f;
};

struct VertexData {
    float x, y, z;
    float nx, ny, nz;
    float r, g, b;
};

struct TowerGeometry {
    std::vector<VertexData> base_mesh;
    std::vector<VertexData> rotate_mesh;
    std::vector<VertexData> gun_mesh;
    float base_y_offset = 0.0f;
    float gun_y_offset = 0.0f;
    float rotate_y_offset = 0.0f;
};

struct TowerInstance {
    float x = 0.0f;
    float z = 0.0f;
    float current_yaw = 0.0f;
    float current_pitch = 0.0f;
    TowerType tower_variant = TowerType::MACHINE_GUN;
};

struct Troop {
    float x = 0.0f, z = 0.0f, altitude = 0.5f, rotation_yaw = 0.0f;
    float health = 100.0f, speed = 8.0f;
    int currentWaypoint = 0;
    TroopType variant = CAR;
};

struct TroopGeometry {
    std::vector<VertexData> base_mesh;
    std::vector<VertexData> wheel_mesh;
    std::vector<VertexData> prop_mesh;
};

struct Projectile{
    float x = 0.0f, y = 0.0f, z = 0.0f;
    float vx = 0.0f, vy = 0.0f, vz = 0.0f;
    float life_span = 0.0f;
};

class Game {
public:
    float hud_win_w;
    float hud_win_h;

    int lives = 20;
    int gold = 100;
    int wave = 0;
    bool gameOver = false;
    bool waveActive = false;
    int troopsRemainingInWave = 0;
    float spawnTimer = 0.0f;

    bool isBuilding = false;
    TowerType ghostType = TowerType::MACHINE_GUN;

    std::vector<Projectile> active_bullets;
    float fire_cooldowns[1000] = { 0 };

    void spawnProjectile(float sx, float sy, float sz, float tx, float ty, float tz);

    void startNextWave();

    bool raycastGroundPlane(float mx, float my, int winW, int winH, float& outX, float& outZ);

    float ghost_wx = 0.0f;
    float ghost_wz = 0.0f;

    float waveEndMessageTimer = 0.0f;

    static const int grid_dim = 24;
    TileType map[grid_dim][grid_dim];
    float height_map[grid_dim][grid_dim];

    std::vector<Troop> troops;
    std::vector<P> pathWaypoints;
    Road game_pathway;

    TowerType selectedType = TowerType::MACHINE_GUN;
    TowerGeometry tower_assets[3];
    std::vector<TowerInstance> active_defenses;

    TroopGeometry troop_assets[3];

    void init();
    void update(float delta_step);
    void render();
    void spawnTroop();

    void loadMapFromImage(const std::string& texture_path);
    void tileCenter(int col_idx, int row_idx, float& world_x_pos, float& world_z_pos);

    void tryPlaceTower(int mouse_x, int mouse_y, Camera& world_camera);
    void loadModels(std::string root_dir, std::string file_name, std::vector<VertexData>& vertex_buffer);
    void loadTroopWave(std::string resource_path);
    void selectTowerType(int list_index);
    void toggleBuildMode(int typeIndex);

    GLuint shader_id = 0;
    GLuint loadShader(const char* vertexPath, const char* fragmentPath);
private:
    void renderHUD();
};