#pragma once

#include <string>
#include <cstdio>
#include <vector>
#include <SFML/Graphics.hpp>
#include <SFML/Audio.hpp> 

class Recorder {
public:
    Recorder(int width, int height, int fps, const std::string& outputFilename);
    ~Recorder();

    // Borrá la de sf::Window y poné esta:
void addFrame(const sf::Texture& texture);
    void addAudioEvent(const sf::Int16* samples, std::size_t sampleCount, float volume);
    
    // MÉTODO NUEVO: Cierra todo, guarda y fusiona.
    void stop(); 

    bool isRecording = false; 

private:
    FILE* ffmpegPipe = nullptr;
    int width;
    int height;
    int fps;
    std::string finalFilename;      
    std::string tempVideoFilename;  
    std::string tempAudioFilename;  

    sf::Texture captureTexture;
    
    std::vector<float> audioMixBuffer; 
    unsigned int sampleRate = 44100;
    long long currentFrame = 0; 
    
    bool isFinished = false; // Para saber si ya cerramos
};