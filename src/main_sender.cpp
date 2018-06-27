//std/stl
#include <thread>
#include <iostream>
#include <vector>
#include <string>
#include <future>
#include <mutex>
#include <memory>
#include <cstdint>
#include <chrono>
using namespace std;

//boost
#include <boost/asio.hpp>
#include <boost/array.hpp>

int continue_sending = 0;
std::mutex flag_mutex;
std::string ip = "127.0.0.1";

void help() {
    cout << "main_sender" << endl;
    cout << endl;
    cout << "Options:" << endl;
    cout << " -n|--n-senders    number of independent sending sockets [default: 1]" << endl;
    cout << " -h|--help         print this help message" << endl;
}

//class Sender {
//
//    public :
//        Sender(int id, int port) :
//            m_id(id), m_port(port) {}
//
//        std::unique_ptr<boost::asio::ip::udp::socket> socket;
//
//        int m_id;
//        int m_port;
//
//        
//
//
//}; // class

void sender_thread(int id, std::shared_ptr<boost::asio::io_service> service) {

    cout << "sender_thread    [" << std::this_thread::get_id() << "]   starting up : id = " << id << endl;

    vector<string> ports = { "1234", "1235", "1236" };
    vector<uint32_t> data = { 0xaa, 0xbb, 0xcc };
    uint32_t l1id = 0; 
    

    std::unique_ptr<boost::asio::ip::udp::socket> sender; //(*service,
    sender = std::make_unique<boost::asio::ip::udp::socket>(*service,
                boost::asio::ip::udp::endpoint(boost::asio::ip::udp::v4(), std::stoi(ports.at(id)) + 10)); // add the offset to not overbind the ports to which the listeners are binding to

    boost::asio::ip::udp::resolver resolver(*service);
    boost::asio::ip::udp::resolver::query query(boost::asio::ip::udp::v4(), ip, ports.at(id));
    boost::asio::ip::udp::resolver::iterator iter;

    try {
        iter = resolver.resolve(query);
    }
    catch(std::exception& e) {
        cout << "sender_thread    [id=" << id << "]    Unable to resolve IP endpoint for port = " << ports.at(id) << endl;
        return;
    }
    boost::asio::ip::udp::endpoint endpoint = *iter;

    while(true) {
        vector<uint32_t> data_to_send = { l1id, (uint32_t)data.at(id) };
        sender->send_to(boost::asio::buffer( data_to_send ), endpoint );
        std::this_thread::sleep_for(std::chrono::microseconds(10));
        //std::this_thread::sleep_for(std::chrono::milliseconds(1));
        l1id++;
        std::lock_guard<std::mutex> lock(flag_mutex);
        if(continue_sending > 0) break;
    }
    return;
}

int main(int argc, char* argv[]) {

    int n_senders = 1;

    int optin(1);
    while(optin < argc) {
        string in = argv[optin];
        if      (in == "-n" || in == "--n-senders") { n_senders = std::atoi(argv[++optin]); }
        else if (in == "-h" || in == "--help") { help(); return 0; }
        else {
            cout << "main_sender    ERROR unknown command line argument (=" << in << ") provided" << endl;
            return 1;
        }
        optin++;
    } // while

    if(n_senders > 3) {
        cout << "main_sender    ERROR requested more than 3 senders, can only do <=3" << endl;
        return 1;
    }

    
    std::shared_ptr<boost::asio::io_service> io_service;
    io_service = std::make_shared<boost::asio::io_service>();
    vector<std::thread> running_threads;
    for(int i = 0; i < n_senders; i++) {
        running_threads.push_back( std::thread(sender_thread, i, io_service) );
    }

    std::cin >> continue_sending;

    for(size_t i = 0; i < running_threads.size(); i++) running_threads.at(i).join();

    return 0;
}
