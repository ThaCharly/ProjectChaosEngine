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

    // Innecesariamente complicado para algo simple
	sf::RenderWindow window(sf::VideoMode(WIDTH, HEIGHT), std::to_string(WIDTH) + "x" + std::to_string(HEIGHT) + " - " + std::to_string(FPS) + "FPS");
	window.setFramerateLimit(FPS);

    if (!ImGui::SFML::Init(window)) {
        std::cerr << "Fallo al inicializar ImGui-SFML" << std::endl;
        return -1;
    }

    PhysicsWorld physics(WIDTH, HEIGHT);

    // Crear directorio de salida de grabaciones si no existe
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

    // Para crear la ventana y hacer que se cierre con el escape
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

        // --- ENGINEER CONSOLE ---
        ImGui::Begin("Engineer Console");
        
        // PAUSA
        if (physics.isPaused) {
            if (ImGui::Button("PLAY", ImVec2(100, 50))) physics.isPaused = false;
        } else {
            if (ImGui::Button("PAUSE", ImVec2(100, 50))) physics.isPaused = true;
        }

        ImGui::Separator();
        ImGui::Text("Physics Parameters");

        // VELOCIDAD
        ImGui::Checkbox("Enforce Speed", &physics.enforceSpeed);
        ImGui::DragFloat("Target Speed", &physics.targetSpeed, 0.0f, 50.0f);
        
        // TAMAÑO (Requiere actualización)
        float size = physics.currentRacerSize;
        if (ImGui::DragFloat("Racer Size (m)", &size, 0.1f, 5.0f)) {
            physics.updateRacerSize(size);
        }

        // RESTITUCIÓN (Rebote)
        float rest = physics.currentRestitution;
        if (ImGui::DragFloat("Bounciness", &rest, 0.0f, 2.0f)) {
            physics.updateRestitution(rest);
        }

        // ROTACIÓN
        bool fixedRot = physics.currentFixedRotation;
        if (ImGui::Checkbox("Fixed Rotation", &fixedRot)) {
            physics.updateFixedRotation(fixedRot);
        }

        // GRAVEDAD
        ImGui::Checkbox("Enable Gravity", &physics.enableGravity);

        ImGui::Separator();
        if (ImGui::Button("RESET SIMULATION", ImVec2(-1, 30))) {
            physics.resetRacers();
        }

        ImGui::Separator();
        ImGui::Text("Racers Data");

        const auto& bodies = physics.getDynamicBodies();
        for (size_t i = 0; i < bodies.size(); ++i) {
            b2Body* b = bodies[i];
            
            ImGui::PushID(i); // Para que ImGui no se confunda con los nombres iguales
            if (ImGui::TreeNode((std::string("Racer ") + std::to_string(i)).c_str())) {
                
                b2Vec2 pos = b->GetPosition();
                b2Vec2 vel = b->GetLinearVelocity();
                float p[2] = { pos.x, pos.y };
                float v[2] = { vel.x, vel.y };

                if (ImGui::DragFloat2("Pos", p, 0.1f)) {
                    b->SetTransform(b2Vec2(p[0], p[1]), b->GetAngle());
                    b->SetAwake(true);
                }
                if (ImGui::DragFloat2("Vel", v, 0.1f)) {
                    b->SetLinearVelocity(b2Vec2(v[0], v[1]));
                    b->SetAwake(true);
                }
                ImGui::TreePop();
            }
            ImGui::PopID();
        }

        ImGui::End();

        sf::Time dt = clock.restart();
        accumulator += dt.asSeconds();

        while (accumulator >= timeStep) {
        physics.step(timeStep, velocityIterations, positionIterations);
        accumulator -= timeStep;
        }


		window.clear(sf::Color(32,32,32)); // Gris oscuro

        

for (size_t i = 0; i < bodies.size(); ++i) {
            b2Body* body = bodies[i];
            b2Vec2 pos = body->GetPosition();
            float angle = body->GetAngle();

            sf::RectangleShape rect;
            rect.setSize(sf::Vector2f(PhysicsWorld::SCALE, PhysicsWorld::SCALE));
            rect.setOrigin(PhysicsWorld::SCALE / 2.0f, PhysicsWorld::SCALE / 2.0f);
            rect.setPosition(pos.x * PhysicsWorld::SCALE, pos.y * PhysicsWorld::SCALE);
            rect.setRotation(angle * 180.0f / b2_pi);

            switch (i) {
                case 0: rect.setFillColor(sf::Color::Cyan); break;
                case 1: rect.setFillColor(sf::Color::Magenta); break;
                case 2: rect.setFillColor(sf::Color::Green); break;
                case 3: rect.setFillColor(sf::Color::Yellow); break;
                default: rect.setFillColor(sf::Color::White); break;
            }
            window.draw(rect);
	}

        recorder.addFrame(window);
        ImGui::SFML::Render(window);

		window.display();
	}

    ImGui::SFML::Shutdown();
	return 0;
}
