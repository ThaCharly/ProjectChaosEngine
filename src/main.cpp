#include <SFML/Graphics.hpp>
#include <SFML/Window.hpp>
#include <box2d/box2d.h>
#include <imgui.h>
#include <imgui-SFML.h>
#include <iostream>
#include <filesystem>
#include <string>

#include "Physics/PhysicsWorld.hpp"
#include "Recorder/Recorder.hpp"

namespace fs = std::filesystem;

int main()
{
    const unsigned int WIDTH = 720;
    const unsigned int HEIGHT = 720;
    const unsigned int FPS = 60;
    const std::string VIDEO_DIRECTORY = "../output/video.mp4";

    sf::RenderWindow window(sf::VideoMode(WIDTH, HEIGHT), "ChaosEngine - Director Mode");
    window.setFramerateLimit(FPS);

    if (!ImGui::SFML::Init(window)) {
        std::cerr << "Fallo al inicializar ImGui-SFML" << std::endl;
        return -1;
    }

    PhysicsWorld physics(WIDTH, HEIGHT);

    fs::path videoPath(VIDEO_DIRECTORY);
    fs::path outputDir = videoPath.parent_path();
    if (!fs::exists(outputDir)) {
        fs::create_directories(outputDir);
    }

    Recorder recorder(WIDTH, HEIGHT, FPS, VIDEO_DIRECTORY);

    const float timeStep = 1.0f / 60.0f;
    int32 velocityIterations = 8;
    int32 positionIterations = 3;

    sf::Clock clock;
    sf::Clock deltaClock;
    float accumulator = 0.0f;

    // Colores para los racers
    sf::Color racerColors[] = {
        sf::Color::Cyan, sf::Color::Magenta, sf::Color::Green, sf::Color::Yellow
    };

    while (window.isOpen()) {

        sf::Event event;
        while (window.pollEvent(event)) {
            ImGui::SFML::ProcessEvent(window, event);
            if (event.type == sf::Event::Closed)
                window.close();
            if (event.type == sf::Event::KeyPressed && event.key.code == sf::Keyboard::Escape)
                window.close();
        }

        ImGui::SFML::Update(window, deltaClock.restart());

        // --- DIRECTOR CONSOLE (Panel de Control Completo) ---
        ImGui::Begin("Chaos Director");
        
        // 1. CONTROL DE LA META (WIN ZONE)
        ImGui::TextColored(ImVec4(1, 0.8f, 0, 1), "WIN ZONE SETTINGS");
        bool updateZone = false;
        updateZone |= ImGui::DragFloat2("Posicion (m)", physics.winZonePos, 0.1f);
        updateZone |= ImGui::DragFloat2("Tamaño (m)", physics.winZoneSize, 0.1f, 0.1f, 20.0f);
        
        if (updateZone) {
            physics.updateWinZone(physics.winZonePos[0], physics.winZonePos[1], 
                                  physics.winZoneSize[0], physics.winZoneSize[1]);
        }

        ImGui::Separator();
        
        // 2. GLITCHES
        ImGui::TextColored(ImVec4(1, 0.4f, 0, 1), "CHAOS ENGINE");
        ImGui::Checkbox("Enable Glitches", &physics.enableChaos);
        ImGui::SliderFloat("Glitch Chance", &physics.chaosChance, 0.0f, 0.5f, "%.2f");
        ImGui::SliderFloat("Glitch Boost", &physics.chaosBoost, 1.0f, 3.0f, "x%.1f");

        ImGui::Separator();

        // 3. FÍSICA Y CONTROLES
        if (physics.isPaused) {
            if (ImGui::Button("RESUME", ImVec2(-1, 40))) physics.isPaused = false;
        } else {
            if (ImGui::Button("PAUSE", ImVec2(-1, 40))) physics.isPaused = true;
        }

        ImGui::Checkbox("Enforce Speed", &physics.enforceSpeed);
        ImGui::DragFloat("Target Speed", &physics.targetSpeed, 0.5f, 0.0f, 100.0f);
        
        float size = physics.currentRacerSize;
        if (ImGui::DragFloat("Racer Size", &size, 0.05f, 0.1f, 5.0f)) physics.updateRacerSize(size);

        float rest = physics.currentRestitution;
        if (ImGui::DragFloat("Bounciness", &rest, 0.01f, 0.0f, 2.0f)) physics.updateRestitution(rest);

        bool fixedRot = physics.currentFixedRotation;
        if (ImGui::Checkbox("Fixed Rotation", &fixedRot)) physics.updateFixedRotation(fixedRot);

        if (ImGui::Button("RESET RACE", ImVec2(-1, 30))) physics.resetRacers();

        ImGui::End();
        // ----------------------------------------------------

        sf::Time dt = clock.restart();
        accumulator += dt.asSeconds();

        while (accumulator >= timeStep) {
            physics.step(timeStep, velocityIterations, positionIterations);
            accumulator -= timeStep;
        }

        // CHECK VICTORY
        if (physics.gameOver) {
            std::cout << ">>> SIMULACIÓN TERMINADA. Ganador: Racer " << physics.winnerIndex << std::endl;
            window.close();
        }

        window.clear(sf::Color(18, 18, 18)); 

        // 1. DIBUJAR WIN ZONE (Fondo)
        b2Body* zone = physics.getWinZoneBody();
        if (zone) {
            b2Vec2 pos = zone->GetPosition();
            sf::RectangleShape zoneRect;
            float w = physics.winZoneSize[0] * PhysicsWorld::SCALE;
            float h = physics.winZoneSize[1] * PhysicsWorld::SCALE;
            
            zoneRect.setSize(sf::Vector2f(w, h));
            zoneRect.setOrigin(w / 2.0f, h / 2.0f);
            zoneRect.setPosition(pos.x * PhysicsWorld::SCALE, pos.y * PhysicsWorld::SCALE);
            
            // Dorado transparente
            zoneRect.setFillColor(sf::Color(255, 215, 0, 80)); 
            zoneRect.setOutlineColor(sf::Color::Yellow);
            zoneRect.setOutlineThickness(2.0f);
            
            window.draw(zoneRect);
        }

        // 2. DIBUJAR RACERS
        const auto& bodies = physics.getDynamicBodies();
        for (size_t i = 0; i < bodies.size(); ++i) {
            b2Body* body = bodies[i];
            b2Vec2 pos = body->GetPosition();
            float angle = body->GetAngle();
            float drawSize = physics.currentRacerSize * PhysicsWorld::SCALE;

            sf::RectangleShape rect;
            rect.setSize(sf::Vector2f(drawSize, drawSize));
            rect.setOrigin(drawSize / 2.0f, drawSize / 2.0f);
            rect.setPosition(pos.x * PhysicsWorld::SCALE, pos.y * PhysicsWorld::SCALE);
            rect.setRotation(angle * 180.0f / b2_pi);

            if (i < 4) rect.setFillColor(racerColors[i]);
            else rect.setFillColor(sf::Color::White);

            window.draw(rect);
        }

        // 3. GRABAR (Video Limpio, sin GUI)
        recorder.addFrame(window);

        // 4. GUI & DISPLAY
        ImGui::SFML::Render(window);
        window.display();
    }

    ImGui::SFML::Shutdown();
    return 0;
}