#include "data_listener.h"

//std/stl
#include <sstream>
#include <iostream>
#include <string>
#include <atomic>
using namespace std;

 #include <netinet/in.h>

//boost
#include <boost/bind.hpp>

DataListener::DataListener(std::string ip_string, int listen_port, std::shared_ptr<boost::asio::io_service> io_service, 
            moodycamel::ConcurrentQueue<DataFragment>* input_queue, std::atomic_bool & listen_flag) :
        token(nullptr),
        m_port(listen_port),
        m_in_queue(input_queue),
        m_active(true)
{
    m_data_to_enqueue.resize(50);

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

    m_listen_flag = &listen_flag;

    cout << "DataListener  listen_flag = " << m_listen_flag << endl;
    cout << "DataListener  listen_flag at start up " << m_listen_flag->load(std::memory_order_acquire) << endl;

    logger->info("DataListener listener for port {} initialized", listen_port);
}

void DataListener::start()
{
    m_thread = std::thread( [this]() {
            // add token
            token = new moodycamel::ProducerToken(*m_in_queue);
            logger->info("DataListener::start listening starting for port {}", m_port);
            m_io_service->run();
//            listen();
        });
}

bool DataListener::continue_listening()
{
    return m_listen_flag->load(std::memory_order_acquire);
}

void DataListener::stop()
{
    m_active = false;
    //m_listener_flag->load(std::memory_order_acquire);
    if(m_thread.joinable()) {
        m_thread.join();
    }
}

void DataListener::listen()
{
    if(continue_listening()) {
        m_socket->async_receive(boost::asio::buffer(m_receive_buffer),
            boost::bind(&DataListener::handle_receive, this,
            boost::asio::placeholders::error, boost::asio::placeholders::bytes_transferred));
    }
}

void DataListener::handle_receive(const boost::system::error_code& error,
        std::size_t n_bytes)
{

    size_t n32 = n_bytes/4;
    m_data_in.clear();
    
    for(size_t i = 0 ; i < n32; i++)
        m_data_in.push_back(htonl(ntohl(m_receive_buffer.at(i))));

    DataFragment fragment;//(m_data_in.at(0));
    fragment.set_link(m_port);
    fragment.set_l1id(m_data_in.at(0));
  //  if(m_data_in.at(0) < 5) {
  //      logger->info("DataListener::handle_receive    L1 {0:x}  port {1:d}", fragment.l1id(), fragment.link_id());
  //  }
  //  stringstream sx;
  //  for(auto x : m_data_in)
  //      sx << " " << std::hex << (unsigned)x;
  //  logger->info("DataListener::handle_receive: data [{2:d} v {3:d}] [L1 {0:x}] = {1}", m_data_in.at(0), sx.str(), n_bytes, n32);
//    logger->info("DataListener::handle_receive   enqueueing data for L1ID : {0:x}", m_data_in.at(0));
    fragment.m_packet = m_data_in;
    m_data_to_enqueue.push_back(fragment);

    if(m_data_to_enqueue.size()>=50) {
        if(!m_in_queue->try_enqueue_bulk(*token, m_data_to_enqueue.data(), 50)) {
            logger->warn("DataListener::handle_receive unable to enqueue incoming data for L1ID {0:x}", (unsigned)m_data_in[0]);
        }
        else {
            //logger->info("DataListener::handle_receive data enqueued OK");
            m_data_to_enqueue.clear();    
        }
    }


    //if(!m_in_queue->enqueue(*token, m_fragment_in)) {
    //    logger->info("DataListener::handle_receive unable to enqueue incoming data for L1ID {0:x}", (unsigned)data_in[0]);
    //}

    if(continue_listening()) {
        listen();
    }

    return;
}



