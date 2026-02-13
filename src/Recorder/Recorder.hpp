#pragma once

#include <string>
#include <cstdio>
#include <SFML/Graphics.hpp>

class Recorder {
public:
    Recorder(int width, int height, int fps, const std::string& outputFilename);
    ~Recorder();

    void addFrame(const sf::Window& window);

private:
    FILE* ffmpegPipe;
    int width;
    int height;
    std::vector<sf::Uint8> buffer;
    sf::Texture captureTexture;
};