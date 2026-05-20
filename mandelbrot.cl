#pragma OPENCL EXTENSION cl_khr_fp64 : enable

__kernel void mandelbrot_kernel(__global unsigned int* image,
                                const int width,
                                const int height,
                                const double realMin,
                                const double realMax,
                                const double imagMin,
                                const double imagMax,
                                const int maxIter)
{
    int x = get_global_id(0);
    int y = get_global_id(1);
    if (x >= width || y >= height) return;

    double real = realMin + (realMax - realMin) * x / (double)width;
    double imag = imagMin + (imagMax - imagMin) * y / (double)height;

    double zr = 0.0;
    double zi = 0.0;
    int iter = 0;

    while(zr*zr + zi*zi < 4.0 && iter < maxIter) {
        double temp = zr*zr - zi*zi + real;
        zi = 2.0*zr*zi + imag;
        zr = temp;
        iter++;
    }

    image[y * width + x] = iter;
}