#pragma once
#include <SFML/Graphics.hpp>
#include <CL/cl.h>
#include <vector>
#include "MandelbrotSet.h"

class Application {
    // OpenCL objects
    cl_context context;        // OpenCL context
    cl_program program;        // OpenCL program
    cl_kernel kernel;          // OpenCL kernel for Mandelbrot computation
    cl_platform_id platform;   // OpenCL platform ID
    cl_device_id device;       // OpenCL device ID (GPU or CPU)
    cl_command_queue queue;    // OpenCL command queue
    cl_mem buffer;             // OpenCL buffer for storing iteration counts

    // Fractal and rendering
    MandelbrotSet fractal;     // Mandelbrot fractal data
    sf::RenderWindow window;   // SFML window for display
    sf::Image image;           // Image storing pixel colors
    sf::Texture texture;       // Texture for displaying the image
    sf::Sprite sprite;         // Sprite to render the texture

    // Zoom selection
    sf::Vector2i start, end;           // Start and end points of the selection rectangle
    sf::RectangleShape selectionRect;  // Rectangle shape for zoom selection

    // UI buttons
    sf::RectangleShape changeColorButton, resetZoomingButton;  // Buttons
    sf::Text changeColorButtonText, resetZoomingButtonText;    // Button labels
    sf::Font font;                                            // Font used for UI text

    int color = 1;                  // Current color scheme
    std::vector<cl_uint> pixels;    // GPU-computed iteration counts

    static constexpr double EPS = 1e-6;  // Small epsilon for numerical comparisons
    bool dragging = false;               // Flag for mouse dragging (zoom selection)

    void setupOpenCL();
    void renderFractalGPU();
    void changeColorButtonHandler();
    void handleEvent(sf::Event& event);
    void resetZoomingButtonHandler();
    void handleZoomingIn();
    void handleDragging();
    void ZoomByScrolling(float delta, int mouseX, int mouseY);
    void createButton(int x, int y, sf::RectangleShape& button, sf::Text& text, const std::string& title) const;
    sf::Color getColor(double iter, int maxIter) const;

public:
    Application(int width, int height, int maxIter, unsigned int numOfThreads);
    void run();
};