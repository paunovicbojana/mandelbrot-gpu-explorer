#include <SFML/Graphics.hpp>
#include <CL/cl.h>
#include <fstream>
#include <iostream>
#include <vector>
#define _USE_MATH_DEFINES
#include "MandelbrotSet.h"
#include "Application.h"

/**
 * @brief Constructor that sets up the Mandelbrot application.
 *
 * Initializes the fractal generator, creates the SFML window,
 * prepares the image buffer and zoom selection rectangle,
 * loads the UI font, creates color-change and reset buttons,
 * sets up OpenCL for GPU rendering, and renders the initial fractal.
 *
 * @param width         Width of the window and fractal image in pixels.
 * @param height        Height of the window and fractal image in pixels.
 * @param maxIter       Maximum iteration count for Mandelbrot calculations.
 * @param numOfThreads  Number of OpenCL threads used for GPU rendering.
 */
Application::Application(int width, int height, int maxIter, unsigned int numOfThreads) :
	fractal(MandelbrotSet(width, height, maxIter, numOfThreads)),
    window(
        sf::VideoMode(width, height), 
        "Mandelbrot Set", 
        sf::Style::Titlebar | sf::Style::Close
    )
{
    // Create an empty image that will store fractal pixels
	image.create(
        fractal.WIDTH, 
        fractal.HEIGHT, 
        sf::Color::Black
    );

    // Configure the rectangle used for zoom selection
    selectionRect.setFillColor(sf::Color(255, 255, 255, 80));
    selectionRect.setOutlineThickness(2.f);
    selectionRect.setOutlineColor(sf::Color::White);

    if (!font.loadFromFile("TimesNewRoman.ttf")) {
        std::cerr << "Please provide TimesNewRoman.ttf in exe folder!\n";
    }

    createButton(
        width - 50, 
        5, 
        changeColorButton, 
        changeColorButtonText, 
        "Color"
    );
    createButton(
        width - 50, 
        50, 
        resetZoomingButton, 
        resetZoomingButtonText, 
        "Reset"
    );

    setupOpenCL();

    renderFractalGPU();

    // Load computed image into a texture and set to sprite
    texture.loadFromImage(image);
    sprite.setTexture(texture, true);
}

void Application::run() {
	while (window.isOpen()) {
	    sf::Event event;
	    while (window.pollEvent(event)) {
	        if (event.type == sf::Event::Closed) {
	            window.close();
	            return;
	        }
	        handleEvent(event);
	    }

	    window.clear(sf::Color::Black);
	    window.draw(sprite);
	    if (dragging) 
            window.draw(selectionRect);
	    window.draw(changeColorButton);
	    window.draw(changeColorButtonText);
	    window.draw(resetZoomingButton);
	    window.draw(resetZoomingButtonText);
	    window.display();
	}
}

/**
 * @brief Initializes OpenCL resources for Mandelbrot computation.
 *
 * Discovers available OpenCL platforms and selects a GPU device if present,
 * otherwise falls back to a CPU device. Creates the OpenCL context, command
 * queue, program, and kernel from the "mandelbrot.cl" source file, and
 * allocates a device buffer for storing iteration counts.
 *
 * @throws std::runtime_error if no suitable OpenCL platform/device is found,
 *         or if context, queue, program, kernel, or buffer creation fails.
 */
void Application::setupOpenCL() {
    // Discover OpenCL platforms and create context, queue, program, kernel
    cl_int err;

    // Get number of OpenCL platforms
    cl_uint numPlatforms;
    err = clGetPlatformIDs(
        0, 
        nullptr, 
        &numPlatforms
    );
    if (err != CL_SUCCESS || numPlatforms == 0)
        throw std::runtime_error("No OpenCL platforms found!\n");

    // Get platform IDs
    std::vector<cl_platform_id> platforms(numPlatforms);
    clGetPlatformIDs(
        numPlatforms, 
        platforms.data(), 
        nullptr
    );

    bool deviceFound = false;

    // Try to find a GPU device
    for (auto p : platforms) {
        cl_uint numDevices = 0;
        err = clGetDeviceIDs(
            p, 
            CL_DEVICE_TYPE_GPU, 
            0, 
            nullptr, 
            &numDevices
        );
        if (numDevices > 0) {
            std::vector<cl_device_id> devices(numDevices);
            clGetDeviceIDs(
                p, 
                CL_DEVICE_TYPE_GPU, 
                numDevices, 
                devices.data(), 
                nullptr
            );
            device = devices[0];
            platform = p;
            deviceFound = true;
            std::cout << "GPU found!\n";
            break;
        }
    }

    // Fallback to CPU if no GPU
    if (!deviceFound) {
        std::cout << "No GPU, using CPU...\n";
        for (auto p : platforms) {
            cl_uint numDevices = 0;
            err = clGetDeviceIDs(
                p, 
                CL_DEVICE_TYPE_CPU, 
                0, 
                nullptr, 
                &numDevices
            );
            if (numDevices > 0) {
                std::vector<cl_device_id> devices(numDevices);
                clGetDeviceIDs(
                    p, 
                    CL_DEVICE_TYPE_CPU, 
                    numDevices, 
                    devices.data(), 
                    nullptr
                );
                device = devices[0];
                platform = p;
                deviceFound = true;
                break;
            }
        }
    }

    if (!deviceFound)
        throw std::runtime_error("No OpenCL devices found!\n");

    // Create an OpenCL context linked to the selected platform
    cl_context_properties props[] = {
    	CL_CONTEXT_PLATFORM,
        reinterpret_cast<cl_context_properties>(platform),
    	0
    };
    context = clCreateContext(
        props, 
        1, 
        &device, 
        nullptr, 
        nullptr, 
        &err
    );
    if (err != CL_SUCCESS) 
        throw std::runtime_error("Failed to create context\n");

    // Create a command queue to issue work to the device
    queue = clCreateCommandQueueWithProperties(
        context, 
        device, 
        nullptr, 
        &err
    );
    if (err != CL_SUCCESS)
		throw std::runtime_error("Failed to create command queue\n");

    // Read kernel source code from file
    std::ifstream file("mandelbrot.cl");
    std::string src(
        (std::istreambuf_iterator<char>(file)),
        (std::istreambuf_iterator<char>())
    );
    const char* srcStr = src.c_str();

    // Create and build OpenCL program
    program = clCreateProgramWithSource(
        context, 
        1, 
        &srcStr, 
        nullptr, 
        &err
    );
    if (err != CL_SUCCESS)
        throw std::runtime_error("Failed to create program\n");

    err = clBuildProgram(
        program, 
        1, 
        &device, 
        nullptr, 
        nullptr, 
        nullptr
    );
    if (err != CL_SUCCESS)
		throw std::runtime_error("Failed to build program\n");

    // Create kernel function object
    kernel = clCreateKernel(
        program, 
        "mandelbrot_kernel", 
        &err
    );
    if (err != CL_SUCCESS)
        throw std::runtime_error("Failed to create kernel\n");

    // Create buffer to hold computed iteration counts
    buffer = clCreateBuffer(
        context, 
        CL_MEM_WRITE_ONLY,
        sizeof(cl_uint) * fractal.WIDTH * fractal.HEIGHT,
        nullptr, 
        &err
    );
    if (err != CL_SUCCESS)
        throw std::runtime_error("Failed to create buffer\n");
}

/**
 * @brief Renders the Mandelbrot set using the GPU via OpenCL.
 *
 * Sets kernel arguments (image size, complex-plane bounds, max iterations),
 * enqueues a 2-D NDRange kernel to compute iteration counts for each pixel,
 * reads the results back to CPU memory, and maps iteration counts to colors
 * to update the SFML image, texture, and sprite.
 *
 * @throws std::runtime_error if setting kernel arguments, running the kernel,
 *         or reading the buffer fails.
 */
void Application::renderFractalGPU() {
    auto start = std::chrono::high_resolution_clock::now();
    pixels.resize( static_cast<size_t>(fractal.WIDTH) * fractal.HEIGHT);

    double realMin = fractal.REAL_MIN;
    double realMax = fractal.REAL_MAX;
    double imagMin = fractal.IMAG_MIN;
    double imagMax = fractal.IMAG_MAX;


    cl_int err = 0;
    err = clSetKernelArg(kernel, 0, sizeof(cl_mem), &buffer);
    err |= clSetKernelArg(kernel, 1, sizeof(int), &fractal.WIDTH);
    err |= clSetKernelArg(kernel, 2, sizeof(int), &fractal.HEIGHT);
    err |= clSetKernelArg(kernel, 3, sizeof(double), &realMin);
    err |= clSetKernelArg(kernel, 4, sizeof(double), &realMax);
    err |= clSetKernelArg(kernel, 5, sizeof(double), &imagMin);
    err |= clSetKernelArg(kernel, 6, sizeof(double), &imagMax);
    err |= clSetKernelArg(kernel, 7, sizeof(int), &fractal.MAX_ITER);

    if (err != CL_SUCCESS)
        throw std::runtime_error("Error setting kernel args\n");

    size_t global[2] = { static_cast<size_t>(fractal.WIDTH),
                         static_cast<size_t>(fractal.HEIGHT) };

    // Execute kernel over a 2D global range
    err = clEnqueueNDRangeKernel(
        queue, 
        kernel, 
        2, 
        nullptr, 
        global, 
        nullptr, 
        0, 
        nullptr, 
        nullptr
    );
    if (err != CL_SUCCESS)
        throw std::runtime_error("Error enqueueing kernel\n");

    clFinish(queue); // Wait for GPU to finish

    // Read computed iteration counts back to CPU memory
    err = clEnqueueReadBuffer(
        queue, 
        buffer, 
        CL_TRUE, 
        0,
        sizeof(cl_uint) * pixels.size(),
        pixels.data(), 
        0, 
        nullptr, 
        nullptr
    );
    if (err != CL_SUCCESS) 
        throw std::runtime_error("Error reading buffer\n");

    // Convert iteration counts to colors and update the image
    for (int y = 0; y < fractal.HEIGHT; ++y) {
        for (int x = 0; x < fractal.WIDTH; ++x) {
            int iter = static_cast<int>(pixels[y * fractal.WIDTH + x]);
            image.setPixel(
                x, 
                y, 
                getColor(iter, fractal.MAX_ITER)
            );
        }
    }

    // Update the texture used for drawing
    texture.loadFromImage(image);
    sprite.setTexture(texture, true);

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();

    std::cout << "Time taken: " << duration << " ms\n";
}

void Application::changeColorButtonHandler() {
    color = (color + 1) % 5;
    for (int y = 0; y < fractal.HEIGHT; ++y) {
        for (int x = 0; x < fractal.WIDTH; ++x) {
            int iter = static_cast<int>(pixels[y * fractal.WIDTH + x]);
            image.setPixel(
                x, 
                y, 
                getColor(iter, fractal.MAX_ITER)
            );
        }
    }
    texture.loadFromImage(image);
    sprite.setTexture(texture, true);
}

/**
 * @brief Maps Mandelbrot iteration counts to an RGB color.
 *
 * Converts the ratio of iterations to maxIter into a color gradient,
 * using the current color scheme selected by the `color` member.
 * Points inside the set (iter >= maxIter) are rendered black.
 *
 * @param iter    Iteration count for the pixel.
 * @param maxIter Maximum iterations used in Mandelbrot computation.
 * @return sf::Color  The computed RGB color for the pixel.
 */
sf::Color Application::getColor(double iter, int maxIter) const {
    if (iter >= maxIter) 
        return sf::Color::Black;

    double t = iter / maxIter;

    switch (color) {
    case 1:
        return sf::Color(
            (sf::Uint8)(16 * (1 - t) * t * t * t * 255),
            (sf::Uint8)(8 * (1 - t) * (1 - t) * t * t * 255),
            (sf::Uint8)(3 * (1 - t) * (1 - t) * (1 - t) * t * 255)
        );

    case 2:
        return sf::Color(
            (sf::Uint8)(12 * (1 - t) * (1 - t) * t * t * 255),
            (sf::Uint8)(9 * (1 - t) * t * t * t * 255),
            (sf::Uint8)(14 * (1 - t) * (1 - t) * (1 - t) * t * 255)
        );

    case 3:
        return sf::Color(
            (sf::Uint8)(4 * (1 - t) * t * t * t * 255),
            (sf::Uint8)(14 * (1 - t) * (1 - t) * t * t * 255),
            (sf::Uint8)(18 * (1 - t) * (1 - t) * (1 - t) * t * 255)
        );

    case 4:
        return sf::Color(
            (sf::Uint8)(10 * std::sin(3.0 * M_PI * t) * t * 255),
            (sf::Uint8)(10 * std::sin(3.0 * M_PI * (t + 0.33)) * t * 255),
            (sf::Uint8)(10 * std::sin(3.0 * M_PI * (t + 0.66)) * t * 255)
        );

    default:
        return sf::Color(
            (sf::Uint8)(9 * (1 - t) * t * t * t * 255),
            (sf::Uint8)(15 * (1 - t) * (1 - t) * t * t * 255),
            (sf::Uint8)(8.5 * (1 - t) * (1 - t) * (1 - t) * t * 255)
        );
    }
}


void Application::handleEvent(sf::Event& event) {
    if (event.type == sf::Event::MouseButtonPressed &&
        event.mouseButton.button == sf::Mouse::Left) {

        sf::Vector2i mousePos(event.mouseButton.x, event.mouseButton.y);

        if (changeColorButton.getGlobalBounds().contains(static_cast<float>(mousePos.x), static_cast<float>(mousePos.y))) {
            changeColorButtonHandler();
            return;
        }

        if (resetZoomingButton.getGlobalBounds().contains(static_cast<float>(mousePos.x), static_cast<float>(mousePos.y))) {
            resetZoomingButtonHandler();
            return;
        }

        dragging = true;
        start = mousePos;
    }

    if (event.type == sf::Event::MouseButtonReleased &&
        event.mouseButton.button == sf::Mouse::Left && dragging) {

        dragging = false;
        handleZoomingIn();
    }

    if (event.type == sf::Event::MouseMoved && dragging) {
        handleDragging();
    }

    if (event.type == sf::Event::MouseWheelScrolled) {
        auto mws = event.mouseWheelScroll;
        ZoomByScrolling(mws.delta, mws.x, mws.y);
    }
}

void Application::resetZoomingButtonHandler() {
    fractal.setDefaultRegion();
    renderFractalGPU();
}

void Application::handleZoomingIn() {
    dragging = false;
    end = sf::Mouse::getPosition(window);

    if (std::abs(start.x - end.x) < EPS &&
        std::abs(start.y - end.y) < EPS) return;

    double aspect = static_cast<double>(fractal.WIDTH) / fractal.HEIGHT;

    int x1 = std::min(start.x, end.x);
    int x2 = std::max(start.x, end.x);
    int w = x2 - x1;
    int h = static_cast<int>(std::round(w / aspect));

    int y1 = std::min(start.y, end.y);
    int y2 = y1 + h;
    if (y2 > fractal.HEIGHT) {
	    y2 = fractal.HEIGHT;
    	y1 = y2 - h;
    }

    double realMin =
        fractal.REAL_MIN + (fractal.REAL_MAX - fractal.REAL_MIN) * (double)x1 / fractal.WIDTH;
    double realMax =
        fractal.REAL_MIN + (fractal.REAL_MAX - fractal.REAL_MIN) * (double)(x1 + w) / fractal.WIDTH;
    double imagMin =
        fractal.IMAG_MIN + (fractal.IMAG_MAX - fractal.IMAG_MIN) * (double)y1 / fractal.HEIGHT;
    double imagMax =
        fractal.IMAG_MIN + (fractal.IMAG_MAX - fractal.IMAG_MIN) * (double)(y1 + h) / fractal.HEIGHT;

    selectionRect.setSize(sf::Vector2f(0, 0));
    fractal.updateRegion(realMin, realMax, imagMin, imagMax);
    renderFractalGPU();
}

void Application::handleDragging() {
    end = sf::Mouse::getPosition(window);
    double newWidth = std::abs(end.x - start.x);
    double aspect = static_cast<double>(fractal.WIDTH) / fractal.HEIGHT;
    double newHeight = newWidth / aspect;

    int left = std::min(start.x, end.x);
    int top = std::min(start.y, end.y);

    selectionRect.setPosition(static_cast<float>(left), static_cast<float>(top));
    selectionRect.setSize(sf::Vector2f(static_cast<float>(newWidth), static_cast<float>(newHeight)));
}

void Application::ZoomByScrolling(float delta, int mouseX, int mouseY) {
    double zoomFactor = (delta > 0) ? 0.9 : 1.1;

    double mouseRe =
        fractal.REAL_MIN +
        (fractal.REAL_MAX - fractal.REAL_MIN) * mouseX / fractal.WIDTH;
    double mouseIm =
        fractal.IMAG_MIN +
        (fractal.IMAG_MAX - fractal.IMAG_MIN) * mouseY / fractal.HEIGHT;

    double newWidth = (fractal.REAL_MAX - fractal.REAL_MIN) * zoomFactor;
    double newHeight = (fractal.IMAG_MAX - fractal.IMAG_MIN) * zoomFactor;

    double newRealMin = mouseRe - (mouseRe - fractal.REAL_MIN) * zoomFactor;
    double newRealMax = newRealMin + newWidth;
    double newImagMin = mouseIm - (mouseIm - fractal.IMAG_MIN) * zoomFactor;
    double newImagMax = newImagMin + newHeight;

    fractal.updateRegion(newRealMin, newRealMax, newImagMin, newImagMax);
    renderFractalGPU();
}

void Application::createButton(int x, int y, sf::RectangleShape& button, sf::Text& text, const std::string& title) const {
    button = sf::RectangleShape(sf::Vector2f(125.f, 30.f));
    button.setFillColor(sf::Color::Black);
    button.setPosition(static_cast<float>(x), static_cast<float>(y));
    button.setOutlineThickness(2.f);
    button.setOutlineColor(sf::Color::White);

    text.setFont(font);
    text.setString(title);
    text.setCharacterSize(15);
    text.setFillColor(sf::Color::White);
    text.setPosition(
        button.getPosition().x + 10.f, button.getPosition().y + 5.f
    );
}