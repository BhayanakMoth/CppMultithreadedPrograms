#pragma once
#include <mutex>
#include <condition_variable>

class Controller {
private:
    std::mutex mtx;
    int numberOfWorkers = 0;
    int doneWorkers = 0;
    std::condition_variable cv;
    std::unique_lock<std::mutex> lk;
public:
    Controller(int numberOfWorkers)
        :
        numberOfWorkers(numberOfWorkers),
        lk(mtx)
    {
    }

    void SignalDone() {
        {
            std::lock_guard lg{ mtx };
            doneWorkers++;
        }
        if (numberOfWorkers == doneWorkers) {
            cv.notify_one();
        }
    }
    void WaitForAllDone() {
        cv.wait(lk, [this]() {return numberOfWorkers == doneWorkers; });
        doneWorkers = 0;
    }
};