#pragma once

#include <SFML/Audio.hpp>
#include <vector>
#include <map>
#include <cmath>
#include <iostream>
// Necesitamos forward declaration para no crear dependencia circular
class Recorder; 

class SoundManager {
public:
    SoundManager() {
        // Frecuencias Do Mayor
        float frequencies[] = {
            261.63f, 293.66f, 329.63f, 349.23f, 
            392.00f, 440.00f, 493.88f, 523.25f 
        };

        for (int i = 0; i < 8; ++i) {
            generateTone(i + 1, frequencies[i]);
        }

        for(int i=0; i<32; ++i) soundPool.emplace_back();
    }

    // SETTER PARA EL RECORDER
    void setRecorder(Recorder* rec) {
        recorder = rec;
    }

    void generateTone(int id, float frequency) {
        const unsigned SAMPLE_RATE = 44100;
        const int AMPLITUDE = 30000;

        std::vector<sf::Int16> rawSamples;
        float duration = 0.3f; 
        int numSamples = (int)(SAMPLE_RATE * duration);

        for (int i = 0; i < numSamples; i++) {
            float t = (float)i / SAMPLE_RATE;
            float sample = AMPLITUDE * std::sin(2 * 3.14159f * frequency * t);
            float decay = std::exp(-15.0f * t); 
            rawSamples.push_back((sf::Int16)(sample * decay));
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
            sound->setVolume(80.0f); 
            sound->setPitch(1.0f); 
            float normalizedX = (xPosition / worldWidth) * 2.0f - 1.0f; 
            sound->setPosition(normalizedX * 10.0f, 0.0f, 0.0f); 
            sound->setMinDistance(5.0f);
            sound->setAttenuation(1.0f);
            sound->play();
        }

        // 2. ENVIAR A LA GRABADORA (Si existe)
        // Necesitamos incluir "Recorder.hpp" en el cpp o usar un método abstracto,
        // pero como esto es header-only por ahora, necesitamos resolver la dependencia.
        // Haremos un include en el main o moveremos la implementación a cpp.
        // TRUCO: Usaremos forward declaration y asumiremos que el puntero es válido.
        // Para que esto compile en C++, necesitamos incluir Recorder.hpp DONDE SE USE playSound.
        
        if (recorder) {
            // Accedemos a los samples crudos del buffer
            const sf::SoundBuffer& buf = soundBuffers[id];
            // Llamada al recorder (requiere que Recorder esté definido totalmente)
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

    // Helper para evitar problemas de include circular en el header
    void sendToRecorder(const sf::Int16* samples, std::size_t count, float vol);

    std::map<int, sf::SoundBuffer> soundBuffers;
    std::vector<sf::Sound> soundPool;
    Recorder* recorder = nullptr;
};