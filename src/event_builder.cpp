#include "event_builder.h"

//std/stl
#include <iostream>
using namespace std;

EventBuilder::EventBuilder(
    unsigned int n_links,
    std::map<unsigned int, moodycamel::ConcurrentQueue<DataFragment>>* l1_queue,
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
        m_map_cond(map_cond),
        m_active(true)
{
    flushing = false;
    logger = spdlog::get("mm_ddaq");
    m_l1_counts.clear();
    m_l1_queue = l1_queue;

    m_build_flag = & build_flag;

    // start processing thread
    //m_thread = std::thread( [this] () {
    //    build();
    //});
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

    uint32_t expected_l1 = 0;
    uint32_t last_l1 = 0;
    bool is_first = true;

    int n_total = 0;
    int n_bad_rec = 0;
    float bad_frac = 0.0;

    uint32_t n_for_flush = 0;

    while(build_flag()<2) {

        std::unique_lock<std::mutex> lock(*m_map_mutex);
//        std::lock_guard<std::mutex> lock(m_store_mutex);
        if(build_flag()==0) {
            bool map_access = !(m_map_cond->wait_for(lock, std::chrono::milliseconds(1000))==std::cv_status::timeout);
            if(!map_access && build_flag()==0) {
                logger->warn("EventBuilder::build timeout, could not gain access to L1 map!");
                continue;
            }
        }
        else if(build_flag()==1 && m_l1_queue->size()==0)
            m_build_flag->store(2);

        //if(m_l1_queue->size()<500) continue;
        //if(!(m_l1_queue->size()>0)) continue;

        //if(flushing && m_l1_queue->size()==0) break;

        //std::unique_lock<std::mutex> lock(*m_map_mutex);
        unsigned int selected_l1 = m_l1_queue->begin()->first;
        auto & l1_items = m_l1_queue->begin()->second;
        //if(l1_items->size_approx()<2) continue;
        m_n_total++;

        DataFragment fragment;
        unsigned int n_read = 0;
        while(l1_items.try_dequeue(fragment)) {
            //logger->info("EventBuilder::build dq'd fragment for L1ID {0:x} [size {1:d}", fragment.l1id(), l1_items->size_approx());
            n_read++;
            if(fragment.l1id() == 0x0) {
                logger->info("EventBuilder::build   fragment for L1ID 0x0 from port {0:d}", fragment.link_id());
            }
            if(fragment.l1id() != selected_l1) logger->error("EventBuilder::build fragment L1ID does not equal selected L1 (fragmetn L1 = {0:x}, selected = {1:x})", fragment.l1id(), selected_l1);
            m_l1_counts[fragment.l1id()]++;
        }// dequeuing
        //logger->info("EventBuilder::build   read {0:d} fragments for L1ID {1:x}", n_read, fragment.l1id());

        if(m_l1_counts[selected_l1] == m_n_links) {
            m_n_ok++;
            //delete l1_items;
            m_l1_queue->erase(selected_l1);
            m_l1_waits.erase(selected_l1);
        }
        else if(m_l1_counts[selected_l1] < m_n_links) {
            // assume
            continue;
        }
        else if(m_l1_counts[selected_l1] > m_n_links) {
            logger->error("EventBuilder::build more ({1:d}) fragments found than # of links for L1ID {0:x}", selected_l1, m_l1_counts[selected_l1]);
            if(selected_l1==0x0) {
                //delete l1_items;
                m_l1_queue->erase(selected_l1);
                m_l1_waits.erase(selected_l1);
            }
        }
    } // while build flag == 0
    //flush();
//    while(build_flag()==1) {
//        logger->info("EventBuilder:build   start pumping...");
//        unsigned int n_pumped = 0;
//        unsigned int n_l1_pumped = 0;
//        while(m_l1_queue->size()>0) {
//            n_l1_pumped++;
//            unsigned int l1 = m_l1_queue->begin()->first;
//            auto & q = m_l1_queue->begin()->second;
//            DataFragment f;
//            while(q.try_dequeue(f)) {
//                n_pumped++;
//            }
//            m_l1_queue->erase(l1);
//        }
//        logger->info("EventBuilder::build    pumped {0:d} L1IDs ({1:d} fragments)", n_l1_pumped, n_pumped);
//    }

/*

        DataFragment* fragment = nullptr;
        unsigned int n_read = 0;
        n_total++;
        while(l1_items->try_dequeue(fragment)) {
            n_read++;
            if(fragment->l1id() != selected_l1) logger->error("EventBuilder::build   fragment L1ID does not equal selected L1 (frag = {0:x}, seleected = {1:x})", fragment->l1id(), selected_l1);
            m_l1_counts[fragment->l1id()]++;
            delete fragment;
        }

       // std::this_thread::sleep_for(std::chrono::milliseconds(1));
        

//        int exp_cnt = 2;
//        m_n_links = exp_cnt;
        if(m_l1_counts[selected_l1] == m_n_links) {
            //logger->info("EventBuilder::build  correct # of links received for L1ID {0:x} (got: {1:d}, expect {2:d})", selected_l1, m_l1_counts[selected_l1], m_n_links);
            m_n_ok++;
            delete l1_items;
            m_l1_queue->erase(selected_l1);
            m_l1_waits.erase(selected_l1);

            continue;
        }
        else if(m_l1_counts[selected_l1] < m_n_links) {
            if(m_l1_waits[selected_l1] > 100) {
                //logger->error("EventBuilder::build waited 10 times for L1ID {0:x}, skipping it! (we only received {1:d} fragments from it and we expected {2:d})", selected_l1, m_l1_counts[selected_l1], m_n_links); 
                delete l1_items;
                m_l1_queue->erase(selected_l1);
                m_l1_waits.erase(selected_l1);

                m_less++;

                m_n_bad++;

                continue;
            }
            else {
                m_l1_waits[selected_l1]++;
            }
        }
        else if(m_l1_counts[selected_l1] > m_n_links) {
            //logger->error("EventBuilder::build received more fragments for L1ID {0:x} than expected! (we got {1:d} and expect {2:d}", selected_l1, m_l1_counts[selected_l1], m_n_links);
            delete l1_items;
            m_l1_queue->erase(selected_l1);
            m_l1_waits.erase(selected_l1);

            m_more++;
            //m_n_bad++;

            //continue;
        }
        else {
            m_bad_amb++;
            m_n_bad++;
            delete l1_items;
            m_l1_queue->erase(selected_l1);
            m_l1_waits.erase(selected_l1);
        }
        
    } // while active
*/
}

uint32_t EventBuilder::n_in_map()
{
    return m_l1_queue->size();
}

void EventBuilder::flush() {

    logger->info("EventBuilder:build   start pumping...");
    unsigned int n_pumped = 0;
    unsigned int n_l1_pumped = 0;
    while(build_flag()==1) {
        if(m_l1_queue->size()>0) {
            n_l1_pumped++;
            unsigned int l1 = m_l1_queue->begin()->first;
            auto & q = m_l1_queue->begin()->second;
            DataFragment f;
            while(q.try_dequeue(f)) {
                n_pumped++;
            }
            m_l1_queue->erase(l1);
        }
        else {
            break;
        }
    }
   logger->info("EventBuilder::build    pumped {0:d} L1IDs ({1:d} fragments)", n_l1_pumped, n_pumped);
}

void EventBuilder::stop()
{
    //flushing = true;
    m_active = false;
//    logger->info("EventBuilder::stop    stopping building with {0:d} L1ID's left in map", n_in_map());;
    if(m_thread.joinable()) {
        m_thread.join();
    }

    m_build_flag->store(1);

    //m_thread = std::thread( [this] () {
    //        flush();
    //});

    //if(m_thread.joinable())
    //    m_thread.join();

    //if(m_l1_queue->size()>0) {
    //    int n_d = m_l1_queue->size();
    //    int n_deq = 0;
    //    while(n_deq < n_d) {
    //        unsigned selected_l1 = m_l1_queue->begin()->first;
    //        auto l1_items = m_l1_queue->begin()->second;
    //        DataFragment* fragment = nullptr;
    //        int n_deq = 0;
    //        while(l1_items->try_dequeue(fragment)) {
    //            n_deq++;
    //        }
    //    }
    //    logger->info("EventBuilder::stop   flushed {0:d} fragments", n_deq);
    //}
}
