#pragma once

#include <SFML/Audio.hpp>
#include <vector>
#include <map>
#include <cmath>
#include <iostream>
#include <random>

// Forward declaration
class Recorder;

class SoundManager {
public:
    SoundManager() {
        // Inicializar RNG
        rng.seed(std::random_device{}());

        // Frecuencias de Do Mayor (Octava 4/5)
        float frequencies[] = {
            261.63f, 293.66f, 329.63f, 349.23f, 
            392.00f, 440.00f, 493.88f, 523.25f 
        };

        for (int i = 0; i < 8; ++i) {
            generateTone(i + 1, frequencies[i]);
        }

        // Pool de 32 voces
        for(int i=0; i<32; ++i) soundPool.emplace_back();
    }

    void setRecorder(Recorder* rec) {
        recorder = rec;
    }

    void generateTone(int id, float frequency) {
        const unsigned SAMPLE_RATE = 44100;
        
        // AJUSTE CRÍTICO: Bajamos la amplitud base. 
        // 20000 deja margen para que suenen ~3 notas juntas sin saturar antes del normalizador.
        const int AMPLITUDE = 20000; 

        std::vector<sf::Int16> rawSamples;
        float duration = 0.3f; 
        int numSamples = (int)(SAMPLE_RATE * duration);

        float attackTime = 0.005f; // 5ms de ataque suave

        for (int i = 0; i < numSamples; i++) {
            float t = (float)i / SAMPLE_RATE;
            
            // --- ONDA PURA (8-BIT STYLE) ---
            float wave = std::sin(2 * 3.14159f * frequency * t);
            
            // ENVELOPE (ADSR Simplificado)
            float envelope = 0.0f;
            if (t < attackTime) {
                envelope = t / attackTime; // Fade In
            } else {
                envelope = std::exp(-10.0f * (t - attackTime)); // Fade Out
            }

            rawSamples.push_back((sf::Int16)(wave * envelope * AMPLITUDE));
        }

        sf::SoundBuffer buffer;
        if (buffer.loadFromSamples(&rawSamples[0], rawSamples.size(), 1, SAMPLE_RATE)) {
            soundBuffers[id] = buffer;
        }
    }

    void playSound(int id, float xPosition, float worldWidth) {
        if (id <= 0 || soundBuffers.find(id) == soundBuffers.end()) return;

        // 1. REPRODUCIR EN VIVO (SFML)
        sf::Sound* sound = getFreeSound();
        if (sound) {
            sound->setBuffer(soundBuffers[id]);
            sound->setVolume(80.0f); // Volumen alto y claro
            
            // --- AFINACIÓN PERFECTA ---
            sound->setPitch(1.0f); // Sin detune
            
            // Paneo Estéreo (Mantenemos la espacialidad)
            float normalizedX = (xPosition / worldWidth) * 2.0f - 1.0f; 
            sound->setPosition(normalizedX * 10.0f, 0.0f, 0.0f); 
            sound->setMinDistance(5.0f);
            sound->setAttenuation(1.0f);

            sound->play();
        }

        // 2. ENVIAR A LA GRABADORA
        if (recorder) {
             const sf::SoundBuffer& buf = soundBuffers[id];
             // Enviamos al recorder. El normalizador del recorder se encargará si satura.
             sendToRecorder(buf.getSamples(), buf.getSampleCount(), 80.0f);
        }
    }

private:
    sf::Sound* getFreeSound() {
        for (auto& s : soundPool) {
            if (s.getStatus() == sf::Sound::Stopped) return &s;
        }
        return &soundPool[0]; 
    }

    void sendToRecorder(const sf::Int16* samples, std::size_t count, float vol);

    std::map<int, sf::SoundBuffer> soundBuffers;
    std::vector<sf::Sound> soundPool;
    Recorder* recorder = nullptr;
    std::mt19937 rng;
};