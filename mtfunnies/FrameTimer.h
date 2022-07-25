#pragma once
#include <chrono>
using namespace std::chrono;

class FrameTimer
{
public:
	FrameTimer();
	float Mark();
	template<typename Unit>
	float Mark() {
		typedef std::chrono::high_resolution_clock Time;
		const auto old = last;
		last = steady_clock::now();
		const duration<float> frameTime = last - old;
		return std::chrono::duration_cast<Unit>(frameTime).count();
	}
private:
	std::chrono::steady_clock::time_point last;
};