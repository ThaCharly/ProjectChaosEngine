#pragma once

#include <box2d/box2d.h>
#include <vector>
#include <random>
#include <set>

// --- NUEVA ESTRUCTURA PARA PAREDES DINÁMICAS ---
// Guardamos el puntero al cuerpo físico y sus dimensiones para poder editarlo
struct CustomWall {
    b2Body* body;
    float width;
    float height;
};

class ChaosContactListener : public b2ContactListener {
public:
    std::set<b2Body*> bodiesToCheck;
    b2Body* winZoneBody = nullptr;
    b2Body* winnerBody = nullptr;

    void BeginContact(b2Contact* contact) override {
        b2Fixture* fa = contact->GetFixtureA();
        b2Fixture* fb = contact->GetFixtureB();

        b2Body* bodyA = fa->GetBody();
        b2Body* bodyB = fb->GetBody();

        if (winZoneBody) {
            if (bodyA == winZoneBody && bodyB->GetType() == b2_dynamicBody) {
                winnerBody = bodyB;
            }
            else if (bodyB == winZoneBody && bodyA->GetType() == b2_dynamicBody) {
                winnerBody = bodyA;
            }
        }

        if (fa->GetBody()->GetType() == b2_dynamicBody) bodiesToCheck.insert(fa->GetBody());
        if (fb->GetBody()->GetType() == b2_dynamicBody) bodiesToCheck.insert(fb->GetBody());
    }
};

class PhysicsWorld {
public:
    PhysicsWorld(float widthPixels, float heightPixels);

    void step(float timeStep, int velocityIterations, int positionIterations);

    const std::vector<b2Body*>& getDynamicBodies() const;
    b2Body* getWinZoneBody() const;
    void resetRacers();

    // --- NUEVOS MÉTODOS PARA GESTIÓN DE PAREDES ---
    // Crea una pared nueva en el centro
    void addCustomWall(float x, float y, float w, float h);
    // Modifica una pared existente (requiere borrar y crear fixture)
    void updateCustomWall(int index, float x, float y, float w, float h);
    // Borra una pared del mundo y del vector
    void removeCustomWall(int index);
    // Getter para dibujar las paredes en el main
    const std::vector<CustomWall>& getCustomWalls() const;

    static constexpr float SCALE = 30.0f;

    void updateRacerSize(float newSize);
    void updateRestitution(float newRest);
    void updateFixedRotation(bool fixed);
    void updateFriction(float newFriction);
    void updateWinZone(float x, float y, float w, float h);
    
    float currentRacerSize = 1.0f;
    float currentRestitution = 1.0f;
    float currentFriction = 0.0f;
    bool currentFixedRotation = true;

    float targetSpeed = 15.0f;
    bool enforceSpeed = true;
    bool enableGravity = false;
    bool isPaused = false;

    bool enableChaos = false;
    float chaosChance = 0.05f;
    float chaosBoost = 1.5f;

    bool gameOver = false;
    int winnerIndex = -1;

    float winZonePos[2] = {0.0f, 0.0f};
    float winZoneSize[2] = {2.0f, 2.0f};

private:
    void createWalls(float widthPixels, float heightPixels);
    void createRacers();
    void createWinZone();

    float randomFloat(float min, float max);

    b2World world;
    std::vector<b2Body*> dynamicBodies;
    
    // --- VECTOR DE PAREDES CREADAS ---
    std::vector<CustomWall> customWalls;

    b2Body* winZoneBody = nullptr;

    ChaosContactListener contactListener;
    std::mt19937 rng;

    float worldWidthMeters;
    float worldHeightMeters;
};