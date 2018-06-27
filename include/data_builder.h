#ifndef MPMC_DATA_BUILDER_H
#define MPMC_DATA_BUILDER_H

//std/stl
#include <map>
#include <iostream>
#include <string>
#include <thread>
#include <mutex>
#include <condition_variable>

//mpmc
#include "data_fragment.h"
#include "concurrentqueue/concurrentqueue.h"

//boost
#include <boost/asio.hpp>
#include <boost/array.hpp>

//logging
#include "spdlog/spdlog.h"


class DataBuilder {

    public :
        DataBuilder(moodycamel::ConcurrentQueue<uint8_t*>* input_queue,
            std::map<unsigned int, moodycamel::ConcurrentQueue<DataFragment*>*>* output_queue,
            std::shared_ptr<std::condition_variable> map_condition,
            std::shared_ptr<std::mutex> map_mutex,
            std::shared_ptr<boost::asio::io_service> io_service);

        void start();
        void build();
        void stop();

        virtual ~DataBuilder() {
            stop();
        }

    protected :

        std::mutex m_store_mutex;
        std::shared_ptr<spdlog::logger> logger;

        std::shared_ptr<std::condition_variable> m_map_cond;
        std::shared_ptr<std::mutex> m_map_mutex;

        moodycamel::ConcurrentQueue<uint8_t*>* m_in_queue;
        std::map<unsigned int, moodycamel::ConcurrentQueue<DataFragment*>*>* m_out_queue;
        std::thread m_thread;
        bool m_active;
        std::shared_ptr<boost::asio::io_service> m_io_service;


};

#endif
