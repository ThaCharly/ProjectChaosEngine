#pragma once

#include <box2d/box2d.h>
#include <vector>
#include <random>
#include <set>

class ChaosContactListener : public b2ContactListener {
public:
    std::set<b2Body*> bodiesToCheck; // Para los glitches
    b2Body* winZoneBody = nullptr;   // Referencia a la meta
    b2Body* winnerBody = nullptr;    // El racer que tocó la meta

    void BeginContact(b2Contact* contact) override {
        b2Fixture* fa = contact->GetFixtureA();
        b2Fixture* fb = contact->GetFixtureB();

        b2Body* bodyA = fa->GetBody();
        b2Body* bodyB = fb->GetBody();

        // 1. Detección de Victoria
        if (winZoneBody) {
            if (bodyA == winZoneBody && bodyB->GetType() == b2_dynamicBody) {
                winnerBody = bodyB;
            }
            else if (bodyB == winZoneBody && bodyA->GetType() == b2_dynamicBody) {
                winnerBody = bodyA;
            }
        }

        // 2. Lógica de Glitches (La de siempre)
        if (fa->GetBody()->GetType() == b2_dynamicBody) bodiesToCheck.insert(fa->GetBody());
        if (fb->GetBody()->GetType() == b2_dynamicBody) bodiesToCheck.insert(fb->GetBody());
    }
};

class PhysicsWorld {
public:
    PhysicsWorld(float widthPixels, float heightPixels);

    void step(float timeStep, int velocityIterations, int positionIterations);

    const std::vector<b2Body*>& getDynamicBodies() const;
    b2Body* getWinZoneBody() const; // Para dibujarlo
    void resetRacers();

    static constexpr float SCALE = 30.0f;

    void updateRacerSize(float newSize);       // Recrea la forma física
    void updateRestitution(float newRest);     // Cambia el rebote
    void updateFixedRotation(bool fixed);      // Traba/Destraba rotación
    void updateFriction(float newFriction);
    void updateWinZone(float x, float y, float w, float h);
    float currentRacerSize = 1.0f;
    float currentRestitution = 1.0f;
    float currentFriction = 0.0f;
    bool currentFixedRotation = true;


    float targetSpeed = 15.0f;     // Velocidad objetivo
    bool enforceSpeed = true;      // ¿Forzamos la velocidad constante?
    bool enableGravity = false;    // Por si querés probar gravedad
    bool isPaused = false;

    bool enableChaos = false;      // Switch maestro para glitches
    float chaosChance = 0.05f;    // 5% probabilidad
    float chaosBoost = 1.5f;      // Multiplicador de empuje

    bool gameOver = false;
    int winnerIndex = -1; // 0 a 3, o -1 si nadie ganó

    // Datos de la Meta (para la UI)
    float winZonePos[2] = {0.0f, 0.0f};
    float winZoneSize[2] = {2.0f, 2.0f};

private:
    void createWalls(float widthPixels, float heightPixels);
    void createRacers();
    void createWinZone();

    float randomFloat(float min, float max);

    b2World world;
    std::vector<b2Body*> dynamicBodies;
    b2Body* winZoneBody = nullptr;

    ChaosContactListener contactListener;
    std::mt19937 rng;

    float worldWidthMeters;
    float worldHeightMeters;
};
