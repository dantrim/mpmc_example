#include "data_builder.h"

//std/stl
#include <sstream>
#include <iostream>
#include <string>
using namespace std;

//logging
#include "spdlog/spdlog.h"

DataBuilder::DataBuilder(moodycamel::ConcurrentQueue<uint8_t*>* input_queue,
        std::map<unsigned int, moodycamel::ConcurrentQueue<DataFragment*>*> * output_queue,
        std::shared_ptr<std::condition_variable> map_condition,
        std::shared_ptr<std::mutex> map_mutex,
        std::shared_ptr<boost::asio::io_service> io_service) :
            m_in_queue(input_queue),
            m_active(true)
{

    logger = spdlog::get("mm_ddaq");
    m_io_service = io_service;

    m_out_queue = output_queue;
    m_map_cond = map_condition;
    m_map_mutex = map_mutex;

//    m_thread = std::thread( [this] () {
//        build();
//    });
}
void DataBuilder::start() {
    m_thread = std::thread( [this]() {
        m_io_service->run();
    });
}

void DataBuilder::build()
{
    logger->info("DataBuilder::build building starts");

    m_map_cond->notify_all();

    uint8_t* data_in = nullptr;

    uint32_t current_l1id = 0;
    bool is_start = true;

    while(m_active) {
//        m_map_cond->notify_all();

        if(m_io_service->stopped()) break;

        std::lock_guard<std::mutex> lock(*m_map_mutex);
        if(m_in_queue->try_dequeue(data_in)) {
            //m_map_cond->notify_all();

//            cout << "DataBuilder::build    [" << std::this_thread::get_id() << "]   receive: " << std::hex << (unsigned)data_in[0] << std::dec << "(queue size = " << m_in_queue->size_approx() << ")" <<   endl;

            uint32_t l1id = (data_in[3] << 24) | (data_in[2] << 16) | (data_in[1] << 8) | (data_in[0]);;
            DataFragment* frag = new DataFragment(l1id);
            if(!is_start) {
                if(l1id != (current_l1id + 1)) {
                    logger->warn("DataBuilder::build L1ID out of sync (expected: {0:x}, received: {1:x})", current_l1id+1, l1id);
//                    cout << "DataBuilder::build    [" << std::this_thread::get_id() << "]    L1ID out of sync (expected = " << std::hex << (current_l1id + 1) << ", got = " << l1id << ")" << endl;
                }
            }

            current_l1id = l1id;


//            std::unique_lock<std::mutex> lock(*m_map_mutex);
           // std::lock_guard<std::mutex> lock(*m_map_mutex);
 //           bool have_map = !(m_map_cond->wait_for(lock, std::chrono::milliseconds(1000))==std::cv_status::timeout);
//            if(!have_map) {
            if(false) {
                logger->warn("DataBuilder::build failed to access L1 queue!");
            }
            else {

                if(m_out_queue->count(l1id)) {
                    m_out_queue->at(l1id)->enqueue(frag);
                    //logger->info("DataBuilder::build L1Queue size for L1ID = {0:x} is {1:d}", (unsigned)l1id, m_out_queue->at(l1id)->size_approx());
                }
                else {
                    m_out_queue->emplace(l1id, new moodycamel::ConcurrentQueue<DataFragment*>(4096));
                    m_out_queue->at(l1id)->enqueue(frag);
                    //logger->info("DataBuilder::build L1Queue size for L1ID = {0:x} is {1:d}", (unsigned)l1id, m_out_queue->at(l1id)->size_approx());
                }
                m_map_cond->notify_one();
            }
            //lock.unlock();
        }
    } // while active

}

void DataBuilder::stop()
{
    m_map_cond->notify_all();
    m_active = false;
    if(m_thread.joinable()) {
        m_thread.join();
    }
}

