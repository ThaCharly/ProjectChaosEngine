#pragma once

#include <string>
#include <cstdio>
#include <vector>
#include <queue>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <SFML/Graphics.hpp>
#include <SFML/OpenGL.hpp> // <--- Magia de OpenGL
#include <SFML/Audio.hpp> 
#include <cstdint>
#include <cstring>

class Recorder {
public:
    Recorder(int width, int height, int fps, const std::string& outputFilename);
    ~Recorder();

    void addFrame(const sf::Texture& texture);
    void addAudioEvent(const std::int16_t* samples, std::size_t sampleCount, float volume);
    void stop(); 

    bool isRecording = false; 

private:
    void workerLoop(); 

    FILE* ffmpegPipe = nullptr;
    int width;
    int height;
    int fps;
    std::string finalFilename;      
    std::string tempVideoFilename;  
    std::string tempAudioFilename;  

    std::vector<float> audioMixBuffer; 
    unsigned int sampleRate = 44100;
    long long currentFrame = 0; 
    
    bool isFinished = false; 

    // --- MULTITHREADING ---
    std::thread workerThread;
    std::mutex queueMutex;
    std::condition_variable queueCV;
    std::condition_variable queueSpaceCV;
    std::queue<std::vector<std::uint8_t>> frameQueue;
    std::queue<std::vector<std::uint8_t>> freeQueue; // <--- POOL DE MEMORIA
    std::atomic<bool> isWorkerRunning;

    const size_t MAX_QUEUE_SIZE = 180; // Bajamos el max porque vamos a pre-alocar la RAM (240 = ~4.4GB fijos)
    int totalAllocatedBuffers = 0;

    // --- PBOs (Pixel Buffer Objects) ---
    GLuint pbo[3]; // <--- TRIPLE BUFFERING
    int pboIndex = 0;
    int nextPboIndex = 1;
    bool firstFrame = true;
};