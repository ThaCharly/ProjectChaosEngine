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
#include <SFML/Graphics/Image.hpp>
#include <SFML/Audio.hpp> 

class Recorder {
public:
    Recorder(int width, int height, int fps, const std::string& outputFilename);
    ~Recorder();

    void addFrame(const sf::Texture& texture);
    void addAudioEvent(const sf::Int16* samples, std::size_t sampleCount, float volume);
    void stop(); 

    bool isRecording = false; 

private:
    void workerLoop(); // <--- El laburante de fondo

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
    std::queue<sf::Image> frameQueue;
    std::atomic<bool> isWorkerRunning;
};