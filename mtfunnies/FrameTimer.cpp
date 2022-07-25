#include "FrameTimer.h"

using namespace std::chrono;

FrameTimer::FrameTimer()
{
	last = steady_clock::now();
}

float FrameTimer::Mark()
{
	typedef std::chrono::high_resolution_clock Time;
	typedef std::chrono::milliseconds ms;
	const auto old = last;
	last = steady_clock::now();
	const duration<float> frameTime = last - old;
	return std::chrono::duration_cast<ms>(frameTime).count();
}