#pragma once

#include <box2d/box2d.h>
#include <vector>
#include <random> // <--- Nuevo

class PhysicsWorld {
public:
    PhysicsWorld(float widthPixels, float heightPixels);

    void step(float timeStep, int velocityIterations, int positionIterations);
    const std::vector<b2Body*>& getDynamicBodies() const;

    static constexpr float SCALE = 30.0f;

private:
    void createWalls(float widthPixels, float heightPixels);
    void createRacers();

    // Helper para random floats entre min y max
    float randomFloat(float min, float max);

    b2World world;
    std::vector<b2Body*> dynamicBodies;

    float worldWidthMeters;
    float worldHeightMeters;

    std::mt19937 rng; // <--- El motor de aleatoriedad
};