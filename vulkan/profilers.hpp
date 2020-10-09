#pragma once

#include <chrono>
#include <string>
#include <iostream>

class OneShotProfiler {
public:
	OneShotProfiler(const std::string& name) {
		name_ = name;
		start_ = std::chrono::high_resolution_clock::now();
	}

	~OneShotProfiler() {
		auto stop = std::chrono::high_resolution_clock::now();
		auto elapsed_us = std::chrono::duration_cast<std::chrono::microseconds>(stop - start_).count();
		std::cout << "[" << name_ << "] Elapsed Time: " << elapsed_us / 1000.0f << " ms" << std::endl;
	}

private:
	std::string name_;
	std::chrono::time_point<std::chrono::high_resolution_clock> start_;
};
