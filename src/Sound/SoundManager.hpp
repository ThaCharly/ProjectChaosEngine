#pragma once

#include <SFML/Audio.hpp>
#include <vector>
#include <map>
#include <cmath>
#include <iostream>

class SoundManager {
public:
    SoundManager() {
        // Generamos los 8 sonidos de la escala al iniciar
        // Frecuencias de Do Mayor (Octava 4/5)
        float frequencies[] = {
            261.63f, // 1: Do (C4)
            293.66f, // 2: Re (D4)
            329.63f, // 3: Mi (E4)
            349.23f, // 4: Fa (F4)
            392.00f, // 5: Sol (G4)
            440.00f, // 6: La (A4)
            493.88f, // 7: Si (B4)
            523.25f  // 8: Do (C5)
        };

        for (int i = 0; i < 8; ++i) {
            generateTone(i + 1, frequencies[i]);
        }

        // Pool de voces para polifonía
        for(int i=0; i<32; ++i) soundPool.emplace_back();
    }

    // Ya no cargamos archivos, generamos ondas matemáticas
    void generateTone(int id, float frequency) {
        const unsigned SAMPLES = 44100; // 1 segundo de audio (aunque usaremos menos)
        const unsigned SAMPLE_RATE = 44100;
        const int AMPLITUDE = 30000;

        std::vector<sf::Int16> rawSamples;
        
        // Duración del "ping": 0.3 segundos
        float duration = 0.3f; 
        int numSamples = (int)(SAMPLE_RATE * duration);

        for (int i = 0; i < numSamples; i++) {
            float t = (float)i / SAMPLE_RATE;
            
            // ONDA SENOIDAL (Suena a "bip" suave)
            // Si querés que suene más "NES", cambiá sin() por una onda cuadrada.
            float sample = AMPLITUDE * std::sin(2 * 3.14159f * frequency * t);

            // ENVELOPE (Decay exponencial)
            // Hacemos que el volumen baje rápido para que sea un golpe percusivo
            float decay = std::exp(-15.0f * t); 
            
            rawSamples.push_back((sf::Int16)(sample * decay));
        }

        sf::SoundBuffer buffer;
        if (buffer.loadFromSamples(&rawSamples[0], rawSamples.size(), 1, SAMPLE_RATE)) {
            soundBuffers[id] = buffer;
            // std::cout << "[SYNTH] Generated Tone ID " << id << " (" << frequency << "Hz)" << std::endl;
        }
    }

    void playSound(int id, float xPosition, float worldWidth) {
        // Si el ID es 0 o no existe, no hacemos nada
        if (id <= 0 || soundBuffers.find(id) == soundBuffers.end()) return;

        sf::Sound* sound = getFreeSound();
        if (!sound) return;

        sound->setBuffer(soundBuffers[id]);
        
        // Volumen fijo y alto, como pediste
        sound->setVolume(80.0f); 
        sound->setPitch(1.0f); // Tono perfecto, sin variación random

        // Mantenemos el Paneo Estéreo porque le da mucha calidad
        float normalizedX = (xPosition / worldWidth) * 2.0f - 1.0f; 
        sound->setPosition(normalizedX * 5.0f, 0.0f, 5.0f);
        sound->setMinDistance(5.0f);
        sound->setAttenuation(1.0f);

        sound->play();
    }

private:
    sf::Sound* getFreeSound() {
        for (auto& s : soundPool) {
            if (s.getStatus() == sf::Sound::Stopped) return &s;
        }
        return &soundPool[0]; // Robar canal si está todo lleno
    }

    std::map<int, sf::SoundBuffer> soundBuffers;
    std::vector<sf::Sound> soundPool;
};