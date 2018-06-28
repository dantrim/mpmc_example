#include "data_builder.h"

//std/stl
#include <sstream>
#include <iostream>
#include <string>
#include <set>
using namespace std;

//logging
#include "spdlog/spdlog.h"

DataBuilder::DataBuilder(moodycamel::ConcurrentQueue<DataFragment>* input_queue,
        //std::map<unsigned int, moodycamel::ConcurrentQueue<DataFragment>> * output_queue,
        L1IndexHash & l1_hash,
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

    //m_out_queue = output_queue;
    //m_l1_index = l1_index;
    m_l1_hash = &l1_hash;

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

    unsigned int n_bulk = 100;
    DataFragment fragments [ n_bulk ];
    uint32_t current_l1id = 0xffffffff;
    bool is_start = true;
    std::set<unsigned int> links;
    while(continue_building()) {

        links.clear();
        if(m_io_service->stopped()) break;

        size_t n_read = m_in_queue->try_dequeue_bulk(*consumer_token, fragments, n_bulk);
        for(size_t ifrag = 0; ifrag < n_read; ifrag++) {
                if(fragments[ifrag].link_id()>0) {
                    //logger->info("DataBuilder::build valid link (={0:d}) for L1 {1:d}", fragments[ifrag].link_id(), fragments[ifrag].l1id());
                    links.insert(fragments[ifrag].link_id());
                }
        }
        //if(links.size()<3) continue;
        //stringstream sx;
        //logger->info("DataBuilder::build   made it past 3 unqie links!");
        for(size_t ifrag = 0; ifrag < n_read; ifrag++) {

            uint32_t frag_l1id = fragments[ifrag].l1id();


            if(frag_l1id <= 0) continue; // invalid


            //if(!is_start) {
            //    if(frag_l1id != (current_l1id+1)) {
            //        logger->warn("DataBuilder::build L1ID out of sync (expected: {0:x}, received: {1:x}", current_l1id+1, frag_l1id);
            //    }
            //}

            // update the currently handled (last) L1ID
            current_l1id = frag_l1id;


            // cuckoo style
            //moodycamel::ConcurrentQueue<DataFragment>* qfrag = nullptr;
            //if(m_l1_hash->find(frag_l1id, qfrag)) {
            //    if(!qfrag->try_enqueue(fragments[ifrag])) {
            //        logger->warn("DataBuilder::updatel1 failed to enqueue fragment for L1ID {0:x}, link {1:d} (current table size {2:d})", fragments[ifrag].l1id(), fragments[ifrag].link_id(), m_l1_hash->size());
            //    }
            //}
            //else {
            //    qfrag = new moodycamel::ConcurrentQueue<DataFragment>(128);
            //    if(!m_l1_hash->insert(frag_l1id, qfrag)) {
            //        logger->warn("DataBuilder::updatel1 failed to insert L1 data for L1 {0:x}, link {1:d}", fragments[ifrag].l1id(), fragments[ifrag].link_id());
            //    }
            //}
            moodycamel::ConcurrentQueue<DataFragment>* qfrag = nullptr;//m_l1_hash->find(frag_l1id);
            //if(m_l1_hash->contains(frag_l1id)) {
            if(m_l1_hash->find(frag_l1id, qfrag)) {
                if(!qfrag->try_enqueue(fragments[ifrag])) {
                    logger->warn("DataBuilder::updatel1 failed to enqueue fragment for L1ID {0:x}, link {1:d} (current table size {2:d})", fragments[ifrag].l1id(), fragments[ifrag].link_id(), m_l1_hash->size());
                }
                //else {
                //    m_l1_hash->update(frag_l1id, qfrag);
                //}
                //logger->info("DataBuilder::build   hash size {}" , m_l1_hash->size());
            }
            else {
                m_l1_hash->insert(frag_l1id, new moodycamel::ConcurrentQueue<DataFragment>(128));
                if(m_l1_hash->find(frag_l1id, qfrag)) {
                    if(!qfrag->try_enqueue(fragments[ifrag])) {
                        logger->warn("DataBuilder::updatel1 failed to enqueue fragment for L1ID {0:x}, link {1:d} (current table size {2:d})", fragments[ifrag].l1id(), fragments[ifrag].link_id(), m_l1_hash->size());

                    }
                }
            }


            //auto updatel1 = [&] (moodycamel::ConcurrentQueue<DataFragment>* qq) {
            //    //logger->info("DataBuilder::updatel1 for L1ID {0:x}", fragments[ifrag].l1id());
            //    if(!qq->try_enqueue(fragments[ifrag])) {
            //        logger->warn("DataBuilder::updatel1 failed to enqueue fragment for L1ID {0:x}, link {1:d} (current table size {2:d})", fragments[ifrag].l1id(), fragments[ifrag].link_id(), m_l1_hash->size());
            //    }
            //    //else {
            //    //    //logger->info("DataBuilder::updatel1 sucessful enqueue for L1ID {0:x} (port {1}) ; Q size {2}", fragments[ifrag].l1id(), fragments[ifrag].link_id(), m_l1_hash->size());
            //    //}
            //}; 
            //m_l1_hash->upsert(frag_l1id, updatel1, new moodycamel::ConcurrentQueue<DataFragment>());

            // handle the emplacement into the index
            //moodycamel::ConcurrentQueue<DataFragment>*  frag_queue = m_l1_index->get(frag_l1id);
            //if(!frag_queue) {
            //    turf::LockGuard<turf::Mutex> lock(*m_map_mutex);
            //    ConcurrentMap::Mutator mutator = m_l1_index->insertOrFind(frag_l1id);
            //    frag_queue = mutator.getValue();
            //    if(!frag_queue) {
            //        frag_queue = new moodycamel::ConcurrentQueue<DataFragment>(128);
            //        mutator.assignValue(frag_queue);
            //    } // 2nd check
            //} // first check

            is_start = false;
        } // ifrag
    } // while continue_building

//    m_map_cond->notify_all();
//
//    uint8_t* data_in = nullptr;
//
//    unsigned int n_bulk = 5;
//    DataFragment fragments [ n_bulk ];
//
//    uint32_t current_l1id = 0xffffffff;
//    bool is_start = true;
//
//    while(continue_building()) {
//
//        if(m_io_service->stopped()) break;
//
//        std::lock_guard<std::mutex> lock(*m_map_mutex);
//        size_t n_read = m_in_queue->try_dequeue_bulk(*consumer_token, fragments, n_bulk);
//        for(size_t ifrag = 0; ifrag < n_read; ifrag++) {
//            uint32_t l1id = fragments[ifrag].l1id();
//            if(fragments[ifrag].link_id() <= 0) continue;
//            if(m_out_queue->count(l1id)==0)
//                m_out_queue->emplace( l1id, moodycamel::ConcurrentQueue<DataFragment>(128) );
//            if(!is_start) {
//                if(l1id != (current_l1id+1)) {
//                    logger->warn("DataBuilder::build L1ID out of sync (expected: {0:x}, received: {1:x})", current_l1id+1, l1id);
//                }
//            }
//            current_l1id = l1id;
//            if(!m_out_queue->at(l1id).try_enqueue(fragments[ifrag])) {
//                logger->warn("DataBuilder::build failed to enqueue frag for L1ID {0:x} (L1 Q size = {1:d})", fragments[ifrag].l1id(), m_out_queue->at(l1id).size_approx());
//            }
//            m_map_cond->notify_one();
//        } // ifrag
//    } // while
}

void DataBuilder::stop()
{
//    m_map_cond->notify_all();
    if(m_thread.joinable()) {
        m_thread.join();
    }
}

