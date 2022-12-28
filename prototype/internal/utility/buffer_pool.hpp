
#ifndef AugmentedNormalcy_Utility_BufferPool
#define AugmentedNormalcy_Utility_BufferPool

#include <vector>
#include <memory>
#include <mutex>
#include <exception>
#include <iostream>
#include <functional>


namespace utility {

    template <typename BufferType>
    struct BufferNode {
        [[nodiscard]] static std::shared_ptr<BufferNode<BufferType>> New() {
            return std::shared_ptr<BufferNode<BufferType>>(new BufferNode<BufferType>());
        }
        std::shared_ptr<BufferNode> _previous_node;
        std::shared_ptr<BufferType> _buffer;
        std::shared_ptr<BufferNode> _next_node;
    private:
        BufferNode() = default;
    };

    template <typename BufferType>
    class BufferPool {
    public:

        [[nodiscard]] explicit BufferPool(int buffer_count, std::function<std::shared_ptr<BufferType>()> initializer) :
                buffer_store(buffer_count),
                available_buffers(buffer_count)
        {
            std::generate(buffer_store.begin(), buffer_store.end(), initializer);
            // head / tail are dummies, no data
            available_buffer_head = BufferNode<BufferType>::New();
            available_buffer_tail = BufferNode<BufferType>::New();
            // no reference here so we can update the reference without corrupting the ll
            auto node = available_buffer_head;

            // after that, one for each item in store
            for (auto buffer_ptr: buffer_store) {
                auto new_node = BufferNode<BufferType>::New();
                new_node->_buffer = std::move(buffer_ptr);
                node->_next_node = new_node;
                new_node->_previous_node = node;
                node = new_node;
            }
            // and attach the tail
            node->_next_node = available_buffer_tail;
            available_buffer_tail->_previous_node = node;
        }

        [[nodiscard]] std::shared_ptr<BufferType> New() {
            std::scoped_lock lock(buffer_mutex);
            if (available_buffer_head->_next_node == available_buffer_tail) {
                throw std::underflow_error("No available buffers");
            }
            auto ret_node = available_buffer_head->_next_node;
            available_buffer_head->_next_node = ret_node->_next_node;
            ret_node->_next_node->_previous_node = available_buffer_head;
            available_buffers--;
            return std::move(ret_node->_buffer);
        }

        void Free(std::shared_ptr<BufferType> &&buffer_ptr) {
            if (buffer_ptr.use_count() > 2) {
                buffer_ptr.reset();
                // someone is still using the buffer
                return;
            }
            std::scoped_lock lock(buffer_mutex);
            auto node = BufferNode<BufferType>::New();
            node->_buffer = std::move(buffer_ptr);
            node->_previous_node = available_buffer_tail->_previous_node;
            available_buffer_tail->_previous_node->_next_node = node;
            node->_next_node = available_buffer_tail;
            available_buffer_tail->_previous_node = node;
            available_buffers++;
        }

        [[nodiscard]] size_t AvailableBuffers() {
            std::scoped_lock lock(buffer_mutex);
            return available_buffers;
        }
    private:
        // technically these should be unique ptrs, but in the error
        // case that they point to each other, I guess we "handle" you
        std::shared_ptr<BufferNode<BufferType>> available_buffer_head;
        std::shared_ptr<BufferNode<BufferType>> available_buffer_tail;
        std::vector<std::shared_ptr<BufferType>> buffer_store;
        size_t available_buffers;
        std::mutex buffer_mutex;
    };
}



#endif // AugmentedNormalcy_Utility_BufferPool