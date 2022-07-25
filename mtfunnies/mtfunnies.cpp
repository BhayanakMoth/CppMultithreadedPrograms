
#include <iostream>
#include <thread>
#include "FrameTimer.h"
#include <limits>
#include <random>
#include <ranges>
#include <range/v3/all.hpp>
#include <array>
#include <vector>
#include <mutex>
#include <span>
#include <algorithm>
#include <stack>
#include "Controller.hpp"
#include "Worker.hpp"
constexpr size_t DATASET_SIZE = 5'000'000;
constexpr size_t NUM_DATASETS = 4;
void ProcessDataset(std::span<int> arr, uint64_t & sum) {
    for (int x : arr) {
        constexpr auto limit = std::numeric_limits<int>::max();
        const auto y = (double)x / limit;
        sum += int(std::sin(std::cos(y)) * limit);
    }
}

void MultiThreadedBenchmark(std::vector <std::array<int, DATASET_SIZE>> & datasets) {
    FrameTimer frameTimer;
    std::vector<std::thread> jobs;
    std::mutex mtx;
    struct Value {
        uint64_t v = 0;
        char padding[56];
    };
    Value sum[NUM_DATASETS];
    float time = frameTimer.Mark();
    int i = 0;
    for (auto& set : datasets) {
        jobs.push_back(std::thread(ProcessDataset, std::span{ set }, std::ref(sum[i].v)));
        i++;
    }
    for (auto& job : jobs) {
        job.join();
    }
    time = frameTimer.Mark();
    uint64_t total = 0;
    for (int j = 0; j < NUM_DATASETS; j++) {
        total += sum[j].v;
    }
    std::cout << "Multi Threaded Performance: \n";
    std::cout << "  Sum: " << (total) << "\n";
    std::cout << "  Time taken: " << time << "ms\n";
}

void SingleThreadedBenchmark(std::vector<std::array<int, DATASET_SIZE>> & datasets) {
    FrameTimer frameTimer;
    uint64_t sum = 0;
    frameTimer.Mark();
    for (auto& arr : datasets) {
        for (int x : arr) {
            constexpr auto limit = std::numeric_limits<int>::max();
            const auto y = (double)x / limit;
            sum += int(std::sin(std::cos(y)) * limit);
        }
    }
    float time =  frameTimer.Mark();
    std::cout << "Single Threaded Performance: \n";
    std::cout << "  Sum: " << sum << "\n";
    std::cout << "  Time taken: " << time << "ms\n";
}

int MultiThreadedSmalliesBenchmark(std::vector<std::array<int, DATASET_SIZE>>& datasets) {
    typedef std::chrono::microseconds micro;

    struct Value {
        uint64_t v = 0;
        char padding[56];
    };
    Value sum[NUM_DATASETS];
    FrameTimer frameTimer;
    FrameTimer piecemealTimer;
    std::vector<float> times(0, 10'000);
    float time = frameTimer.Mark();
    constexpr auto subsetSize = DATASET_SIZE / 10'000;
    std::vector<Worker*> workers;
    Controller controller(NUM_DATASETS);
    for (int i = 0; i < NUM_DATASETS; i++) {
        workers.push_back(new Worker(&controller));
    }
    for (size_t i = 0; i < DATASET_SIZE; i += subsetSize) {
        piecemealTimer.Mark();
        {
            for (size_t j = 0; j < NUM_DATASETS; j++){
                workers[j]->SetJob([&datasets, &sum, i, j](){ ProcessDataset(std::span(&datasets[j][i], subsetSize), sum[j].v); });
            }
            controller.WaitForAllDone();
        }
        times.push_back(piecemealTimer.Mark<micro>());
    }
    time = frameTimer.Mark();
    std::sort(std::begin(times), std::end(times));
    uint64_t total = 0;
    for (int j = 0; j < NUM_DATASETS; j++) {
        total += sum[j].v;
    }
    for (Worker* worker : workers) {
        worker->Kill();
    }
    std::cout << "Multi Threaded Smallies Performance: \n";
    std::cout << "  Sum: " << total << "\n";
    std::cout << "  Time taken: " << time << "ms\n";
    std::cout << "  Median Time Taken: " << times[4999] << " micro seconds.\n";
    return 0;
}
void experiment1() {
    std::minstd_rand rng;
    std::vector<std::array<int, DATASET_SIZE>> datasets{ NUM_DATASETS };
    FrameTimer frameTimer;
    for (auto& arr : datasets) {
        std::ranges::generate(arr, rng);
    }

    SingleThreadedBenchmark(datasets);
    MultiThreadedBenchmark(datasets);
    MultiThreadedSmalliesBenchmark(datasets);
}

template<typename T>
class ThreadSafeStack {
public:

    T pop() {
        std::lock_guard lg{ mtx };
        if (!stack.empty()) {
            T elem = stack.top();
            stack.pop();
            return elem;
        }
        return T();
    }

    void push(T elem) {
        std::lock_guard lg{ mtx };
        stack.push(elem);
    }

    bool empty() {
        return stack.empty();
    }
private:
    std::stack<T> stack;
    std::mutex mtx;
};

struct Work {
    int wait = 0;
    int data = 0;
    Work() {
        wait = 0;
        data = 0;
    }
    Work(int wait, int data)
        :
        wait(wait),
        data(data)
    {
    }
};

void ProcessElementST(Work elem, uint64_t& accum) {
    std::this_thread::sleep_for(std::chrono::microseconds(elem.wait));
    accum += elem.data;
}

void ProcessElementMT(Work elem, uint64_t& accum, int id) {
    std::this_thread::sleep_for(std::chrono::microseconds(elem.wait));
    accum += elem.data;
}
int main()
{
    int x = 0;
    while (x < 10) {
        std::stack<Work> stdata;
        ThreadSafeStack<Work> mtdata;
        std::random_device rd;
        std::uniform_int_distribution<> distr(1, 1);
        for (int i = 1; i <= 100; i++)
        {
            Work work(distr(rd), i);
            mtdata.push(work);
            stdata.push(work);
        }
        FrameTimer ft;
        ft.Mark();
        uint64_t accum = 0;
        while (!stdata.empty()) {
            Work elem = stdata.top();
            stdata.pop();
            ProcessElementST(elem, accum);
        }
        float time = ft.Mark();
        std::cout << "The total(ST): " << accum << "\n";
        std::cout << "Time taken(ST): " << time << "ms\n";
        struct {
            uint64_t accum = 0;
            char padding[56];
        } data1, data2;
        std::mutex mtx;
        auto f1 = [&]() {
            while (!mtdata.empty()) {
                Work elem = mtdata.pop();
                ProcessElementMT(elem, data1.accum, 1);
            }
        };
        auto f2 = [&]() {
            while (!mtdata.empty()) {
                Work elem = mtdata.pop();
                ProcessElementMT(elem, data2.accum, 2);
            }
        };
        ft.Mark();
        std::thread worker1(f1);
        std::thread worker2(f2);
        worker1.join();
        worker2.join();
        data1.accum += data2.accum;
        time = ft.Mark();
        std::cout << "The total(MT): " << data1.accum << "\n";

        std::cout << "Time taken(MT): " << time << "ms\n";
        if (data1.accum != accum) {
            std::system("pause");
        }
        std::cout << "End of loop: " << x+1 << "\n";
        x++;
    }
    return 0;
}