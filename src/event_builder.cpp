#include "event_builder.h"

//std/stl
#include <iostream>
#include <algorithm> // std::max
#include <set>
using namespace std;

EventBuilder::EventBuilder(
    unsigned int n_links,
    //std::map<unsigned int, moodycamel::ConcurrentQueue<DataFragment>>* l1_queue,
    //L1IndexMap* l1_index,
    L1IndexHash & l1_hash,
    std::shared_ptr<std::mutex> map_mutex,
    std::shared_ptr<std::condition_variable> map_cond,
    std::atomic_int & build_flag) :
        m_n_links(n_links),
        m_less(0),
        m_more(0),
        m_bad_amb(0),
        m_n_bad(0),
        m_n_ok(0),
        m_n_total(0),
        m_map_mutex(map_mutex),
        m_map_cond(map_cond)
{
    logger = spdlog::get("mm_ddaq");
    m_l1_counts.clear();
    //m_l1_queue = l1_queue;
    //m_l1_index = l1_index;
    m_l1_hash = &l1_hash;

    m_build_flag = & build_flag;
}

void EventBuilder::start() {
    m_thread = std::thread( [this] () {
            build();
    });
}

int EventBuilder::build_flag()
{
    return m_build_flag->load(std::memory_order_acquire);
}

int EventBuilder::continue_building()
{
    return m_build_flag->load(std::memory_order_acquire);
}

void EventBuilder::build()
{
    logger->info("EventBuilder::build indexed building starts");

    uint32_t last_l1 = 0xffffffff;
    bool is_first = true;

    int n_total = 0;
    int n_bad_rec = 0;
    float bad_frac = 0.0;

    uint32_t n_for_flush = 0;

    uint32_t expected_l1 = 0;
    bool is_start = true;

    int n_warning = 0;

    uint32_t bs_counter = 0;

    std::map<uint32_t, uint32_t> try_map;
    std::map<uint32_t, bool> erase_map;

    std::set<uint32_t> triggers_received;

    uint32_t bottom = 0x0;

    m_startTime=std::chrono::system_clock::now();
    

    while(build_flag()<2) {

        if(n_warning>200) break;
        if(n_total>100000) break;

        for(int ib = 0; ib < 10; ib++) {


            if(build_flag()==1 && m_l1_hash->size()==0)
                m_build_flag->store(2);

            else if(m_l1_hash->size()==0) continue;

                //float rate = build_rate();
                //if(rate>0)
                //     logger->info("EventBuilder::build build rate update: {}", build_rate());


            auto lt = m_l1_hash->lock_table();
            uint32_t lowest = lt.begin()->first;
            lt.unlock();
            bottom = lowest;

            if(is_start) is_start = false;

            moodycamel::ConcurrentQueue<DataFragment>* frag_queue = nullptr;
            DataFragment fragment;
            if(m_l1_hash->find(bottom, frag_queue)) {
                m_n_total++;
                //size_t n_read = 0;
                //stringstream sx;
                //while(frag_queue->try_dequeue(fragment)) {
                ////    sx << " " << fragment.link_id(); 
                //    n_read++;
                //    m_l1_counts[bottom]++;
                //}
                DataFragment frag_array [20];
                size_t n_read = frag_queue->try_dequeue_bulk(frag_array, 20);
                //logger->info("EventBuilder::build  n_read {0:d} for bottom = {1:d}", n_read, bottom);
                m_l1_counts[bottom]+=n_read;
                if(m_l1_counts[bottom]==m_n_links) {
                    m_n_ok++;
                    m_l1_hash->erase(bottom);
                    m_l1_counts.erase(bottom);
                    bottom++;
                }
                else if(m_l1_counts[bottom]<m_n_links) {
                    m_l1_waits[bottom]++;
                    if(m_l1_waits[bottom]>1000) {
                        logger->info("EventBuilder::build we  have waited too long for L1 {0:d} to catch up (q size {1:d})", bottom, m_l1_hash->size());
                        m_l1_hash->erase(bottom);
                        m_l1_counts.erase(bottom);
                        m_l1_waits.erase(bottom);
                        bottom++;
                    }
                    continue;
                }
                else if(m_l1_counts[bottom]>m_n_links) {
                    logger->warn("EventBuilder::build more fragments than linkes for L1ID {0:x}", bottom);
                    m_l1_hash->erase(bottom);
                    m_l1_counts.erase(bottom);
                    bottom++;
                }
            }
        } // ib
    } // while build flag 2
    //flush();
}

uint32_t EventBuilder::n_in_map()
{
    return m_l1_hash->size();
//    return m_l1_queue->size();
}

void EventBuilder::flush() {

    logger->info("EventBuilder:build   start pumping...");
    //unsigned int n_pumped = 0;
    //unsigned int n_l1_pumped = 0;
    //while(build_flag()==1) {
    //    if(m_l1_queue->size()>0) {
    //        n_l1_pumped++;
    //        unsigned int l1 = m_l1_queue->begin()->first;
    //        auto & q = m_l1_queue->begin()->second;
    //        DataFragment f;
    //        while(q.try_dequeue(f)) {
    //            n_pumped++;
    //        }
    //        m_l1_queue->erase(l1);
    //    }
    //    else {
    //        break;
    //    }
    //}
    //logger->info("EventBuilder::build    pumped {0:d} L1IDs ({1:d} fragments)", n_l1_pumped, n_pumped);
}

void EventBuilder::stop()
{
    m_build_flag->store(1);
    if(m_thread.joinable()) {
        m_thread.join();
    }
}
