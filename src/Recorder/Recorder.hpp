#pragma once

#include <string>
#include <cstdio>
#include <vector>
#include <SFML/Graphics.hpp>
#include <SFML/Audio.hpp> // Necesario para tipos de audio

class Recorder {
public:
    Recorder(int width, int height, int fps, const std::string& outputFilename);
    ~Recorder();

    void addFrame(const sf::Window& window);
    
    // MÉTODO NUEVO: Recibe un sonido y lo mezcla en la pista de audio
    void addAudioEvent(const sf::Int16* samples, std::size_t sampleCount, float volume);

    bool isRecording = false; 

private:
    FILE* ffmpegPipe = nullptr;
    int width;
    int height;
    int fps;
    std::string finalFilename;      // Nombre final (video.mp4)
    std::string tempVideoFilename;  // Video mudo temporal (temp_video.mp4)
    std::string tempAudioFilename;  // Audio temporal (temp_audio.wav)

    sf::Texture captureTexture;
    
    // MIXER DE AUDIO INTERNO
    std::vector<float> audioMixBuffer; // Usamos float para evitar saturación al sumar
    unsigned int sampleRate = 44100;
    long long currentFrame = 0; // Para saber en qué milisegundo estamos
};