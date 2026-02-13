#include "Recorder.hpp"
#include <iostream>
#include <stdexcept>

Recorder::Recorder(int width, int height, int fps, const std::string& outputFilename) 
    : width(width), height(height), ffmpegPipe(nullptr) 
{
    // Preparamos la textura para capturar
    if (!captureTexture.create(width, height)) {
        throw std::runtime_error("No se pudo crear la textura de captura");
    }

    // Comando FFmpeg:
    // -f rawvideo: Entrada cruda
    // -pix_fmt rgba: SFML entrega RGBA
    // -r: Framerate
    // -i -: Leer de stdin (pipe)
    // -c:v libx264: Codec H.264
    // -pix_fmt yuv420p: Compatible con la mayoría de players/Youtube
    // -y: Sobrescribir archivo
    std::string cmd = "ffmpeg -y "
                      "-f rawvideo -vcodec rawvideo "
                      "-s " + std::to_string(width) + "x" + std::to_string(height) + " "
                      "-pix_fmt rgba "
                      "-r " + std::to_string(fps) + " "
                      "-i - "
                      "-c:v libx264 -preset ultrafast "
                      "-pix_fmt yuv420p "
                      "\"" + outputFilename + "\"";

    // Abrir pipe (Linux/Unix)
    ffmpegPipe = popen(cmd.c_str(), "w");

    if (!ffmpegPipe) {
        throw std::runtime_error("No se pudo iniciar FFmpeg. Está instalado? Pelotudo");
    }

    std::cout << "[REC] Grabando en: " << outputFilename << std::endl;
}

Recorder::~Recorder() {
    if (ffmpegPipe) {
        std::cout << "[REC] Finalizando grabacion..." << std::endl;
        pclose(ffmpegPipe);
        std::cout << "[REC] Video exportado." << std::endl;
    }
}

void Recorder::addFrame(const sf::Window& window) {
    if (!ffmpegPipe) return;

    // Capturar el contenido de la ventana a una textura
    captureTexture.update(window);

    // Copiar a imagen en CPU (costoso, pero necesario para el pipe)
    sf::Image img = captureTexture.copyToImage();

    // Obtener puntero a los pixeles crudos
    const sf::Uint8* pixels = img.getPixelsPtr();

    // Escribir al pipe
    fwrite(pixels, 1, width * height * 4, ffmpegPipe);
}