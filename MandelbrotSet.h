#pragma once
#include <fstream>
#include <iostream>
#include <mutex>
#include <vector>

class MandelbrotSet {
public:
    std::vector<int> image;  // Stores the number of iterations for each pixel

    int WIDTH;   // Width of the fractal image in pixels
    int HEIGHT;  // Height of the fractal image in pixels

    double REAL_MIN;  // Minimum real value of the complex plane
    double REAL_MAX;  // Maximum real value of the complex plane
    double IMAG_MIN;  // Minimum imaginary value of the complex plane
    double IMAG_MAX;  // Maximum imaginary value of the complex plane

    int MAX_ITER;          // Maximum number of iterations per pixel
    unsigned int NUM_THREADS;  // Number of threads to use for computation

    MandelbrotSet(int width, int height, int maxIter, unsigned int numOfThreads)
        : WIDTH(width), HEIGHT(height), MAX_ITER(maxIter), NUM_THREADS(numOfThreads) {
        setDefaultRegion();
    }

    void setDefaultRegion() {
        REAL_MIN = -2.0;
        REAL_MAX = 1.0;
        IMAG_MIN = -1.5;
        IMAG_MAX = 1.5;
    }

    void updateRegion(double realMin, double realMax, double imagMin, double imagMax) {
        REAL_MIN = realMin;
        REAL_MAX = realMax;
        IMAG_MIN = imagMin;
        IMAG_MAX = imagMax;
    }

	/**
	 * @brief Computes the Mandelbrot set using OpenCL and returns the iteration counts.
	 *
	 * Sets up an OpenCL platform, selects a GPU device, creates a context and command queue,
	 * compiles the "mandelbrot.cl" kernel, allocates a buffer for storing iteration counts,
	 * executes the kernel over a 2D grid, and reads the results back into a CPU vector.
	 *
	 * @return std::vector<int>  A vector of iteration counts for each pixel in the image.
	 */
    std::vector<int> computeFractalGPU() const {
        std::vector<int> image(WIDTH * HEIGHT);

        // OpenCL setup
        cl_platform_id platform;
        cl_device_id device;
        cl_int err = clGetPlatformIDs(
            1, 
            &platform, 
            nullptr
        );
        err = clGetDeviceIDs(
            platform, 
            CL_DEVICE_TYPE_GPU, 
            1, 
            &device, 
            nullptr
        );
        cl_context context = clCreateContext(
            nullptr, 
            1, 
            &device, 
            nullptr, 
            nullptr, 
            &err
        );
        cl_command_queue queue = clCreateCommandQueueWithProperties(
            context, 
            device, 
            nullptr, 
            &err
        );

        std::ifstream file("mandelbrot.cl");
        std::string source((std::istreambuf_iterator<char>(file)),
            std::istreambuf_iterator<char>());
        const char* kernelSource = source.c_str();

        cl_program program = clCreateProgramWithSource(
            context, 
            1, 
            &kernelSource, 
            nullptr, 
            &err
        );
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

        cl_kernel kernel = clCreateKernel(
            program, 
            "mandelbrot", 
            &err
        );
        cl_mem buffer = clCreateBuffer(
            context, 
            CL_MEM_WRITE_ONLY, 
            sizeof(int) * WIDTH * HEIGHT, 
            nullptr, 
            &err
        );

        err = clSetKernelArg(kernel, 0, sizeof(cl_mem), &buffer);
        err |= clSetKernelArg(kernel, 1, sizeof(int), &WIDTH);
        err |= clSetKernelArg(kernel, 2, sizeof(int), &HEIGHT);
        err |= clSetKernelArg(kernel, 3, sizeof(int), &MAX_ITER);
        err |= clSetKernelArg(kernel, 4, sizeof(double), &REAL_MIN);
        err |= clSetKernelArg(kernel, 5, sizeof(double), &REAL_MAX);
        err |= clSetKernelArg(kernel, 6, sizeof(double), &IMAG_MIN);
        err |= clSetKernelArg(kernel, 7, sizeof(double), &IMAG_MAX);

        size_t global[2] = { static_cast<size_t>(WIDTH), static_cast<size_t>(HEIGHT) };
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
        clFinish(queue);

        err = clEnqueueReadBuffer(
            queue, 
            buffer, 
            CL_TRUE, 
            0, 
            sizeof(int) * WIDTH * HEIGHT, 
            image.data(), 
            0, 
            nullptr, 
            nullptr
        );

        clReleaseMemObject(buffer);
        clReleaseKernel(kernel);
        clReleaseProgram(program);
        clReleaseCommandQueue(queue);
        clReleaseContext(context);

        return image;
    }
};