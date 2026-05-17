#define TINYOBJLOADER_IMPLEMENTATION
#include "tiny_obj_loader.h" 
#include <iostream>
#include <fstream>
#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <GL/glu.h>
#include <GL/glut.h>
#include "Game.h"

static float spawn_interval = 2.0f;
static int   tower_cost_lookup[3] = { 20, 50, 80 };
static float tower_rotate_speed[3] = { 15.0f, 8.0f, 25.0f };  // machine gun, rockets, sniper
static float tower_fire_rate[3] = { 0.15f, 0.8f, 0.4f };  // machine gun, rockets, sniper

void Game::init() {
    lives = 20;
    gold = 200;
    wave = 1;
    waveActive = false;
    gameOver = false;
    isBuilding = false;
    ghost_wx = 0.0f;
    ghost_wz = 0.0f;
    active_defenses.clear();
    active_bullets.clear();
    troops.clear();
    pathWaypoints.clear();
    selectedType = TowerType::MACHINE_GUN;

    pathWaypoints = {
        {-45.0f, 10.0f},
        {-20.0f, 10.0f},
        {-20.0f, -15.0f},
        {20.0f, -15.0f},
        {20.0f, 30.0f},
        {45.0f, 30.0f}
    };

    std::string tower_directory = "Assets/Towers/source/";
    loadModels(tower_directory, "base.obj", tower_assets[0].base_mesh);
    loadModels(tower_directory, "machine_rotate.obj", tower_assets[0].rotate_mesh);
    loadModels(tower_directory, "machine_gun.obj", tower_assets[0].gun_mesh);
    tower_assets[0].base_y_offset = 0.2f;
    tower_assets[0].gun_y_offset = 1.8f;
    tower_assets[0].rotate_y_offset = 0.8f;

    loadModels(tower_directory, "base.obj", tower_assets[1].base_mesh);
    loadModels(tower_directory, "rockets_rotate.obj", tower_assets[1].rotate_mesh);
    loadModels(tower_directory, "rockets_gun.obj", tower_assets[1].gun_mesh);
    tower_assets[1].base_y_offset = 0.2f;
    tower_assets[1].gun_y_offset = 0.6f;
    tower_assets[1].rotate_y_offset = 2.8f;

    loadModels(tower_directory, "base.obj", tower_assets[2].base_mesh);
    loadModels(tower_directory, "sniper_rotate.obj", tower_assets[2].rotate_mesh);
    loadModels(tower_directory, "sniper_gun.obj", tower_assets[2].gun_mesh);
    tower_assets[2].base_y_offset = 0.2f;
    tower_assets[2].gun_y_offset = 1.8f;
    tower_assets[2].rotate_y_offset = 0.8f;

    std::string troop_dir = "Assets/Troops/source/";
    loadModels(troop_dir, "car.obj", troop_assets[0].mesh);
    loadModels(troop_dir, "tank.obj", troop_assets[1].mesh);
    loadModels(troop_dir, "helicopter.obj", troop_assets[2].mesh);
}

void Game::startNextWave() {
    if (waveActive) return;
    waveActive = true;
    wave++;
    if (wave <= 3)
        troopsRemainingInWave = 3 + wave;
    else
        troopsRemainingInWave = 5 + (wave * 2);
    spawnTimer = 0.0f;
}

void Game::update(float delta_step) {
    if (gameOver) return;

    if (waveActive && troopsRemainingInWave > 0) {
        spawnTimer += delta_step;
        if (spawnTimer > spawn_interval) {
            spawnTroop();
            troopsRemainingInWave--;
            spawnTimer = 0.0f;
        }
    }
    else if (troopsRemainingInWave <= 0 && troops.empty()) {
        if (waveActive) waveEndMessageTimer = 3.0f;
        waveActive = false;
    }

    if (waveEndMessageTimer > 0.0f) waveEndMessageTimer -= delta_step;

    for (auto it = troops.begin(); it != troops.end(); ) {
        float tx = pathWaypoints[it->currentWaypoint].x;
        float tz = pathWaypoints[it->currentWaypoint].z;
        float dx = tx - it->x;
        float dz = tz - it->z;
        float dist = sqrtf(dx * dx + dz * dz);
        float moveDist = it->speed * delta_step;

        if (dist > 0.001f) {
            float targetA = atan2f(dx, dz) * 180.0f / 3.14159f;
            float diff = targetA - it->rotation_yaw;
            while (diff < -180) diff += 360;
            while (diff > 180) diff -= 360;
            it->rotation_yaw += diff * delta_step * 10.0f;
        }

        if (dist <= moveDist) {
            it->x = tx; it->z = tz;
            it->currentWaypoint++;
        }
        else {
            it->x += (dx / dist) * moveDist;
            it->z += (dz / dist) * moveDist;
        }

        if (it->currentWaypoint >= (int)pathWaypoints.size()) {
            lives--; it = troops.erase(it);
        }
        else ++it;
    }

    for (size_t i = 0; i < active_defenses.size(); i++) {
        auto& tower = active_defenses[i];
        float range = 9.0f;
        if (tower.tower_variant == TowerType::ROCKETS) range = 10.0f;
        else if (tower.tower_variant == TowerType::SNIPER) range = 15.0f;

        Troop* target = nullptr;
        float bestDist = range;
        for (auto& troop : troops) {
            float d = sqrtf(powf(troop.x - tower.x, 2) + powf(troop.z - tower.z, 2));
            if (d < bestDist) { bestDist = d; target = &troop; }
        }

        if (target) {
            const auto& assets = tower_assets[(int)tower.tower_variant];
            float adx = target->x - tower.x;
            float adz = target->z - tower.z;
            float gunWorldY = assets.base_y_offset + assets.rotate_y_offset + assets.gun_y_offset;
            float ady = target->altitude - gunWorldY;

            float targetYaw = atan2f(adx, adz) * 180.0f / 3.14159f;
            float dist2D = sqrtf(adx * adx + adz * adz);
            float targetPitch = atan2f(ady, dist2D) * 180.0f / 3.14159f + 20.0f;

            float yDiff = targetYaw - tower.current_yaw;
            while (yDiff < -180) yDiff += 360;
            while (yDiff > 180) yDiff -= 360;
            int ti = (int)tower.tower_variant;

            tower.current_yaw += yDiff * delta_step * tower_rotate_speed[ti];
            tower.current_pitch += (targetPitch - tower.current_pitch) * delta_step * tower_rotate_speed[ti];

            if (i < 1000) fire_cooldowns[i] -= delta_step;
            if (fire_cooldowns[i] <= 0.0f && fabs(yDiff) < 15.0f) {
                spawnProjectile(tower.x, gunWorldY, tower.z, target->x, target->altitude, target->z);
                fire_cooldowns[i] = tower_fire_rate[ti];
            }
        }
    }

    for (auto it = active_bullets.begin(); it != active_bullets.end(); ) {
        it->x += it->vx * delta_step;
        it->y += it->vy * delta_step;
        it->z += it->vz * delta_step;
        it->life_span -= delta_step;
        bool destroyed = false;
        for (auto& enemy : troops) {
            float dist = sqrtf(powf(enemy.x - it->x, 2) + powf(enemy.altitude - it->y, 2) + powf(enemy.z - it->z, 2));
            if (dist < 1.2f) { enemy.health -= 20.0f; destroyed = true; break; }
        }
        if (destroyed || it->life_span <= 0) it = active_bullets.erase(it);
        else ++it;
    }

    for (auto it = troops.begin(); it != troops.end(); ) {
        if (it->health <= 0) { gold += 15; it = troops.erase(it); }
        else ++it;
    }
    if (lives <= 0) { lives = 0; gameOver = true; }
}

static void renderMesh(const std::vector<VertexData>& mesh, bool useColor = true) {
    glBegin(GL_TRIANGLES);
    for (const auto& v : mesh) {
        if (useColor) glColor3f(v.r, v.g, v.b);
        glNormal3f(v.nx, v.ny, v.nz);
        glVertex3f(v.x, v.y, v.z);
    }
    glEnd();
}

static bool isOnRoad(float wx, float wz, const std::vector<P>& waypoints, float halfWidth) {
    for (int i = 0; i + 1 < (int)waypoints.size(); i++) {
        float ax = waypoints[i].x, az = waypoints[i].z;
        float bx = waypoints[i + 1].x, bz = waypoints[i + 1].z;
        float abx = bx - ax, abz = bz - az;
        float apx = wx - ax, apz = wz - az;
        float ab2 = abx * abx + abz * abz;
        float t = (ab2 > 0.f) ? (apx * abx + apz * abz) / ab2 : 0.f;
        t = fmaxf(0.f, fminf(1.f, t));
        float cx = ax + t * abx, cz = az + t * abz;
        float d = sqrtf(powf(wx - cx, 2) + powf(wz - cz, 2));
        if (d < halfWidth) return true;
    }
    return false;
}

bool Game::raycastGroundPlane(float mx, float my, int winW, int winH, float& outX, float& outZ) {
    GLdouble mv[16], pr[16]; GLint vp[4];
    glGetDoublev(GL_MODELVIEW_MATRIX, mv);
    glGetDoublev(GL_PROJECTION_MATRIX, pr);
    glGetIntegerv(GL_VIEWPORT, vp);

    GLdouble ax, ay, az, bx, by, bz;
    gluUnProject(mx, vp[3] - my, 0.0, mv, pr, vp, &ax, &ay, &az);
    gluUnProject(mx, vp[3] - my, 1.0, mv, pr, vp, &bx, &by, &bz);

    float rayOy = (float)ay;
    float rayDy = (float)(by - ay);
    if (fabsf(rayDy) < 0.0001f) return false;
    float t = -rayOy / rayDy;
    if (t < 0) return false;

    outX = (float)(ax + t * (bx - ax));
    outZ = (float)(az + t * (bz - az));
    return true;
}

void Game::render() {
    glBegin(GL_QUADS);
    glColor3f(0.2f, 0.5f, 0.2f); glNormal3f(0, 1, 0);
    glVertex3f(-50, -0.01f, -50); glVertex3f(50, -0.01f, -50);
    glVertex3f(50, -0.01f, 50); glVertex3f(-50, -0.01f, 50);
    glEnd();

    game_pathway.Draw();

    for (const auto& troop : troops) {
        glPushMatrix();
        glTranslatef(troop.x, troop.altitude, troop.z);
        glRotatef(troop.rotation_yaw, 0, 1, 0);
        glScalef(0.3f, 0.3f, 0.3f);
        renderMesh(troop_assets[(int)troop.variant].mesh);
        glPopMatrix();
    }

    for (const auto& tower : active_defenses) {
        const auto& assets = tower_assets[(int)tower.tower_variant];
        glPushMatrix();
        glTranslatef(tower.x, assets.base_y_offset, tower.z);
        glScalef(0.5f, 0.5f, 0.5f);
        renderMesh(assets.base_mesh);
        glTranslatef(0, assets.rotate_y_offset, 0);
        glRotatef(tower.current_yaw, 0, 1, 0);
        renderMesh(assets.rotate_mesh);
        glTranslatef(0, assets.gun_y_offset, 0);
        glRotatef(-tower.current_pitch, 1, 0, 0);
        renderMesh(assets.gun_mesh);
        glPopMatrix();
    }

    glDisable(GL_LIGHTING);

    for (size_t i = 0; i < active_bullets.size(); i++) {
        const auto& b = active_bullets[i];
        glPushMatrix();
        glTranslatef(b.x, b.y, b.z);
        if (i == 0) glColor3f(1.0f, 0.8f, 0.2f); 
        else        glColor3f(0.2f, 0.5f, 1.0f); 

        float s = 0.15f;
        glBegin(GL_QUADS);
        glVertex3f(-s, -s, -s); glVertex3f(s, -s, -s); glVertex3f(s, s, -s); glVertex3f(-s, s, -s);
        glEnd();
        glPopMatrix();

        if (i == 0) {
            glEnable(GL_LIGHT1);
            GLfloat bullet_yellow[] = { 1.0f, 0.8f, 0.2f, 1.0f };
            glLightfv(GL_LIGHT1, GL_DIFFUSE, bullet_yellow);

            GLfloat pos1[] = { b.x, b.y, b.z, 1.0f };
            glLightfv(GL_LIGHT1, GL_POSITION, pos1);

            glLightf(GL_LIGHT1, GL_CONSTANT_ATTENUATION, 0.5f);
            glLightf(GL_LIGHT1, GL_LINEAR_ATTENUATION, 0.2f);
            glLightf(GL_LIGHT1, GL_QUADRATIC_ATTENUATION, 0.05f);
        }
        else if (i == 1) {
            glEnable(GL_LIGHT2);
            GLfloat bullet_blue[] = { 0.2f, 0.5f, 1.0f, 1.0f };
            glLightfv(GL_LIGHT2, GL_DIFFUSE, bullet_blue);

            GLfloat pos2[] = { b.x, b.y, b.z, 1.0f };
            glLightfv(GL_LIGHT2, GL_POSITION, pos2);

            glLightf(GL_LIGHT2, GL_CONSTANT_ATTENUATION, 0.5f);
            glLightf(GL_LIGHT2, GL_LINEAR_ATTENUATION, 0.2f);
            glLightf(GL_LIGHT2, GL_QUADRATIC_ATTENUATION, 0.05f);
        }
    }

    if (active_bullets.empty()) {
        glDisable(GL_LIGHT1);
    }
    if (active_bullets.size() < 2) {
        glDisable(GL_LIGHT2);
    }

    glEnable(GL_LIGHTING);

    if (isBuilding && !gameOver) {
        double mx, my;
        glfwGetCursorPos(glfwGetCurrentContext(), &mx, &my);
        int win_w, win_h;
        glfwGetWindowSize(glfwGetCurrentContext(), &win_w, &win_h);

        float ground_x, ground_z;
        if (raycastGroundPlane((float)mx, (float)my, win_w, win_h, ground_x, ground_z)) {
            ghost_wx = ground_x;
            ghost_wz = ground_z;
        }

        float ghostRange = 9.0f;
        if (ghostType == TowerType::ROCKETS) ghostRange = 10.0f;
        else if (ghostType == TowerType::SNIPER) ghostRange = 15.0f;

        bool onRoad = isOnRoad(ghost_wx, ghost_wz, pathWaypoints, game_pathway.roadWidth);

        const auto& assets = tower_assets[(int)ghostType];
        glDisable(GL_LIGHTING);
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

        glPushMatrix();
        glTranslatef(ghost_wx, 0.1f, ghost_wz);
        if (onRoad) glColor4f(1, 0, 0, 0.6f);
        else        glColor4f(0, 1, 0, 0.3f);
        glBegin(GL_LINE_LOOP);
        for (int i = 0; i < 36; i++) {
            float a = i * 10.0f * 3.1415f / 180.0f;
            glVertex3f(cosf(a) * ghostRange, 0, sinf(a) * ghostRange);
        }
        glEnd();
        glPopMatrix();

        glPushMatrix();
        glTranslatef(ghost_wx, assets.base_y_offset, ghost_wz);
        glScalef(0.5f, 0.5f, 0.5f);
        if (onRoad) glColor4f(1.0f, 0.3f, 0.3f, 0.5f);
        else        glColor4f(1.0f, 1.0f, 1.0f, 0.5f);
        renderMesh(assets.base_mesh, false);
        glTranslatef(0, assets.rotate_y_offset, 0);
        renderMesh(assets.rotate_mesh, false);
        glTranslatef(0, assets.gun_y_offset, 0);
        renderMesh(assets.gun_mesh, false);
        glPopMatrix();

        glDisable(GL_BLEND);
        glEnable(GL_LIGHTING);
    }

    renderHUD();
}

void Game::spawnProjectile(float sx, float sy, float sz, float tx, float ty, float tz) {
    Projectile p;
    p.x = sx; p.y = sy; p.z = sz;
    float dx = tx - sx, dy = ty - sy, dz = tz - sz;
    float dist = sqrtf(dx * dx + dy * dy + dz * dz);
    float speed = 50.0f;
    p.vx = (dx / dist) * speed; p.vy = (dy / dist) * speed; p.vz = (dz / dist) * speed;
    p.life_span = 2.0f;
    active_bullets.push_back(p);
}

void Game::spawnTroop() {
    if (pathWaypoints.empty()) return;
    Troop t;
    t.x = pathWaypoints[0].x;
    t.z = pathWaypoints[0].z;
    t.currentWaypoint = 1;
    t.rotation_yaw = 0.0f;

    int type = rand() % 3;
    t.variant = (TroopType)type;
    if (t.variant == CAR) { t.health = 40;  t.speed = 3.5f; t.altitude = 0.5f; }
    else if (t.variant == TANK) { t.health = 150; t.speed = 2.0f; t.altitude = 0.5f; }
    else { t.health = 60;  t.speed = 12.0f; t.altitude = 3.0f; }

    troops.push_back(t);
}

void Game::renderHUD() {
    glMatrixMode(GL_PROJECTION); glPushMatrix(); glLoadIdentity(); gluOrtho2D(0, 800, 600, 0);
    glMatrixMode(GL_MODELVIEW);  glPushMatrix(); glLoadIdentity();
    glDisable(GL_LIGHTING); glDisable(GL_DEPTH_TEST); glDisable(GL_TEXTURE_2D);

    glColor3f(1, 1, 0);
    std::string hud = "Gold: " + std::to_string(gold) + "  Lives: " + std::to_string(lives) + "  Wave: " + std::to_string(wave);
    glRasterPos2f(20, 30);
    for (char c : hud) glutBitmapCharacter(GLUT_BITMAP_9_BY_15, c);

    if (waveEndMessageTimer > 0.0f) {
        glColor3f(1.0f, 0.5f, 0.0f);
        std::string msg = "Wave " + std::to_string(wave - 1) + " complete";
        glRasterPos2f(250, 280);
        for (char c : msg) glutBitmapCharacter(GLUT_BITMAP_HELVETICA_18, c);
    }

    if (gameOver) {
        glColor3f(1.0f, 0.0f, 0.0f);
        std::string msg = "GAME OVER";
        glRasterPos2f(250, 280);
        for (char c : msg) glutBitmapCharacter(GLUT_BITMAP_HELVETICA_18, c);
    }

    glEnable(GL_DEPTH_TEST); glEnable(GL_LIGHTING);
    glMatrixMode(GL_PROJECTION); glPopMatrix();
    glMatrixMode(GL_MODELVIEW);  glPopMatrix();
}

void Game::selectTowerType(int i) { if (i >= 0 && i <= 2) selectedType = (TowerType)i; }

void Game::loadModels(std::string root, std::string file, std::vector<VertexData>& buffer) {
    tinyobj::attrib_t attr; std::vector<tinyobj::shape_t> shp; std::vector<tinyobj::material_t> mat; std::string w, e;
    if (!tinyobj::LoadObj(&attr, &shp, &mat, &w, &e, (root + file).c_str(), root.c_str())) return;
    for (const auto& s : shp) {
        size_t off = 0;
        for (size_t f = 0; f < s.mesh.num_face_vertices.size(); f++) {
            int fv = s.mesh.num_face_vertices[f];
            for (size_t v = 0; v < fv; v++) {
                tinyobj::index_t idx = s.mesh.indices[off + v];
                VertexData vd;
                vd.x = attr.vertices[3 * idx.vertex_index + 0];
                vd.y = attr.vertices[3 * idx.vertex_index + 1];
                vd.z = attr.vertices[3 * idx.vertex_index + 2];
                if (idx.normal_index >= 0) {
                    vd.nx = attr.normals[3 * idx.normal_index + 0];
                    vd.ny = attr.normals[3 * idx.normal_index + 1];
                    vd.nz = attr.normals[3 * idx.normal_index + 2];
                }
                else {
                    vd.nx = 0; vd.ny = 1; vd.nz = 0;
                }
                int mat_id = s.mesh.material_ids[f];
                if (mat_id >= 0 && mat_id < (int)mat.size()) {
                    vd.r = mat[mat_id].diffuse[0];
                    vd.g = mat[mat_id].diffuse[1];
                    vd.b = mat[mat_id].diffuse[2];
                }
                else {
                    vd.r = vd.g = vd.b = 0.7f;
                }
                buffer.push_back(vd);
            }
            off += fv;
        }
    }
}

void Game::loadTroopWave(std::string resource_path) {
    std::ifstream wave_input_stream(resource_path);
    if (!wave_input_stream.is_open()) return;
    std::string troop_variant;
    float current_move_speed, current_max_hp;
    while (wave_input_stream >> troop_variant >> current_move_speed >> current_max_hp) {
        Troop troop_instance;
        troop_instance.x = pathWaypoints[0].x;
        troop_instance.z = pathWaypoints[0].z;
        troop_instance.currentWaypoint = 1;
        troop_instance.speed = current_move_speed;
        troop_instance.health = current_max_hp;
        troops.push_back(troop_instance);
    }
    wave_input_stream.close();
}

void Game::tileCenter(int col_idx, int row_idx, float& world_x_pos, float& world_z_pos) {
    float half = 24 * 5.0f * 0.5f;
    world_x_pos = -half + col_idx * 5.0f + 2.5f;
    world_z_pos = -half + row_idx * 5.0f + 2.5f;
}

void Game::toggleBuildMode(int typeIndex) {
    TowerType t = (TowerType)typeIndex;
    if (isBuilding && ghostType == t) isBuilding = false;
    else { isBuilding = true; ghostType = t; selectedType = t; }
}

void Game::tryPlaceTower(int mouse_x, int mouse_y, Camera& world_camera) {
    if (!isBuilding) return;
    int current_cost = tower_cost_lookup[(int)selectedType];
    if (gold < current_cost) return;
    if (isOnRoad(ghost_wx, ghost_wz, pathWaypoints, game_pathway.roadWidth)) return;

    TowerInstance turret_unit;
    turret_unit.x = ghost_wx;
    turret_unit.z = ghost_wz;
    turret_unit.current_yaw = 0.0f;
    turret_unit.tower_variant = selectedType;
    active_defenses.push_back(turret_unit);
    gold -= current_cost;

    std::cout << "Postawiono wieze typu " << (int)selectedType
        << " | X=" << ghost_wx << " Z=" << ghost_wz
        << " | Zloto: " << gold << std::endl;
}