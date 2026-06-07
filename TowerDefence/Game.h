#pragma once
#include <vector>
#include <string>
#include <GL/glew.h>
#include "Camera.h"
#include "Road.h"

// Enumeration for different types of towers available in the game, which can be used to determine the tower's behavior, appearance, and stats. Each tower type has its own unique properties such as fire rate, damage, range, and cost, which are defined in the getTowerDefaults() method when placing a new tower instance.
enum class TowerType { MACHINE_GUN = 0, ROCKETS = 1, SNIPER = 2 };
enum TroopType { CAR, TANK, HELICOPTER };

// 2D point structure for map logic
struct P {
    float x = 0.0f;
    float z = 0.0f;
};

// Struct for passing verted data to the GPU
struct VertexData {
    float x, y, z; // Position
    float nx, ny, nz; // Normals
    float r, g, b; // Vertex colors
    float u, v; // UVs textures
	float tx, tz, ty; // Tangents for normal mapping
};

// Group texture for the same model
struct TextureBundle {
    GLuint baseColor = 0;
    GLuint normalMap = 0;
    GLuint metallic = 0;
    GLuint roughness = 0;
    GLuint emissive = 0;
    GLuint ao = 0;
	GLuint height = 0;
};

// Stores GPU handles
struct MeshBuffer {
    GLuint vao = 0;
    GLuint vbo = 0;
    int    count = 0;
};

// Connects meshes to their textures + offsets
struct TowerGeometry {
    std::vector<VertexData> base_mesh;
    std::vector<VertexData> rotate_mesh;
    std::vector<VertexData> gun_mesh;
    float base_y_offset = 0.0f;
    float gun_y_offset = 0.0f;
    float rotate_y_offset = 0.0f;

    TextureBundle base_tex;
    TextureBundle rotate_tex;
    TextureBundle gun_tex;

    MeshBuffer base_buf;
    MeshBuffer rotate_buf;
    MeshBuffer gun_buf;
};

// Tracks individual tower instance
struct TowerInstance {
    float x = 0.0f;
    float z = 0.0f;
    // orientation of the turrets
    float current_yaw = 0.0f;
    float current_pitch = 0.0f;
    TowerType tower_variant = TowerType::MACHINE_GUN;
    float tower_rotate_speed = 15.0f;
    float tower_fire_rate = 0.15f;
    float tower_dmg = 20.0f;
    float tower_range = 9.0f;
    int cost = 20;
    float current_cooldown = 0.0f;

    int nextBarrel = 0; // Cycling between next barrel - used in rockets and machine gun logic
};

// Tracks individual enemy
struct Troop {
    float x = 0.0f, z = 0.0f, altitude = 0.5f, rotation_yaw = 0.0f;
    float health = 100.0f, speed = 8.0f;
    int currentWaypoint = 0;
    TroopType variant = CAR;
};

// Connects troop meshes with textures 
struct TroopGeometry {
    std::vector<VertexData> base_mesh;
    std::vector<VertexData> wheel_mesh;
    std::vector<VertexData> prop_mesh;

    TextureBundle base_tex;

    // GPU buffer handles
    MeshBuffer base_buf;
    MeshBuffer wheel_buf;
    MeshBuffer prop_buf;
};

struct Projectile {
    float x = 0.0f, y = 0.0f, z = 0.0f;
    float vx = 0.0f, vy = 0.0f, vz = 0.0f;
    float life_span = 0.0f;
    float damage;
    bool isRocket = false;
};

// Connects texutures and meshes of rockets and bullets
struct ProjectileGeometry {
    std::vector<VertexData> bullet_mesh;
    std::vector<VertexData> rocket_mesh;

    TextureBundle bullet_tex;
    TextureBundle rocket_tex;

    // GPU buffer
    MeshBuffer bullet_buf;
    MeshBuffer rocket_buf;
};

// Game logic
class Game {
public:
    // screen size
    float hud_win_w;
    float hud_win_h;

    float gameSpeed = 1.0f;
    bool  paused = false;

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

    void spawnProjectile(float sx, float sy, float sz, float tx, float ty, float tz, float damage_val, bool isRocket = false);

    void startNextWave();

    bool raycastGroundPlane(float mx, float my, int winW, int winH, float& outX, float& outZ);

    float ghost_wx = 0.0f;
    float ghost_wz = 0.0f;

    float waveEndMessageTimer = 0.0f;

    std::vector<Troop> troops;
    std::vector<P> pathWaypoints;
    Road game_pathway;

    TowerType selectedType = TowerType::MACHINE_GUN;
    TowerGeometry tower_assets[3];
    std::vector<TowerInstance> active_defenses;

    TroopGeometry troop_assets[3];

    ProjectileGeometry projectile_assets;

    TextureBundle groundTex;
    TextureBundle pathTex;

    MeshBuffer groundBuf;
    MeshBuffer roadBuf;
    MeshBuffer circleBuf;

    void init();
    void update(float delta_step);
    void render(glm::mat4 P, glm::mat4 V);
    void spawnTroop();

    void tryPlaceTower(int mouse_x, int mouse_y, Camera& world_camera);
    void loadModels(std::string root_dir, std::string file_name, std::vector<VertexData>& vertex_buffer);

    void toggleBuildMode(int typeIndex);
    void selectTowerType(int i);

    TowerInstance getTowerDefaults(TowerType type);

    GLuint shader_id = 0;
    GLuint hud_shader = 0;
    GLuint loadShader(const char* vertexPath, const char* fragmentPath);

    static GLuint readTexture(const char* filename);

private:
    MeshBuffer uploadMesh(const std::vector<VertexData>& verts);
    MeshBuffer uploadLineLoop(const std::vector<float>& xyzs);

    void drawMesh(const MeshBuffer& buf);
    void renderHUD();
};