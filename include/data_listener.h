#ifndef MPMC_DATA_LISTENER_H
#define MPMC_DATA_LISTENER_H

//std/stl
#include <map>
#include <iostream>
#include <string>
#include <thread>
#include <mutex>

//mpmc
#include "data_fragment.h"
#include "concurrentqueue/concurrentqueue.h"

//boost
#include <boost/asio.hpp>
#include <boost/array.hpp>

#define MAX_UDP_LEN 2500
typedef boost::array<uint8_t, MAX_UDP_LEN> data_array_t;

class DataListener {

    public :

        DataListener(std::string ip, int listen_port, std::shared_ptr<boost::asio::io_service> io_service,
                        moodycamel::ConcurrentQueue<uint8_t*>* input_queue,
                        std::map<unsigned int, moodycamel::ConcurrentQueue<DataFragment*>*> & output_queue);

        virtual ~DataListener() {
            stop();
        }

        virtual void start();
        virtual void listen();

        virtual void handle_receive(const boost::system::error_code& error, std::size_t bytes_transferred);

        virtual void stop() {
            m_active = false;
            if(m_thread.joinable()) {
                m_thread.join();
            }
        }

    protected :

        unsigned int m_port;
        moodycamel::ConcurrentQueue<uint8_t*>* m_in_queue;
        std::map<unsigned int, moodycamel::ConcurrentQueue<DataFragment*>* > m_out_queue;
        std::thread m_thread;
        bool m_active;
        boost::asio::ip::udp::endpoint m_endpoint;
        data_array_t m_receive_buffer;
        std::shared_ptr<boost::asio::io_service> m_io_service;
        std::unique_ptr<boost::asio::ip::udp::socket> m_socket;

    


};


#endif
