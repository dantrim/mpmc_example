#ifndef MPMC_DATA_BUILDER_H
#define MPMC_DATA_BUILDER_H

//std/stl
#include <map>
#include <iostream>
#include <string>
#include <thread>
#include <mutex>
#include <atomic>
#include <condition_variable>

//mpmc
#include "data_fragment.h"
#include "concurrentqueue/concurrentqueue.h"

//boost
#include <boost/asio.hpp>
#include <boost/array.hpp>

//logging
#include "spdlog/spdlog.h"

//concurrent map
#include "map_defs.h"


class DataBuilder {

    public :
        DataBuilder(moodycamel::ConcurrentQueue<DataFragment*>* input_queue,
            //std::map<unsigned int, moodycamel::ConcurrentQueue<DataFragment>>* output_queue,
            //L1IndexMap* l1_index,
            L1IndexHash & l1_hash,
            //moodycamel::ConcurrentQueue<DataFragment*>* output_queue,
            std::shared_ptr<std::condition_variable> map_condition,
            std::shared_ptr<std::mutex> map_mutex,
            std::shared_ptr<boost::asio::io_service> io_service,
            std::atomic_bool & build_flag);

        void start();
        void build();
        void stop();
        bool continue_building();

        virtual ~DataBuilder() {
            stop();
        }

    protected :

        std::mutex m_store_mutex;
        std::atomic_bool* m_build_flag;
        std::shared_ptr<spdlog::logger> logger;

        std::shared_ptr<std::condition_variable> m_map_cond;
        std::shared_ptr<std::mutex> m_map_mutex;

        moodycamel::ConcurrentQueue<DataFragment*>* m_in_queue;
        //L1IndexMap* m_l1_index;
        L1IndexHash* m_l1_hash;
        //std::map<unsigned int, moodycamel::ConcurrentQueue<DataFragment>>* m_out_queue;
        std::thread m_thread;
        std::shared_ptr<boost::asio::io_service> m_io_service;

        moodycamel::ConsumerToken* consumer_token;
        moodycamel::ProducerToken* producer_token;


};

#endif
