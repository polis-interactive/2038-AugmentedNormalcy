//
// Created by bruce on 12/28/2022.
//

#ifndef AugmentedNormalcy_Utility_WorkerThread
#define AugmentedNormalcy_Utility_WorkerThread

#include <thread>
#include <memory>
#include <condition_variable>
#include <queue>
#include <stop_token>
#include <functional>
#include <mutex>

#include <iostream>

namespace utility {

    /*
     * There is currently no backpressure here... could be a problem
     *
     * Also, we could tentatively use a stop_source service to stop all types of workthreads at the
     * same time? Maybe pass a stop source into the thread?
     */


    template <typename DataType>
    using WorkerThreadCallback = std::function<void(std::shared_ptr<DataType> &&)>;

    template <typename DataType>
    class WorkerThread {
    public:

        [[nodiscard]] static std::shared_ptr<WorkerThread<DataType>>
        CreateWorkerThread(WorkerThreadCallback<DataType> callback, std::function<void()> startup = nullptr) {
            return std::shared_ptr<WorkerThread<DataType>>(new WorkerThread<DataType>(std::move(callback), std::move(startup)));
        }
        void Start() noexcept {
            {
                std::unique_lock<std::mutex> lock(_mutex);
                _queue = {};
            }
            if (!_thread) {
                _thread = std::make_unique<std::jthread>(std::bind_front(&WorkerThread<DataType>::Run, this));
            }
        }
        void PostWork(std::shared_ptr<DataType> &&data) {
            std::unique_lock<std::mutex> lock(_mutex);
            _queue.push(std::move(data));
            _cv.notify_one();
        }
        void Stop() {
            if (_thread) {
                if (_thread->joinable()) {
                    _thread->request_stop();
                    _thread->join();
                }
                _thread.reset();
            }
            std::unique_lock<std::mutex> lock(_mutex);
            _queue = {};
        }
    private:
        explicit WorkerThread(WorkerThreadCallback<DataType> &&callback, std::function<void()> &&startup) :
            _callback(std::move(callback)),
            _startup(std::move(startup))
        {}

        void Run(std::stop_token st) noexcept {
            std::stop_callback cb(st, [this]() {
                _cv.notify_all(); //Wake thread on stop request
            });
            if (_startup) {
                _startup();
            }
            while (true) {
                std::shared_ptr<DataType> msg;
                {
                    std::unique_lock<std::mutex> lock(_mutex);
                    _cv.wait(lock, [st, this]() {
                        return st.stop_requested() ||
                            !_queue.empty();
                    });
                    if (st.stop_requested()) {
                        break;
                    } else if (_queue.empty()) {
                        continue;
                    }
                    msg = _queue.front();
                    _queue.pop();
                }
                _callback(std::move(msg));
            }
        }

        WorkerThreadCallback<DataType> _callback;
        std::function<void()> _startup;
        std::unique_ptr<std::jthread> _thread;
        std::queue<std::shared_ptr<DataType>> _queue;
        std::mutex _mutex;
        std::condition_variable _cv;
    };

}

#endif //AugmentedNormalcy_Utility_WorkerThread
