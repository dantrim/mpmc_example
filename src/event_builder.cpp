#include "event_builder.h"

//std/stl
#include <iostream>
#include <algorithm> // std::max
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
        m_map_cond(map_cond)
{
    logger = spdlog::get("mm_ddaq");
    m_l1_counts.clear();
    m_l1_queue = l1_queue;

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

    uint32_t expected_l1 = 0;
    uint32_t last_l1 = 0;
    bool is_first = true;

    int n_total = 0;
    int n_bad_rec = 0;
    float bad_frac = 0.0;

    uint32_t n_for_flush = 0;

    while(build_flag()<2) {

        std::unique_lock<std::mutex> lock(*m_map_mutex);
        if(build_flag()==0) {
            bool map_access = !(m_map_cond->wait_for(lock, std::chrono::milliseconds(1000))==std::cv_status::timeout);
            if(!map_access && build_flag()==0) {
                logger->warn("EventBuilder::build timeout, could not gain access to L1 map!");
                continue;
            }
        }
        else if(build_flag()==1 && m_l1_queue->size()==0)
            m_build_flag->store(2);

        
            unsigned int selected_l1 = m_l1_queue->begin()->first;
            auto & l1_items = m_l1_queue->begin()->second;
            m_n_total++;

            DataFragment fragment;
            unsigned int n_read = 0;
            while(l1_items.try_dequeue(fragment)) {
                n_read++;
                if(fragment.l1id() == 0x0) {
                    logger->info("EventBuilder::build   fragment for L1ID 0x0 from port {0:d}", fragment.link_id());
                }
                if(fragment.l1id() != selected_l1) logger->error("EventBuilder::build fragment L1ID does not equal selected L1 (fragmetn L1 = {0:x}, selected = {1:x})", fragment.l1id(), selected_l1);
                m_l1_counts[fragment.l1id()]++;
            }// dequeuing

            if(m_l1_counts[selected_l1] == m_n_links) {
                m_n_ok++;
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
    m_build_flag->store(1);
    if(m_thread.joinable()) {
        m_thread.join();
    }
}
