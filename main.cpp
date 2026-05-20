#include "Application.h"
#include <thread>

int main() {
	int width = 800;
	int height = 600;
	int maxIter = 3000;
	const unsigned int numThreads = std::thread::hardware_concurrency();

	try
	{
		Application application(width, height, maxIter, numThreads);
		application.run();
	}
	catch (std::exception& e)
	{
		std::cerr << e.what() << "\n";
		return 1;
	}

	return 0;
}