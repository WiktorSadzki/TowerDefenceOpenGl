#define TINYOBJLOADER_IMPLEMENTATION
#include "tiny_obj_loader.h"
#include <iostream>
#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <GL/glu.h>
#include "Game.h"

static int towerCost[3] = { 20, 50, 80 };

void Game::init() {
    lives = 20;
    gold = 200;
    wave = 1;
    gameOver = false;
    active_defenses.clear();
    selectedType = TowerType::MACHINE_GUN;

    active_defenses.clear();
    troops.clear();
    pathWaypoints.clear();

    pathWaypoints = {
        {-45.0f, 10.0f},
        {-20.0f, 10.0f},
        {-20.0f, -15.0f},
        {20.0f, -15.0f},
        {20.0f, 30.0f},
        {45.0f, 30.0f}
    };

    std::string towers_path = "Assets/Towers/source/";

    // Machine Gun
    loadModels(towers_path, "base.obj", tower_assets[0].base_mesh);
    loadModels(towers_path, "machine_rotate.obj", tower_assets[0].rotate_mesh);
    loadModels(towers_path, "machine_gun.obj", tower_assets[0].gun_mesh);

    tower_assets[0].base_y_offset = 0.2f;
    tower_assets[0].gun_y_offset = 1.8f;
    tower_assets[0].rotate_y_offset = 0.8f;

    // Rockets
    loadModels(towers_path, "base.obj", tower_assets[1].base_mesh);
    loadModels(towers_path, "rockets_rotate.obj", tower_assets[1].rotate_mesh);
    loadModels(towers_path, "rockets_gun.obj", tower_assets[1].gun_mesh);

    tower_assets[1].base_y_offset = 0.2f;
    tower_assets[1].gun_y_offset = 0.6f;
    tower_assets[1].rotate_y_offset = 2.8f;

    // Sniper
    loadModels(towers_path, "base.obj", tower_assets[2].base_mesh);
    loadModels(towers_path, "sniper_rotate.obj", tower_assets[2].rotate_mesh);
    loadModels(towers_path, "sniper_gun.obj", tower_assets[2].gun_mesh);

    tower_assets[2].base_y_offset = 0.2f;
    tower_assets[2].gun_y_offset = 1.8f;
    tower_assets[2].rotate_y_offset = 0.8f;
}

void Game::update(float delta_step) {
    if (gameOver) return;

    static float spawn_timer_accumulator = 0;
    spawn_timer_accumulator += delta_step;
    if (spawn_timer_accumulator > 3.0f) {
        spawnTroop();
        spawn_timer_accumulator = 0;
    }

    for (auto it = troops.begin(); it != troops.end(); ) {
        if (it->currentWaypoint >= (int)pathWaypoints.size()) {
            lives--;
            it = troops.erase(it);
            continue;
        }

        float target_node_x = pathWaypoints[it->currentWaypoint].x;
        float target_node_z = pathWaypoints[it->currentWaypoint].z;
        float diff_x = target_node_x - it->x;
        float diff_z = target_node_z - it->z;
        float distance_to_node = sqrtf(diff_x * diff_x + diff_z * diff_z);

        if (distance_to_node < 0.2f) {
            it->currentWaypoint++;
        }
        else {
            it->x += (diff_x / distance_to_node) * it->speed * delta_step;
            it->z += (diff_z / distance_to_node) * it->speed * delta_step;
        }
        ++it;
    }

    for (auto& defense_unit : active_defenses) {
        float tracking_range = 30.0f;
        Troop* current_target = nullptr;

        for (auto& active_unit : troops) {
            float dx = active_unit.x - defense_unit.x;
            float dz = active_unit.z - defense_unit.z;
            float range_check = sqrtf(dx * dx + dz * dz);

            if (range_check < tracking_range) {
                tracking_range = range_check;
                current_target = &active_unit;
            }
        }

        if (current_target) {
            float aim_dx = current_target->x - defense_unit.x;
            float aim_dz = current_target->z - defense_unit.z;
            float calculated_angle = atan2f(aim_dx, aim_dz) * 180.0f / 3.14159f;

            float rotation_offset = calculated_angle - defense_unit.current_yaw;
            while (rotation_offset < -180) rotation_offset += 360;
            while (rotation_offset > 180) rotation_offset -= 360;

            defense_unit.current_yaw += rotation_offset * delta_step * 5.0f;

            if (fabs(rotation_offset) < 15.0f) {
                current_target->health -= 15.0f * delta_step;
            }
        }
    }

    for (auto it = troops.begin(); it != troops.end(); ) {
        if (it->health <= 0) {
            gold += 15;
            it = troops.erase(it);
        }
        else {
            ++it;
        }
    }

    if (lives <= 0) { lives = 0; gameOver = true; }
}

void Game::spawnTroop() {
    if (pathWaypoints.empty()) return;
    Troop t;
    t.x = pathWaypoints[0].x;
    t.z = pathWaypoints[0].z;
    t.currentWaypoint = 1;
    t.speed = 10.0f;
    t.health = 50.0f;
    troops.push_back(t);
}

static void renderMesh(const std::vector<VertexData>& mesh) {
    glBegin(GL_TRIANGLES);
    for (const auto& v : mesh) {
        glColor3f(v.r, v.g, v.b);
        glNormal3f(v.nx, v.ny, v.nz);
        glVertex3f(v.x, v.y, v.z);
    }
    glEnd();
}

void Game::render() {
    glBegin(GL_QUADS);
    glColor3f(0.2f, 0.5f, 0.2f);
    glNormal3f(0.0f, 1.0f, 0.0f);
    glVertex3f(-50.0f, -0.01f, -50.0f);
    glVertex3f(50.0f, -0.01f, -50.0f);
    glVertex3f(50.0f, -0.01f, 50.0f);
    glVertex3f(-50.0f, -0.01f, 50.0f);
    glEnd();

    glLineWidth(8.0f);
    glBegin(GL_LINE_STRIP);
    glColor3f(0.5f, 0.4f, 0.3f);
    for (const auto& path_node : pathWaypoints) {
        glVertex3f(path_node.x, 0.1f, path_node.z);
    }
    glEnd();

    for (const auto& active_unit : troops) {
        glPushMatrix();
        glTranslatef(active_unit.x, 0.5f, active_unit.z);
        glColor3f(0.8f, 0.1f, 0.1f);
        float unit_scale = 0.6f;
        glBegin(GL_QUADS);
        glNormal3f(0, 1, 0);
        glVertex3f(-unit_scale, unit_scale, -unit_scale);
        glVertex3f(unit_scale, unit_scale, -unit_scale);
        glVertex3f(unit_scale, unit_scale, unit_scale);
        glVertex3f(-unit_scale, unit_scale, unit_scale);
        glEnd();
        glPopMatrix();
    }

    for (const auto& defense_unit : active_defenses) {
        int type_index = (int)defense_unit.tower_variant;
        const auto& visual_assets = tower_assets[type_index];

        glPushMatrix();
        glTranslatef(defense_unit.x, visual_assets.base_y_offset, defense_unit.z);
        renderMesh(visual_assets.base_mesh);

        glRotatef(defense_unit.current_yaw, 0, 1, 0);
        glTranslatef(0.0f, visual_assets.rotate_y_offset, 0.0f);
        renderMesh(visual_assets.rotate_mesh);

        glTranslatef(0.0f, visual_assets.gun_y_offset, 0.0f);
        renderMesh(visual_assets.gun_mesh);

        glPopMatrix();

        for (const auto& potential_target : troops) {
            float dist_x = potential_target.x - defense_unit.x;
            float dist_z = potential_target.z - defense_unit.z;
            float range_sq = dist_x * dist_x + dist_z * dist_z;

            if (range_sq < 900.0f) {
                glDisable(GL_LIGHTING);
                glBegin(GL_LINES);
                glColor3f(1.0f, 0.8f, 0.0f);
                glVertex3f(defense_unit.x, 2.5f, defense_unit.z);
                glVertex3f(potential_target.x, 0.5f, potential_target.z);
                glEnd();
                glEnable(GL_LIGHTING);
                break;
            }
        }
    }
}

void Game::selectTowerType(int index) {
    if (index >= 0 && index <= 2)
        selectedType = (TowerType)index;
}

void Game::loadModels(std::string baseDir, std::string fileName, std::vector<VertexData>& targetVector) {
    tinyobj::attrib_t attrib;
    std::vector<tinyobj::shape_t> shapes;
    std::vector<tinyobj::material_t> materials;
    std::string warn, err;

    std::string full_path = baseDir + fileName;

    //nazwy plików
    //ostatni parametr mówi gdzie szukać pliku .mtl
    bool ret = tinyobj::LoadObj(&attrib, &shapes, &materials, &warn, &err, full_path.c_str(), baseDir.c_str());

    if (!warn.empty()) std::cout << "Ostrzeżenie: " << warn << std::endl;
    if (!err.empty()) std::cerr << "Błąd: " << err << std::endl;
    if (!ret) return;

    for (size_t s = 0; s < shapes.size(); s++) {
        size_t index_offset = 0;
        for (size_t f = 0; f < shapes[s].mesh.num_face_vertices.size(); f++) {
            int fv = shapes[s].mesh.num_face_vertices[f];
            int material_id = shapes[s].mesh.material_ids[f];

            for (size_t v = 0; v < fv; v++) {
                tinyobj::index_t idx = shapes[s].mesh.indices[index_offset + v];
                VertexData vertex;

                vertex.x = attrib.vertices[3 * idx.vertex_index + 0];
                vertex.y = attrib.vertices[3 * idx.vertex_index + 1];
                vertex.z = attrib.vertices[3 * idx.vertex_index + 2];

                if (idx.normal_index >= 0) {
                    vertex.nx = attrib.normals[3 * idx.normal_index + 0];
                    vertex.ny = attrib.normals[3 * idx.normal_index + 1];
                    vertex.nz = attrib.normals[3 * idx.normal_index + 2];
                }
                else {
                    vertex.nx = 0.0f; vertex.ny = 1.0f; vertex.nz = 0.0f;
                }

                if (material_id >= 0) {
                    vertex.r = materials[material_id].diffuse[0];
                    vertex.g = materials[material_id].diffuse[1];
                    vertex.b = materials[material_id].diffuse[2];
                }
                else {
                    vertex.r = 0.8f; vertex.g = 0.8f; vertex.b = 0.8f;
                }

                targetVector.push_back(vertex);
            }
            index_offset += fv;
        }
    }

    std::cout << "Zaladowano: " << fileName << " | wierzcholkow: " << targetVector.size() << std::endl;
}

void Game::loadTroopWave(std::string path) {
    std::ifstream wave_file(path);
    if (!wave_file.is_open()) return;

    std::string unit_type;
    float move_speed, max_hp;

    while (wave_file >> unit_type >> move_speed >> max_hp) {
        Troop t;
        t.x = pathWaypoints[0].x;
        t.z = pathWaypoints[0].z;
        t.currentWaypoint = 1;
        t.speed = move_speed;
        t.health = max_hp;
        t.type_name = unit_type;
        troops.push_back(t);
    }
    wave_file.close();
}

void Game::tryPlaceTower(int mx, int my, Camera& cam) {
    int idx = (int)selectedType;
    int cost = towerCost[idx];
    if (gold < cost) return;

    GLint viewport[4]; GLdouble mv[16], proj[16]; GLfloat wz; GLdouble wx, wy, wz_world;
    glGetIntegerv(GL_VIEWPORT, viewport);
    glGetDoublev(GL_MODELVIEW_MATRIX, mv);
    glGetDoublev(GL_PROJECTION_MATRIX, proj);
    glReadPixels(mx, viewport[3] - my, 1, 1, GL_DEPTH_COMPONENT, GL_FLOAT, &wz);
    gluUnProject(mx, viewport[3] - my, wz, mv, proj, viewport, &wx, &wy, &wz_world);

    TowerInstance t;
    t.x = (float)wx;
    t.z = (float)wz_world;
    t.current_yaw = 0.0f;
    t.tower_variant = selectedType;
    active_defenses.push_back(t);
    gold -= cost;

    std::cout << "Postawiono wieze typu " << idx << " | X=" << wx << " Z=" << wz_world << " | Zloto: " << gold << std::endl;
}