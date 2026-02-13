#include "PhysicsWorld.hpp"

PhysicsWorld::PhysicsWorld(float widthPixels, float heightPixels)
    : world(b2Vec2(0.0f, 0.0f)) 
{
    // SEED FIJA: Importante para que el video sea reproducible.
    // Cambiá este número y cambia toda la carrera.
    rng.seed(42); 

    worldWidthMeters = widthPixels / SCALE;
    worldHeightMeters = heightPixels / SCALE;

    createWalls(widthPixels, heightPixels);
    createRacers();
}

float PhysicsWorld::randomFloat(float min, float max) {
    std::uniform_real_distribution<float> dist(min, max);
    return dist(rng);
}

void PhysicsWorld::step(float timeStep, int velIter, int posIter) {
    world.Step(timeStep, velIter, posIter);

    for (b2Body* b : dynamicBodies) {
        // 1. Mantenimiento de Velocidad Lineal (El de siempre)
        b2Vec2 vel = b->GetLinearVelocity();
        float speed = vel.Length();
        
        if (speed < 15.0f || speed > 15.5f) { 
            vel.Normalize();
            vel *= 15.0f; 
            b->SetLinearVelocity(vel);
        }

        // 2. Mantenimiento de ROTACIÓN (El Sabor) [NUEVO]
        // Si giran muy lento, les damos un empujoncito en la dirección que ya tenían.
        float angularVel = b->GetAngularVelocity();
        if (std::abs(angularVel) < 2.0f) { // Si giran a menos de 2 rad/s
            // Mantenemos el signo del giro
            float boost = (angularVel > 0) ? 0.1f : -0.1f; 
            b->ApplyTorque(boost, true);
        }
    } 
}

const std::vector<b2Body*>& PhysicsWorld::getDynamicBodies() const {
    return dynamicBodies;
}



void PhysicsWorld::createWalls(float widthPixels, float heightPixels) {
    float width = worldWidthMeters;
    float height = worldHeightMeters;

    b2BodyDef wallBodyDef;
    wallBodyDef.type = b2_staticBody;

    b2Body* wall;

    b2PolygonShape wallShape;

    b2FixtureDef fixtureDef;
    fixtureDef.shape = &wallShape;
    fixtureDef.friction = 0.0f;
    fixtureDef.restitution = 1.0f;

    // Bottom
    wallBodyDef.position.Set(width / 2.0f, height);
    wall = world.CreateBody(&wallBodyDef);
    wallShape.SetAsBox(width / 2.0f, 0.5f);
    wall->CreateFixture(&fixtureDef);

    // Top
    wallBodyDef.position.Set(width / 2.0f, 0.0f);
    wall = world.CreateBody(&wallBodyDef);
    wallShape.SetAsBox(width / 2.0f, 0.5f);
    wall->CreateFixture(&fixtureDef);

    // Left
    wallBodyDef.position.Set(0.0f, height / 2.0f);
    wall = world.CreateBody(&wallBodyDef);
    wallShape.SetAsBox(0.5f, height / 2.0f);
    wall->CreateFixture(&fixtureDef);

    // Right
    wallBodyDef.position.Set(width, height / 2.0f);
    wall = world.CreateBody(&wallBodyDef);
    wallShape.SetAsBox(0.5f, height / 2.0f);
    wall->CreateFixture(&fixtureDef);
}

void PhysicsWorld::createRacers() {
    float size = 1.0f; 
    b2PolygonShape dynamicBox;
    dynamicBox.SetAsBox(size / 2.0f, size / 2.0f);

    b2FixtureDef fixtureDef;
    fixtureDef.shape = &dynamicBox;
    fixtureDef.density = 1.0f;
    fixtureDef.friction = 0.0f;     // Hielo
    fixtureDef.restitution = 1.0f;  // Rebote perfecto (el 1.02 lo sacamos para probar pureza con giro)

    for (int i = 0; i < 4; ++i) {
        b2BodyDef bodyDef;
        bodyDef.type = b2_dynamicBody;
        bodyDef.bullet = true;
        
        // POSICIÓN INICIAL: Un poco más separada y simétrica para empezar
        float x = (worldWidthMeters / 5.0f) * (i + 1);
        float y = worldHeightMeters / 2.0f;
        
        bodyDef.position.Set(x, y);

        // ÁNGULO INICIAL: Ya no arrancan derechos.
        // Esto hace que al primer choque, cada uno salga para cualquier lado.
        bodyDef.angle = randomFloat(0.0f, 6.28f); 

        b2Body* body = world.CreateBody(&bodyDef);
        body->CreateFixture(&fixtureDef);

        // VELOCIDAD INICIAL: Randomizada en dirección, pero magnitud 15.
        // Usamos trigonometría básica para darles una dirección al azar.
        float angle = randomFloat(0.0f, 6.28f);
        b2Vec2 initialVel(cos(angle) * 15.0f, sin(angle) * 15.0f);
        body->SetLinearVelocity(initialVel);

        // TORQUE INICIAL: Hacelos girar desde el segundo cero.
        // Entre -5 y 5 radianes por segundo.
        body->SetAngularVelocity(randomFloat(-5.0f, 5.0f));

        dynamicBodies.push_back(body);
    }
}
