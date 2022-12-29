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
        CreateWorkerThread(WorkerThreadCallback<DataType> callback) {
            return std::shared_ptr<WorkerThread<DataType>>(new WorkerThread<DataType>(callback));
        }
        void Start() noexcept {
            if (!m_thread) {
                m_thread = std::make_unique<std::jthread>(std::bind_front(&WorkerThread<DataType>::Run, this));
            }
        }
        void PostWork(std::shared_ptr<DataType> &&data) {
            std::unique_lock<std::mutex> lk(m_mutex);
            m_queue.push(std::move(data));
            m_cv.notify_one();
        }
        void Stop() {
            if (m_thread) {
                if (m_thread->joinable()) {
                    m_thread->request_stop();
                    m_thread->join();
                }
                m_thread.reset();
            }
        }
    private:
        explicit WorkerThread(WorkerThreadCallback<DataType> callback) : m_callback(callback) {}

        void Run(std::stop_token st) noexcept {
            std::stop_callback cb(st, [this]() {
                m_cv.notify_all(); //Wake thread on stop request
            });
            while (true) {
                std::shared_ptr<DataType> msg;
                {
                    std::unique_lock<std::mutex> lock(m_mutex);
                    m_cv.wait(lock, [st, this]() {
                        return st.stop_requested() ||
                            !m_queue.empty();
                    });
                    if (st.stop_requested()) {
                        break;
                    } else if (m_queue.empty()) {
                        continue;
                    }
                    msg = m_queue.front();
                    m_queue.pop();
                }
                m_callback(std::move(msg));
            }
        }

        WorkerThreadCallback<DataType> m_callback;
        std::unique_ptr<std::jthread> m_thread;
        std::queue<std::shared_ptr<DataType>> m_queue;
        std::mutex m_mutex;
        std::condition_variable m_cv;
    };

}

#endif //AugmentedNormalcy_Utility_WorkerThread
