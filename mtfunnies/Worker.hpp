#pragma once
#include "Controller.hpp"
#include <utility>
#include <thread>
#include <functional>
class Worker {
private:
    std::mutex mtx;
    std::condition_variable cv;
    std::jthread thread;
    Controller* controller;
    std::function<void()> job;
    bool pending = false;
    bool dying = false;
public:
    Worker(Controller* controller)
        :
        controller(controller),
        thread(&Worker::_Run, this)
    {
    }

    void SetJob(std::function<void()> job) {
        {
            std::lock_guard lg{ mtx };
            this->job = job;
            pending = true;
        }
        cv.notify_one();
    }

    void Kill() {
        {
            std::lock_guard lg{ mtx };
            dying = true;
        }
        cv.notify_one();
    }

private:
    void _Run() {
        std::unique_lock lk{ mtx };
        while (true) {
            cv.wait(lk, [&]() { return pending || dying; });
            if (dying) break;
            else if (pending) {
                job();
                pending = false;
            }
            controller->SignalDone();
        }
    }
};
