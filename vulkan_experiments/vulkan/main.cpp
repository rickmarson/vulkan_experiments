#include "vulkan.hpp"

int main(int argc, char** argv) {
	VulkanTutorial app;
	if (!app.setup()) {
		return -1;
	}
	app.run();
	return 0;
}
