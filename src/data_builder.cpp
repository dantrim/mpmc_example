#include "data_builder.h"

//std/stl
#include <sstream>
#include <iostream>
#include <string>
using namespace std;

//logging
#include "spdlog/spdlog.h"

DataBuilder::DataBuilder(moodycamel::ConcurrentQueue<DataFragment>* input_queue,
        std::map<unsigned int, moodycamel::ConcurrentQueue<DataFragment>> * output_queue,
        std::shared_ptr<std::condition_variable> map_condition,
        std::shared_ptr<std::mutex> map_mutex,
        std::shared_ptr<boost::asio::io_service> io_service,
        std::atomic_bool & build_flag) :
            consumer_token(nullptr),
            producer_token(nullptr),
            m_in_queue(input_queue)
{

    logger = spdlog::get("mm_ddaq");
    m_io_service = io_service;

    m_out_queue = output_queue;
    m_map_cond = map_condition;
    m_map_mutex = map_mutex;

    m_build_flag = &build_flag;

}

void DataBuilder::start() {
    m_thread = std::thread( [this]() {
        consumer_token = new moodycamel::ConsumerToken(*m_in_queue);
        m_io_service->run();
    });
}

bool DataBuilder::continue_building() {
    return m_build_flag->load(std::memory_order_acquire);
}

void DataBuilder::build()
{
    logger->info("DataBuilder::build building starts");

    m_map_cond->notify_all();

    uint8_t* data_in = nullptr;

    unsigned int n_bulk = 5;
    DataFragment fragments [ n_bulk ];

    uint32_t current_l1id = 0xffffffff;
    bool is_start = true;

    while(continue_building()) {

        if(m_io_service->stopped()) break;

        std::lock_guard<std::mutex> lock(*m_map_mutex);
        size_t n_read = m_in_queue->try_dequeue_bulk(*consumer_token, fragments, n_bulk);
        for(size_t ifrag = 0; ifrag < n_read; ifrag++) {
            uint32_t l1id = fragments[ifrag].l1id();
            if(fragments[ifrag].link_id() <= 0) continue;
            if(m_out_queue->count(l1id)==0)
                m_out_queue->emplace( l1id, moodycamel::ConcurrentQueue<DataFragment>(128) );
            if(!is_start) {
                if(l1id != (current_l1id+1)) {
                    logger->warn("DataBuilder::build L1ID out of sync (expected: {0:x}, received: {1:x})", current_l1id+1, l1id);
                }
            }
            current_l1id = l1id;
            if(!m_out_queue->at(l1id).try_enqueue(fragments[ifrag])) {
                logger->warn("DataBuilder::build failed to enqueue frag for L1ID {0:x} (L1 Q size = {1:d})", fragments[ifrag].l1id(), m_out_queue->at(l1id).size_approx());
            }
            m_map_cond->notify_one();
        } // ifrag
    } // while
}

void DataBuilder::stop()
{
    m_map_cond->notify_all();
    if(m_thread.joinable()) {
        m_thread.join();
    }
}

