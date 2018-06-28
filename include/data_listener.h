#ifndef MPMC_DATA_LISTENER_H
#define MPMC_DATA_LISTENER_H

//std/stl
#include <map>
#include <iostream>
#include <string>
#include <thread>
#include <mutex>
#include <vector>

//mpmc
#include "data_fragment.h"
#include "concurrentqueue/concurrentqueue.h"

//boost
#include <boost/asio.hpp>
#include <boost/array.hpp>

//logging
#include "spdlog/spdlog.h"

#define MAX_UDP_LEN 2500
typedef boost::array<uint32_t, MAX_UDP_LEN> data_array_t;

class DataListener {

    public :

        DataListener(std::string ip, int listen_port, std::shared_ptr<boost::asio::io_service> io_service,
                        moodycamel::ConcurrentQueue<DataFragment>* input_queue, std::atomic_bool & listen_flag);

        virtual ~DataListener() {
            stop();
        }

        virtual void start();
        virtual void stop();
        virtual void listen();

        virtual void handle_receive(const boost::system::error_code& error, std::size_t bytes_transferred);

        bool continue_listening();


    protected :

        std::shared_ptr<spdlog::logger> logger;
        moodycamel::ProducerToken* token;

        std::atomic_bool* m_listen_flag;

        std::vector<DataFragment> m_data_to_enqueue;

        unsigned int m_port;
        std::vector<uint32_t> m_data_in;
        DataFragment* m_fragment_in;
        moodycamel::ConcurrentQueue<DataFragment>* m_in_queue;
        std::thread m_thread;
        bool m_active;
        boost::asio::ip::udp::endpoint m_endpoint;
        data_array_t m_receive_buffer;
        std::shared_ptr<boost::asio::io_service> m_io_service;
        std::unique_ptr<boost::asio::ip::udp::socket> m_socket;

    


};


#endif
