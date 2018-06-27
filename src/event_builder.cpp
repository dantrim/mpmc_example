#include "event_builder.h"

//std/stl
#include <iostream>
using namespace std;

EventBuilder::EventBuilder(
    unsigned int n_links,
    std::map<unsigned int, moodycamel::ConcurrentQueue<DataFragment*>*>* l1_queue,
    std::shared_ptr<std::mutex> map_mutex,
    std::shared_ptr<std::condition_variable> map_cond) :
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
    logger = spdlog::get("mm_ddaq");
    m_l1_counts.clear();
    m_l1_queue = l1_queue;

    // start processing thread
    m_thread = std::thread( [this] () {
        build();
    });
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

    while(m_active) {

        std::unique_lock<std::mutex> lock(*m_map_mutex);
//        std::lock_guard<std::mutex> lock(m_store_mutex);
        bool map_access = !(m_map_cond->wait_for(lock, std::chrono::milliseconds(1000))==std::cv_status::timeout);
        if(!map_access) {
            logger->warn("EventBuilder::build timeout, could not gain access to L1 map!");
            continue;
        }

        //if(m_l1_queue->size()<500) continue;
        if(!(m_l1_queue->size()>0)) continue;


        //std::unique_lock<std::mutex> lock(*m_map_mutex);
        unsigned selected_l1 = m_l1_queue->begin()->first;
        auto l1_items = m_l1_queue->begin()->second;
        //if(l1_items->size_approx()<2) continue;
        m_n_total++;

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
        
        

        //delete l1_items;
        //m_l1_queue->erase(selected_l1);
        //int expected = 2;
        //if(n_read!=expected) { n_bad_rec++; logger->warn("EventBuilder::build    did not read expected ({0:d}) number of events from L1ID ({1:x}), got {2:d} events ==> frac bad {3:f}, total in map {4:d}", expected, selected_l1, n_read, float(n_bad_rec) / float(n_total), m_l1_queue->size());
        //}

   //     unsigned int expected_l1 = last_l1++;
   //     unsigned int first_l1 = m_l1_queue->begin()->first;
   //     unsigned int selected_l1 = 0;
   //     if(expected_l1 != first_l1) {
   //         if(expected_l1 < first_l1) {
   //             logger->warn("EventBuilder::build    the expected L1ID ({0:x}) is earlier than the earliers L1ID in the map queue ({1:d})!", expected_l1, first_l1);
   //             selected_l1 = first_l1;
   //         }
   //         if(expected_l1 >= first_l1) {
   //             selected_l1 = first_l1;
   //         }
   //     }
   //     else selected_l1 = first_l1;

   //     auto l1_items = m_l1_queue->at(selected_l1);
   //     DataFragment* fragment = nullptr;
   //     while(l1_items->try_dequeue(fragment)) {
   //         delete fragment;
   //     }
   //     delete l1_items;
   //     m_l1_queue->erase(selected_l1);

   //     last_l1 = selected_l1;


            ////////
//        {
//        std::lock_guard<std::mutex> lock(*m_map_mutex);
//        if(m_l1_queue->size()>10) {
//
//            unsigned int exp_l1 = last_l1++;
//            unsigned int lowest_in = 0xffffffff;
//            if(!m_l1_queue->count(exp_l1)) {
//                lowest_in = m_l1_queue->begin()->first;
//                if(lowest_in > exp_l1) exp_l1 = lowest_in;
//                else logger->warn("EventBuilder::build   weird state! (lowest = {0:x}, exp_l1 = {1:x})", lowest_in, exp_l1);
//            }
//            
//            try {
//                auto l1_items = m_l1_queue->at(exp_l1);
//                int n_l1 = 0;
//                DataFragment* fragment = nullptr;
//                size_t init_size = l1_items->size_approx();
//                while(l1_items->try_dequeue(fragment)) {
//                    logger->info("EventBuilder::build   fragment l1id {}", fragment->l1id());
//                    n_l1++;
//                }
//                if(n_l1!=2) logger->warn("EventBuilder::build  did not retrieve 2 fragments for L1ID {1:x} (got {1:d})", exp_l1, n_l1);
//
//                try {
//                    delete l1_items;
//                    m_l1_queue->erase(exp_l1);
//                } catch(std::exception& e) {
//                    logger->error("EventBuilder::build   failed to erase L1ID {0:x}", exp_l1);
//                }
//            }
//            catch(std::exception& e) {
//                logger->warn("EventBuilder::build could not retrieve fragment for L1ID {0:x} (expected L1ID is {1:x})", exp_l1, exp_l1);
//            }
//            //logger->info("EventBuilder::build  retrieved {0:d} items for L1ID {1:x}", n_l1, l1id);
//
//        }
//        }
    } // while active
}

void EventBuilder::stop()
{
    m_active = false;
    logger->info("EventBuilder::stop    stopping building with {0:d} L1ID's left in map", m_l1_queue->size());
    if(m_thread.joinable()) {
        m_thread.join();
    }
}
