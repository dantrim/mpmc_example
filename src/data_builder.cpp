#include "data_builder.h"

//std/stl
#include <sstream>
#include <iostream>
#include <string>
#include <set>
using namespace std;

//logging
#include "spdlog/spdlog.h"

DataBuilder::DataBuilder(moodycamel::ConcurrentQueue<DataFragment*>* input_queue,
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
    //m_out_queue = output_queue;

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

    unsigned int n_bulk = 5;
    DataFragment* fragments [ n_bulk ];
    uint32_t current_l1id = 0xffffffff;
    bool is_start = true;
    while(continue_building()) {

        if(m_io_service->stopped()) break;

//        std::this_thread::sleep_for(std::chrono::microseconds(750));
        DataFragment* fragment = nullptr;
        if(!m_in_queue->try_dequeue(*consumer_token, fragment)) {
            continue;
        }
        //logger->info("DataBuilder::build  link {0:d} queue size {1:d}", fragment->link_id(), m_in_queue->size_approx());
        moodycamel::ConcurrentQueue<DataFragment*>* qfrag = nullptr;
        if(m_l1_hash->find(fragment->l1id(), qfrag)) {
            if(!qfrag->try_enqueue(fragment)) {
                logger->warn("DataBuilder::build failed to enqueue fragment for L1ID {0:x}, link {1:d}", fragment->l1id(), fragment->link_id());
            }
        }
        else {
            m_l1_hash->insert(fragment->l1id(), new moodycamel::ConcurrentQueue<DataFragment*>(128));
            if(m_l1_hash->find(fragment->l1id(), qfrag)) {
                if(!qfrag->try_enqueue(fragment)) {
                    logger->warn("DataBuilder::build 2 failed to enqueue fragment for L1ID {0:x}, link {1:d}", fragment->l1id(), fragment->link_id());
                }
            }
        }
        

        //auto update_l1 = [&] (moodycamel::ConcurrentQueue<DataFragment*>* q) {
        //    if(!q->try_enqueue(fragment)) {
        //        logger->warn("DataBuilder::update_l1 failed to enqueue fragment from link {0:d} for L1ID {1:x}", fragment->link_id(), fragment->l1id());
        //    }
        //};

        //if(!m_l1_hash->update_fn(fragment->l1id(), update_l1)) {
        //    moodycamel::ConcurrentQueue<DataFragment*>* l1q = new moodycamel::ConcurrentQueue<DataFragment*>();
        //    if(!l1q->try_enqueue(fragment)) {
        //        logger->warn("DataBuilder::build failed to enqueue fragment from link {0:d} for L1ID {1:x}", fragment->link_id(), fragment->l1id());
        //    }
        //    m_l1_hash->insert(fragment->l1id(), l1q);
        //}
    }


//        size_t n_read = m_in_queue->try_dequeue_bulk(*consumer_token, fragments, n_bulk);
//        for(size_t ifrag = 0; ifrag < n_read; ifrag++) {
//                logger->info("DataBuilder::build {}", __LINE__);
//                if(fragments[ifrag]->link_id()>0) {
//                    //logger->info("DataBuilder::build valid link (={0:d}) for L1 {1:d}", fragments[ifrag].link_id(), fragments[ifrag].l1id());
//                    links.insert(fragments[ifrag]->link_id());
//                }
//        }
//        //if(links.size()<3) continue;
//        //stringstream sx;
//        //logger->info("DataBuilder::build   made it past 3 unqie links!");
//        for(size_t ifrag = 0; ifrag < n_read; ifrag++) {
//
//            uint32_t frag_l1id = fragments[ifrag]->l1id();
//
//
//            if(frag_l1id <= 0) continue; // invalid
//
//            // update the currently handled (last) L1ID
//            current_l1id = frag_l1id;
//
//            
//
//
//          //  // cuckoo style
//          //  moodycamel::ConcurrentQueue<DataFragment>* qfrag = nullptr;//m_l1_hash->find(frag_l1id);
//          //  //if(m_l1_hash->contains(frag_l1id)) {
//          //  if(m_l1_hash->find(frag_l1id, qfrag)) {
//          //      fragments[ifrag].update_time_stamp();
//          //      if(!qfrag->try_enqueue(fragments[ifrag])) {
//          //          logger->warn("DataBuilder::updatel1 failed to enqueue fragment for L1ID {0:x}, link {1:d} (current table size {2:d})", fragments[ifrag].l1id(), fragments[ifrag].link_id(), m_l1_hash->size());
//          //      }
//          //      //else {
//          //      //    m_l1_hash->update(frag_l1id, qfrag);
//          //      //}
//          //      //logger->info("DataBuilder::build   hash size {}" , m_l1_hash->size());
//          //  }
//          //  else {
//          //      m_l1_hash->insert(frag_l1id, new moodycamel::ConcurrentQueue<DataFragment>(128));
//          //      if(m_l1_hash->find(frag_l1id, qfrag)) {
//          //          fragments[ifrag].update_time_stamp();
//          //          if(!qfrag->try_enqueue(fragments[ifrag])) {
//          //              logger->warn("DataBuilder::updatel1 failed to enqueue fragment for L1ID {0:x}, link {1:d} (current table size {2:d})", fragments[ifrag].l1id(), fragments[ifrag].link_id(), m_l1_hash->size());
//
//          //          }
//          //      }
//          //  }
//
//
//
//            is_start = false;
//        } // ifrag
//    } // while continue_building
}

void DataBuilder::stop()
{
//    m_map_cond->notify_all();
    if(m_thread.joinable()) {
        m_thread.join();
    }
}

