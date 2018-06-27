#include "data_listener.h"

//std/stl
#include <sstream>
#include <iostream>
#include <string>
using namespace std;

//boost
#include <boost/bind.hpp>

DataListener::DataListener(std::string ip_string, int listen_port, std::shared_ptr<boost::asio::io_service> io_service, 
            moodycamel::ConcurrentQueue<uint8_t*>* input_queue) :
        m_port(listen_port),
        m_in_queue(input_queue),
        m_active(true)
{
    logger = spdlog::get("mm_ddaq");

    stringstream port_str;
    port_str << listen_port;
    m_io_service = io_service;

    boost::asio::ip::udp::resolver resolver(*io_service);
    boost::asio::ip::udp::resolver::query query(boost::asio::ip::udp::v4(), ip_string, port_str.str());
    boost::asio::ip::udp::resolver::iterator iter;
    try {
        iter = resolver.resolve(query);
    }
    catch(std::exception& e) {
        logger->error("DataListener failed to resolve IP endpoint for (IP,port)=({0},{1})", ip_string, port_str.str());
        exit(1);
    }
    m_endpoint = *iter;
    m_socket = std::make_unique<boost::asio::ip::udp::socket>(*io_service, m_endpoint);

    logger->info("DataListener listener for port {} initialized", listen_port);
}

void DataListener::start()
{
    m_thread = std::thread( [this]() {
            logger->info("DataListener::start listening starting for port {}", m_port);
            m_io_service->run();
//            listen();
        });
}

void DataListener::listen()
{
    if(m_active) {
        m_socket->async_receive(boost::asio::buffer(m_receive_buffer),
            boost::bind(&DataListener::handle_receive, this,
            boost::asio::placeholders::error, boost::asio::placeholders::bytes_transferred));
    }
}

void DataListener::handle_receive(const boost::system::error_code& error,
        std::size_t n_bytes)
{

    uint8_t data_in [ n_bytes ];
    std::copy(m_receive_buffer.begin(), m_receive_buffer.begin() + n_bytes, data_in);

    if(!m_in_queue->enqueue(data_in)) {
        logger->info("DataListener::handle_receive unable to enqueue incoming data for L1ID {0:x}", (unsigned)data_in[0]);
    }

    if(m_active) {
        listen();
    }

    return;
}



