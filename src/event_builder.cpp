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
    std::atomic_int & build_flag,
    int n_to_exp) :
        m_n_links(n_links),
        m_less(0),
        m_more(0),
        m_bad_amb(0),
        m_n_bad(0),
        m_n_ok(0),
        m_n_total(0),
        m_map_mutex(map_mutex),
        m_map_cond(map_cond),
        n_to_exp(n_to_exp)
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
    m_l1_hash->clear();

    uint32_t last_l1id = 0x0;
    bool is_start = true;

    std::set<uint32_t> l1set;

    while(build_flag()<2) {

        if(build_flag()==1 & m_l1_hash->size()==0)
            m_build_flag->store(2);

        if(m_l1_hash->size()==0) continue;

        uint32_t l1id = 0x0;
        std::this_thread::sleep_for(std::chrono::microseconds(500));
        moodycamel::ConcurrentQueue<DataFragment*>* queue = nullptr;
        {
            auto lt = m_l1_hash->lock_table();
            l1id = lt.begin()->first;
            //logger->info("EventBuilder::build   lowest l1id = {0:x} - {1} {2}", l1id, m_n_total, m_l1_hash->size());
            queue = lt.begin()->second;
            lt.unlock();
        }
        bool check = false;
        //if(l1set.count(l1id)) {
        //    check = true;
        //    logger->warn("EventBuilder::build we have already encountered L1ID {0:x}, and it has fragment count of {1:d}", l1id, m_l1_counts[l1id]);
        //}
        l1set.insert(l1id);

        if(queue==nullptr) continue;
        
        if(is_start) { last_l1id = l1id; is_start = false; }
        //if(l1id != (last_l1id+1)) {
        //    logger->warn("EventBuilder::build Current L1ID (={0:x}) is not the expected one (={1:x}), might have lost sync", l1id, last_l1id+1);
        //}


        m_n_total++;

        if(n_to_exp>0 && m_n_total > n_to_exp) {
            logger->info("EventBuilder::build exiting: TOTAL = {0:d}, TOTAL OK = {1:d} --> {2}\%", m_n_total, m_n_ok, 100 * float(m_n_ok) / float(m_n_total));
            exit(0);
        }

        m_startTime=std::chrono::system_clock::now();

        //vector<DataFragment*> fragments;
        //size_t n_frags_read = queue->try_dequeue_bulk(fragments.begin(), 20);
        DataFragment* fragment = nullptr;
        size_t n_frags_read = 0;
        stringstream sx;
        while(queue->try_dequeue(fragment)) {
            sx << " " << fragment->link_id();
            m_l1_counts[fragment->l1id()]++;
            n_frags_read++;
        }
        //if(check) {
        //    logger->warn("EventBuilder::build    for already-seen L1ID {0:x} we have dq'd {1:d} fragments", l1id, n_frags_read);
        //    check = false;
        //}
        if(n_frags_read<m_n_links) {
            if(n_frags_read==0) m_l1_hash->erase(l1id);
            continue;
        }
        //logger->info("EventBuilder::build {}",__LINE__);

        //if(n_frags_read < m_n_links) continue;
        

        if(n_frags_read==m_n_links) {
            //logger->info("EventBuilder::build  n_fragments_read for L1 {0:x} = {1:d} from links {2}", l1id, n_frags_read, sx.str());
            m_n_ok++;
            //delete queue;
            m_l1_hash->erase(l1id);
        }
        else if(n_frags_read>m_n_links) {
            logger->info("EventBuilder::build  more fragments than links for L1 {0:x}", l1id);
            m_l1_hash->erase(l1id);
        }

        last_l1id = l1id;

        //for(auto & f : fragments) {
        //    delete f; 
        //}
        //m_l1_hash->erase(l1id);

    } // while build_flag < 2


//    uint32_t last_l1 = 0xffffffff;
//    bool is_first = true;
//
//    int n_total = 0;
//    int n_bad_rec = 0;
//    float bad_frac = 0.0;
//
//    uint32_t n_for_flush = 0;
//
//    uint32_t expected_l1 = 0;
//    bool is_start = true;
//
//    int n_warning = 0;
//
//    uint32_t bs_counter = 0;
//
//    std::map<uint32_t, uint32_t> try_map;
//    std::map<uint32_t, bool> erase_map;
//
//    std::set<uint32_t> triggers_received;
//
//    uint32_t bottom = 0x0;
//
//    m_startTime=std::chrono::system_clock::now();
//    
//
//    while(build_flag()<2) {
//
//        if(n_warning>200) break;
//        if(n_total>100000) break;
//
//        for(int ib = 0; ib < 10; ib++) {
//
//            //if(build_flag()==1) {
//            //    logger->info("EventBuilder::build build flag == 1, Q size {0:d}", m_l1_hash->size()); 
//            //}
//
//            if(build_flag()==1 && m_l1_hash->size()==0)
//                m_build_flag->store(2);
//
//            else if(m_l1_hash->size()==0) continue;
//
//                //float rate = build_rate();
//                //if(rate>0)
//                //     logger->info("EventBuilder::build build rate update: {}", build_rate());
//
//            uint32_t lowest = 0x0;
//            if(build_flag()==1)
//                while(!m_l1_hash->contains(bottom)) bottom++;
//            else {
//                auto lt = m_l1_hash->lock_table();
//                lowest = lt.begin()->first;
//                lt.unlock();
//                bottom = lowest;
//            }
//
//            //auto lt = m_l1_hash->lock_table();
//            //uint32_t lowest = lt.begin()->first;
//            //lt.unlock();
//            //bottom = lowest;
//
//            if(is_start) is_start = false;
//
//            auto now = std::chrono::system_clock::now();
//            moodycamel::ConcurrentQueue<DataFragment*>* frag_queue = nullptr;
//            //DataFragment* fragment = new DataFragment();
//            if(m_l1_hash->find(bottom, frag_queue)) {
//                if(build_flag()==1) {
//                    DataFragment frag_array2 [20];
//                    //logger->info("EventBuilder::build   dq'ing Q size {0:d}", m_l1_hash->size());
//                    size_t n_read = frag_queue->try_dequeue_bulk(frag_array2, 20);
//                    logger->info("EventBuilder::build  pumping {0:d}", m_l1_hash->size());
//                    m_l1_hash->erase(bottom);
//                    bottom++;
//                    continue;
//                }
//                m_n_total++;
//                //size_t n_read = 0;
//                //stringstream sx;
//                //while(frag_queue->try_dequeue(fragment)) {
//                ////    sx << " " << fragment.link_id(); 
//                //    n_read++;
//                //    m_l1_counts[bottom]++;
//                //}
//                DataFragment* frag_array [20];
//                size_t n_read = frag_queue->try_dequeue_bulk(frag_array, 20);
//                //logger->info("EventBuilder::build  n_read {0:d} for bottom = {1:d}", n_read, bottom);
//                m_l1_counts[bottom]+=n_read;
//                if(m_l1_counts[bottom]==m_n_links) {
//                    m_n_ok++;
//                    for(int i = 0; i < n_read; i++) delete frag_array[i];
//                    m_l1_hash->erase(bottom);
//                    m_l1_counts.erase(bottom);
//                    bottom++;
//                }
//                else if(m_l1_counts[bottom]<m_n_links) {
//                    bool is_bad = false;
//                    for(size_t ifrag = 0; ifrag < n_read; ifrag++) {
//                        auto age = std::chrono::duration_cast<std::chrono::microseconds> ( now - frag_array[ifrag].init_time() );
//                        if(age.count() > 5000) {
//                            is_bad = true;
//                        }
//                    }
//                    logger->warn("EventBuilder::build stale fragment for L1ID {0:x}, removing it from the queue (age = {1:d})", bottom, age.count());
//                    for
//                    m_l1_hash->erase(bottom);
//                    m_l1_counts.erase(bottom);
//                    //m_l1_waits[bottom]++;
//                    //if(m_l1_waits[bottom]>1000) {
//                    //    logger->info("EventBuilder::build we  have waited too long for L1 {0:d} to catch up (q size {1:d})", bottom, m_l1_hash->size());
//                    //    m_l1_hash->erase(bottom);
//                    //    m_l1_counts.erase(bottom);
//                    //    m_l1_waits.erase(bottom);
//                    //    bottom++;
//                    }
//                    continue;
//                }
//                else if(m_l1_counts[bottom]>m_n_links) {
//                    logger->warn("EventBuilder::build more fragments than linkes for L1ID {0:x}", bottom);
//                    m_l1_hash->erase(bottom);
//                    m_l1_counts.erase(bottom);
//                    bottom++;
//                }
//            }
//        } // ib
//    } // while build flag 2
//    //flush();
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
