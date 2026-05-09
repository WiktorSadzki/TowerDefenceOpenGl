#define TINYOBJLOADER_IMPLEMENTATION
#include "tiny_obj_loader.h"
#include <iostream>
#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <GL/glu.h>
#include "Game.h"

void Game::init() {
    lives = 20; gold = 100; wave = 1; gameOver = false;
    towers.clear();
    loadModels();
}

void Game::update(float dt) {
    if (gameOver) return;
    if (lives <= 0) { lives = 0; gameOver = true; }
}

void Game::render() {
    glBegin(GL_QUADS);
    glColor3f(0.4f, 0.8f, 0.4f);
    glNormal3f(0.0f, 1.0f, 0.0f);
    glVertex3f(-50.0f, 0.0f, -50.0f); glVertex3f(50.0f, 0.0f, -50.0f);
    glVertex3f(50.0f, 0.0f, 50.0f); glVertex3f(-50.0f, 0.0f, 50.0f);
    glEnd();

    for (const auto& t : towers) {
        glPushMatrix();
        glTranslatef(t.x, 0.0f, t.z);

        // skalowanie
        glScalef(0.1f, 0.1f, 0.1f); 
        glBegin(GL_TRIANGLES);
        for (const auto& v : towerModel) {
            glColor3f(v.r, v.g, v.b);       // Kolor
            glNormal3f(v.nx, v.ny, v.nz);   // Normalna
            glVertex3f(v.x, v.y, v.z);      // Pozycja
        }
        glEnd();

        glPopMatrix();
    }
}

void Game::loadModels() {
    tinyobj::attrib_t attrib;
    std::vector<tinyobj::shape_t> shapes;
    std::vector<tinyobj::material_t> materials;
    std::string warn, err;

    //nazwy plików
    //ostatni parametr mówi gdzie szukać pliku .mtl
    bool ret = tinyobj::LoadObj(&attrib, &shapes, &materials, &warn, &err, 
        "Assets/Towers/source/Base.obj", "Assets/Towers/source/");

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

                towerModel.push_back(vertex);
            }
            index_offset += fv;
        }
    }

    std::cout << "--- RAPORT Z LADOWANIA ---" << std::endl;
    std::cout << "Liczba wierzcholkow modelu: " << towerModel.size() << std::endl;
}

void Game::tryPlaceTower(int mx, int my, Camera& cam) {
    if (gold < 20) return;
    GLint viewport[4]; GLdouble mv[16], proj[16]; GLfloat wz; GLdouble wx, wy, wz_world;
    glGetIntegerv(GL_VIEWPORT, viewport);
    glGetDoublev(GL_MODELVIEW_MATRIX, mv);
    glGetDoublev(GL_PROJECTION_MATRIX, proj);
    glReadPixels(mx, viewport[3] - my, 1, 1, GL_DEPTH_COMPONENT, GL_FLOAT, &wz);
    gluUnProject(mx, viewport[3] - my, wz, mv, proj, viewport, &wx, &wy, &wz_world);
    towers.push_back({ (float)wx, (float)wz_world });
    gold -= 20;

    std::cout << "Postawiono wieze! Wspolrzedne: X=" << wx << " Z=" << wz_world << " | Zostalo zlota: " << gold << std::endl;
    std::cout << "Wszystkich wiez na mapie: " << towers.size() << std::endl;
}