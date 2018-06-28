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
            m_in_queue(input_queue),
            m_active(true)
{

    logger = spdlog::get("mm_ddaq");
    m_io_service = io_service;

    m_out_queue = output_queue;
    m_map_cond = map_condition;
    m_map_mutex = map_mutex;

    m_build_flag = &build_flag;

//    m_thread = std::thread( [this] () {
//        build();
//    });
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

    unsigned int n_bulk = 50;
    DataFragment fragments [ n_bulk ];

    uint32_t current_l1id = 0xffffffff;
    bool is_start = true;

    while(continue_building()) {

        if(m_io_service->stopped()) break;

        std::lock_guard<std::mutex> lock(*m_map_mutex);
        size_t n_read = m_in_queue->try_dequeue_bulk(*consumer_token, fragments, n_bulk);
        //logger->info("DataBuilder::build  n_read = {}", n_read);
        for(size_t ifrag = 0; ifrag < n_read; ifrag++) {
            uint32_t l1id = fragments[ifrag].l1id();
            if(fragments[ifrag].link_id() <= 0) continue;
            //if(l1id<5) {
            //    logger->info("DataBuilder::build XXX L1ID {0:x} port {1:d}", fragments[ifrag].l1id(), fragments[ifrag].link_id());
            //}
            if(m_out_queue->count(l1id)==0)
                m_out_queue->emplace( l1id, moodycamel::ConcurrentQueue<DataFragment>(128) );
            //logger->info("DataBuilder::build fragment l1id = {0:d} {1:d}", l1id, fragments[ifrag].link_id());
            if(!is_start) {
                if(l1id != (current_l1id+1)) {
                    logger->warn("DataBuilder::build L1ID out of sync (expected: {0:x}, received: {1:x})", current_l1id+1, l1id);
                }
            }
            current_l1id = l1id;
            if(!m_out_queue->at(l1id).try_enqueue(fragments[ifrag])) {
                logger->warn("DataBuilder::build failed to enqueue frag for L1ID {0:x} (L1 Q size = {1:d})", fragments[ifrag].l1id(), m_out_queue->at(l1id).size_approx());
            }
            //if(m_out_queue->count(l1id)) {
            //    //logger->info("DataBuilder::build Q size for L1ID {0:x} = {1:d}", l1id, m_out_queue->at(l1id)->size_approx());
            //    if(!m_out_queue->at(l1id)->try_enqueue(fragments[ifrag])) {
            //        logger->warn("DataBuilder::build failed to enqueue frag for L1ID {0:x} (L1 Q size = {1:d})", fragments[ifrag].l1id(), m_out_queue->at(l1id)->size_approx());
            //    }
            //}
            //else {
            //    m_out_queue->emplace(l1id, new moodycamel::ConcurrentQueue<DataFragment>(128));
            //    if(!m_out_queue->at(l1id)->try_enqueue(fragments[ifrag])) {
            //        logger->warn("DataBuilder::build failed to enqueue frag for L1ID {0:x}", fragments[ifrag].l1id());
            //    }
            //}
            m_map_cond->notify_one();
        } // ifrag
    } // while


        //////// [BULK TEST END]
/*


        std::lock_guard<std::mutex> lock(*m_map_mutex);
        if(m_in_queue->try_dequeue(*consumer_token, data_in, n_bulk)) {
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
                    if(!m_out_queue->at(l1id)->try_enqueue(frag))
                        logger->warn("DataBuilder::build failed to enqueue frag for L1ID {0:x}", frag->l1id());
                    //logger->info("DataBuilder::build L1Queue size for L1ID = {0:x} is {1:d}", (unsigned)l1id, m_out_queue->at(l1id)->size_approx());
                }
                else {
                    m_out_queue->emplace(l1id, new moodycamel::ConcurrentQueue<DataFragment*>(4096));
                    if(!m_out_queue->at(l1id)->try_enqueue(frag))
                        logger->warn("DataBuilder::build failed to enqueue frag for L1ID {0:x}", frag->l1id());
                    //logger->info("DataBuilder::build L1Queue size for L1ID = {0:x} is {1:d}", (unsigned)l1id, m_out_queue->at(l1id)->size_approx());
                }
                m_map_cond->notify_one();
            }
            //lock.unlock();
        }
    } // while active
*/

}

void DataBuilder::stop()
{
    m_map_cond->notify_all();
    m_active = false;
    if(m_thread.joinable()) {
        m_thread.join();
    }
}

