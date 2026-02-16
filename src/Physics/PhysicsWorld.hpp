#pragma once

#include <box2d/box2d.h>
#include <vector>
#include <random>
#include <set>
#include <string>

// Estructura para paredes dinámicas
struct CustomWall {
    b2Body* body;
    float width;
    float height;
    // Visuals
    float flashTimer = 0.0f; // 1.0f = Blanco, 0.0f = Gris
};

class ChaosContactListener : public b2ContactListener {
public:
    std::set<b2Body*> bodiesToCheck;
    std::set<b2Body*> wallsHit; // Paredes golpeadas en este frame
    b2Body* winZoneBody = nullptr;
    b2Body* winnerBody = nullptr;

    void BeginContact(b2Contact* contact) override {
        b2Fixture* fa = contact->GetFixtureA();
        b2Fixture* fb = contact->GetFixtureB();

        b2Body* bodyA = fa->GetBody();
        b2Body* bodyB = fb->GetBody();

        // 1. Detección de Victoria
        if (winZoneBody) {
            if (bodyA == winZoneBody && bodyB->GetType() == b2_dynamicBody) winnerBody = bodyB;
            else if (bodyB == winZoneBody && bodyA->GetType() == b2_dynamicBody) winnerBody = bodyA;
        }

        // 2. Glitches (Dinámico vs Dinámico)
        if (fa->GetBody()->GetType() == b2_dynamicBody && fb->GetBody()->GetType() == b2_dynamicBody) {
            bodiesToCheck.insert(fa->GetBody());
            bodiesToCheck.insert(fb->GetBody());
        }

        // 3. Paredes Reactivas (Dinámico vs Estático)
        // Si A es dinámico y B estático (pared)
        if (bodyA->GetType() == b2_dynamicBody && bodyB->GetType() == b2_staticBody) {
            bodiesToCheck.insert(bodyA); // Glitch potential
            wallsHit.insert(bodyB);      // Flash visual
        }
        // Si B es dinámico y A estático (pared)
        else if (bodyB->GetType() == b2_dynamicBody && bodyA->GetType() == b2_staticBody) {
            bodiesToCheck.insert(bodyB); // Glitch potential
            wallsHit.insert(bodyA);      // Flash visual
        }
    }
};

class PhysicsWorld {
public:
    PhysicsWorld(float widthPixels, float heightPixels);

    void step(float timeStep, int velocityIterations, int positionIterations);
    
    // Método para actualizar los efectos visuales (Flashes)
    void updateWallVisuals(float dt);

    const std::vector<b2Body*>& getDynamicBodies() const;
    b2Body* getWinZoneBody() const;
    void resetRacers();

    // --- NUEVO SISTEMA DE MAPAS (SAVE/LOAD) ---
    void saveMap(const std::string& filename);
    void loadMap(const std::string& filename);
    void clearCustomWalls(); 
    // ------------------------------------------

    void addCustomWall(float x, float y, float w, float h);
    void updateCustomWall(int index, float x, float y, float w, float h);
    void removeCustomWall(int index);
    std::vector<CustomWall>& getCustomWalls(); // Sin const para poder modificar flashTimer

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
    
    std::vector<CustomWall> customWalls;

    b2Body* winZoneBody = nullptr;

    ChaosContactListener contactListener;
    std::mt19937 rng;

    float worldWidthMeters;
    float worldHeightMeters;
};