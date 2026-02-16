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
    // --- CONFIGURACIÓN BASE ---
    const unsigned int WIDTH = 720;
    const unsigned int HEIGHT = 720;
    const unsigned int FPS = 60;
    const std::string VIDEO_DIRECTORY = "../output/video.mp4";

    sf::RenderWindow window(sf::VideoMode(WIDTH, HEIGHT), "ChaosEngine - Dual Deck");
    window.setFramerateLimit(FPS);

    if (!ImGui::SFML::Init(window)) {
        std::cerr << "Fallo al inicializar ImGui-SFML" << std::endl;
        return -1;
    }

    // 1. FÍSICA E INICIALIZACIÓN
    PhysicsWorld physics(WIDTH, HEIGHT);
    physics.isPaused = true; // Arrancamos en PAUSA

    // 2. SISTEMA DE ARCHIVOS
    fs::path videoPath(VIDEO_DIRECTORY);
    fs::path outputDir = videoPath.parent_path();
    if (!fs::exists(outputDir)) fs::create_directories(outputDir);

    // 3. GRABADORA
    Recorder recorder(WIDTH, HEIGHT, FPS, VIDEO_DIRECTORY);
    recorder.isRecording = false; // Arrancamos SIN GRABAR

    // 4. RELOJES
    const float timeStep = 1.0f / 60.0f;
    int32 velIter = 8;
    int32 posIter = 3;

    sf::Clock clock;
    sf::Clock deltaClock;
    float accumulator = 0.0f;

    // --- DATOS PARA LA UI (ESTO FALTABA) ---
    // Nombres para mostrar en el inspector
    const char* racerNames[] = { "Cyan", "Magenta", "Green", "Yellow" };
    
    // Colores para los headers de ImGui
    ImVec4 guiColors[] = {
        ImVec4(0, 1, 1, 1),   // Cyan
        ImVec4(1, 0, 1, 1),   // Magenta
        ImVec4(0, 1, 0, 1),   // Green
        ImVec4(1, 1, 0, 1)    // Yellow
    };

    // Colores para dibujar en SFML
    sf::Color racerColors[] = {
        sf::Color::Cyan, sf::Color::Magenta, sf::Color::Green, sf::Color::Yellow
    };

    while (window.isOpen()) {

        // --- EVENTOS ---
        sf::Event event;
        while (window.pollEvent(event)) {
            ImGui::SFML::ProcessEvent(window, event);
            if (event.type == sf::Event::Closed) window.close();
            if (event.type == sf::Event::KeyPressed && event.key.code == sf::Keyboard::Escape) window.close();
        }

        ImGui::SFML::Update(window, deltaClock.restart());

        // ============================================================
        //                VENTANA 1: DIRECTOR DECK
        // ============================================================
        
        ImGui::SetNextWindowPos(ImVec2(10, 10), ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowSize(ImVec2(350, 550), ImGuiCond_FirstUseEver);
        
        ImGui::Begin("Director Control", nullptr);

        // A. CÁMARA (GRABACIÓN)
        ImGui::TextColored(ImVec4(1, 0.2f, 0.2f, 1), "CAMERA & ACTION");
        if (recorder.isRecording) {
            ImGui::PushStyleColor(ImGuiCol_Button, (ImVec4)ImColor::HSV(0.0f, 0.6f, 0.6f));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, (ImVec4)ImColor::HSV(0.0f, 0.7f, 0.7f));
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, (ImVec4)ImColor::HSV(0.0f, 0.8f, 0.8f));
            if (ImGui::Button("STOP REC", ImVec2(-1, 40))) recorder.isRecording = false;
            ImGui::PopStyleColor(3);
        } else {
            ImGui::PushStyleColor(ImGuiCol_Button, (ImVec4)ImColor::HSV(0.33f, 0.6f, 0.6f));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, (ImVec4)ImColor::HSV(0.33f, 0.7f, 0.7f));
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, (ImVec4)ImColor::HSV(0.33f, 0.8f, 0.8f));
            if (ImGui::Button("START REC", ImVec2(-1, 40))) recorder.isRecording = true;
            ImGui::PopStyleColor(3);
        }

        // B. SIMULACIÓN
        ImGui::Spacing();
        if (physics.isPaused) {
            if (ImGui::Button("RESUME PHYS", ImVec2(-1, 30))) physics.isPaused = false;
        } else {
            if (ImGui::Button("PAUSE PHYS", ImVec2(-1, 30))) physics.isPaused = true;
        }
        if (ImGui::Button("RESET RACE (Positions)", ImVec2(-1, 20))) physics.resetRacers();

        ImGui::Separator();

        // C. LA META
        ImGui::TextColored(ImVec4(1, 0.8f, 0, 1), "WIN ZONE CONFIG");
        bool updateZone = false;
        updateZone |= ImGui::DragFloat2("Posicion (m)", physics.winZonePos, 0.1f);
        updateZone |= ImGui::DragFloat2("Tamaño (m)", physics.winZoneSize, 0.1f, 0.1f, 30.0f);
        if (updateZone) {
            physics.updateWinZone(physics.winZonePos[0], physics.winZonePos[1], 
                                  physics.winZoneSize[0], physics.winZoneSize[1]);
        }

        ImGui::Separator();

        // D. CAOS Y GRAVEDAD
        ImGui::TextColored(ImVec4(1, 0.5f, 0, 1), "GLOBAL PHYSICS");
        ImGui::Checkbox("Enable Gravity", &physics.enableGravity);
        ImGui::Checkbox("Enable Glitches (Chaos)", &physics.enableChaos);
        if (physics.enableChaos) {
            ImGui::SliderFloat("Chaos %", &physics.chaosChance, 0.0f, 0.5f);
            ImGui::SliderFloat("Chaos Boost", &physics.chaosBoost, 1.0f, 3.0f);
        }
        
        ImGui::End(); // Fin Ventana 1


        // ============================================================
        //                VENTANA 2: RACER INSPECTOR
        // ============================================================
        
        ImGui::SetNextWindowPos(ImVec2(370, 10), ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowSize(ImVec2(340, 600), ImGuiCond_FirstUseEver);

        ImGui::Begin("Racer Inspector", nullptr);

        // A. CONFIGURACIÓN GLOBAL
        ImGui::TextColored(ImVec4(0.5f, 0.5f, 1.0f, 1), "FLEET SETTINGS");
        
        float size = physics.currentRacerSize;
        if (ImGui::DragFloat("Size (m)", &size, 0.05f, 0.1f, 10.0f, "%.2f")) {
            physics.updateRacerSize(size);
        }

        float rest = physics.currentRestitution;
        if (ImGui::DragFloat("Bounce", &rest, 0.01f, 0.0f, 2.0f, "%.2f")) {
            physics.updateRestitution(rest);
        }

        bool fixedRot = physics.currentFixedRotation;
        if (ImGui::Checkbox("Fixed Rotation", &fixedRot)) {
            physics.updateFixedRotation(fixedRot);
        }

        ImGui::Checkbox("Enforce Speed", &physics.enforceSpeed);
        ImGui::DragFloat("Target Vel", &physics.targetSpeed, 0.5f, 0.0f, 100.0f, "%.1f m/s");

        ImGui::Separator();

        // B. INSPECTOR INDIVIDUAL
        ImGui::Text("INDIVIDUAL CONTROLS");
        
        const auto& bodies = physics.getDynamicBodies();
        
        for (size_t i = 0; i < bodies.size(); ++i) {
            b2Body* b = bodies[i];
            
            // Usamos colores definidos arriba
            ImGui::PushStyleColor(ImGuiCol_Header, guiColors[i % 4]);
            ImGui::PushID((int)i);

            // Nombre seguro para evitar ambigüedades
            std::string headerName;
            if (i < 4) {
                headerName = std::string(racerNames[i]);
            } else {
                headerName = "Racer " + std::to_string(i);
            }
            
            if (ImGui::CollapsingHeader(headerName.c_str())) {
                ImGui::PopStyleColor(); 
                
                // Posición
                b2Vec2 pos = b->GetPosition();
                float p[2] = { pos.x, pos.y };
                if (ImGui::DragFloat2("Pos", p, 0.1f)) {
                    b->SetTransform(b2Vec2(p[0], p[1]), b->GetAngle());
                    b->SetAwake(true);
                }

                // Velocidad
                b2Vec2 vel = b->GetLinearVelocity();
                float v[2] = { vel.x, vel.y };
                if (ImGui::DragFloat2("Vel", v, 0.1f)) {
                    b->SetLinearVelocity(b2Vec2(v[0], v[1]));
                    b->SetAwake(true);
                }

                // Info Rotación
                float angleDeg = b->GetAngle() * 180.0f / 3.14159f;
                ImGui::Text("Angle: %.1f deg", angleDeg);
                if (ImGui::Button("Zero Rot")) {
                    b->SetTransform(b->GetPosition(), 0.0f);
                    b->SetAngularVelocity(0.0f);
                }

            } else {
                ImGui::PopStyleColor();
            }
            ImGui::PopID();
        }

        ImGui::End(); // Fin Ventana 2


        // --- UPDATE FÍSICA ---
        sf::Time dt = clock.restart();
        if (!physics.isPaused) {
            accumulator += dt.asSeconds();
            while (accumulator >= timeStep) {
                physics.step(timeStep, velIter, posIter);
                accumulator -= timeStep;
            }
        } else {
            accumulator = 0.0f;
        }

        if (physics.gameOver) {
            std::cout << ">>> WINNER: " << physics.winnerIndex << std::endl;
            window.close();
        }

        // --- RENDER ---
        window.clear(sf::Color(18, 18, 18)); 

        // Meta
        b2Body* zone = physics.getWinZoneBody();
        if (zone) {
            b2Vec2 pos = zone->GetPosition();
            sf::RectangleShape zoneRect;
            float w = physics.winZoneSize[0] * PhysicsWorld::SCALE;
            float h = physics.winZoneSize[1] * PhysicsWorld::SCALE;
            zoneRect.setSize(sf::Vector2f(w, h));
            zoneRect.setOrigin(w/2.0f, h/2.0f);
            zoneRect.setPosition(pos.x * PhysicsWorld::SCALE, pos.y * PhysicsWorld::SCALE);
            zoneRect.setFillColor(sf::Color(255, 215, 0, 80)); 
            zoneRect.setOutlineColor(sf::Color::Yellow);
            zoneRect.setOutlineThickness(2.0f);
            window.draw(zoneRect);
        }

        // Racers
        for (size_t i = 0; i < bodies.size(); ++i) {
            b2Body* body = bodies[i];
            b2Vec2 pos = body->GetPosition();
            float angle = body->GetAngle();
            float drawSize = physics.currentRacerSize * PhysicsWorld::SCALE;

            sf::RectangleShape rect;
            rect.setSize(sf::Vector2f(drawSize, drawSize));
            rect.setOrigin(drawSize / 2.0f, drawSize / 2.0f);
            rect.setPosition(pos.x * PhysicsWorld::SCALE, pos.y * PhysicsWorld::SCALE);
            rect.setRotation(angle * 180.0f / 3.14159f);

            if (i < 4) rect.setFillColor(racerColors[i]);
            else rect.setFillColor(sf::Color::White);

            window.draw(rect);
        }

        recorder.addFrame(window);
        ImGui::SFML::Render(window);
        window.display();
    }

    ImGui::SFML::Shutdown();
    return 0;
}