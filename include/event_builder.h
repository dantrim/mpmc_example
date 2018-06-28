#ifndef MPMC_EVENT_BUILDER_H
#define MPMC_EVENT_BUILDER_H

//std/stl
#include <iostream>
#include <string>
#include <thread>
#include <mutex>
#include <memory>
#include <atomic>
#include <condition_variable>

//mpmc
#include "data_fragment.h"
#include "concurrentqueue/concurrentqueue.h"

//boost
#include <boost/asio.hpp>

//logging
#include "spdlog/spdlog.h"

class EventBuilder {

    public :
        EventBuilder(
            unsigned int n_links,
            std::map<unsigned int, moodycamel::ConcurrentQueue<DataFragment>>* l1_queue,
            std::shared_ptr<std::mutex> map_mutex, std::shared_ptr<std::condition_variable> map_cond,
            std::atomic_int & build_flag);

        void build();
        void start();
        void stop();
        int build_flag();
        int continue_building();
        void flush();
        uint32_t n_in_map();

        virtual ~EventBuilder() {
            stop();
        }

        float bad_frac() {
            return float(m_n_bad) / float(m_n_total);
        }
        float ok_frac() {
            return float(m_n_ok) / float(m_n_total);
        }
        float more_frac() {
            return float(m_more) / float(m_n_total);
        }
        float less_frac() {
            return float(m_less) / float(m_n_total);
        }
        float amb_frac() {
            return float(m_bad_amb) / float(m_n_total);
        }

        unsigned int n_total() const { return m_n_total; }
        unsigned int n_ok() const { return m_n_ok; }
        unsigned int n_bad() const { return m_n_bad; }
        unsigned int n_more() const { return m_more; }
        unsigned int n_less() const { return m_less; }
        unsigned int n_amb() const { return m_bad_amb; }
        

    protected :
        std::mutex m_store_mutex;

        std::shared_ptr<spdlog::logger> logger;

        std::atomic_int * m_build_flag;

        std::shared_ptr<std::mutex> m_map_mutex;
        std::shared_ptr<std::condition_variable> m_map_cond;
        unsigned int m_n_links;
        unsigned int m_less;
        unsigned int m_more;
        unsigned int m_bad_amb;
        unsigned int m_n_bad;
        unsigned int m_n_ok;
        unsigned int m_n_total;
        std::map<unsigned int, unsigned int> m_l1_waits;
        std::map<unsigned int, unsigned int> m_l1_counts;

        std::map<unsigned int, moodycamel::ConcurrentQueue<DataFragment>>* m_l1_queue;
        std::thread m_thread;

};


#endif
