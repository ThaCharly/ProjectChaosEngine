#pragma once

#include <box2d/box2d.h>
#include <vector>

class PhysicsWorld {
public:
    PhysicsWorld(float widthPixels, float heightPixels);

    void step(float timeStep, int velocityIterations, int positionIterations);

    const std::vector<b2Body*>& getDynamicBodies() const;
    void resetRacers();

    static constexpr float SCALE = 30.0f;

    void updateRacerSize(float newSize);       // Recrea la forma física
    void updateRestitution(float newRest);     // Cambia el rebote
    void updateFixedRotation(bool fixed);      // Traba/Destraba rotación
    void updateFriction(float newFriction);
    float currentRacerSize = 1.0f;
    float currentRestitution = 1.0f;
    float currentFriction = 0.0f;
    bool currentFixedRotation = true;


    float targetSpeed = 15.0f;     // Velocidad objetivo
    bool enforceSpeed = true;      // ¿Forzamos la velocidad constante?
    bool enableGravity = false;    // Por si querés probar gravedad
    bool isPaused = false;

private:
    void createWalls(float widthPixels, float heightPixels);
    void createRacers();

    b2World world;
    std::vector<b2Body*> dynamicBodies;

    float worldWidthMeters;
    float worldHeightMeters;
};
