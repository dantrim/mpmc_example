// std/stl
#include <iostream>
#include <string>
#include <vector>
#include <thread>
#include <chrono>
#include <map>
#include <condition_variable>
#include <mutex>
using namespace std;

// boost
#include "boost/program_options.hpp"
#include "boost/asio.hpp"
#include "boost/bind.hpp"

// queue
#include "concurrentqueue/concurrentqueue.h"

// mpmc
#include "data_listener.h"
#include "data_builder.h"
#include "data_fragment.h"

void help()
{
    return;
}

int main(int argc, char* argv[]) {

    cout << "mpmc_test" << endl;

    int n_listeners = 1;

    int optin(1);
    while(optin < argc) {
        string in = argv[optin];
        if      (in == "-n" || in == "--n-listeners") { n_listeners = std::atoi(argv[++optin]); }
        else if (in == "-h" || in == "--help") { help(); return 0; }
        else {
            cout << "mpmc_test    error unknown input argument (=" << in << ") provided" << endl;
            return 1;
        }
        optin++;
    } // while
    
    if(n_listeners > 3) {
        cout << "mpmc_test    ERROR currently we only handle <=3 listerns, you requested " << n_listeners << endl;
        return 1;
    }

    string ip_address = "127.0.0.1";
    vector<string> listen_ports = { "1234", "1235", "1236" };

    using moodycamel::ConcurrentQueue;

    // this is the main collector queue that everything ends up in
    ConcurrentQueue<DataFragment*> output_queue;
    std::map< unsigned int, ConcurrentQueue<DataFragment*>* > output_l1_queue;

    // build the queues that will be used by the listeners
    vector< ConcurrentQueue<uint8_t*>* > listener_queues;
    for(size_t nl = 0; nl < n_listeners; nl++) {
        listener_queues.push_back( new ConcurrentQueue<uint8_t*>() );
    }

    // build the listeners
    std::shared_ptr<boost::asio::io_service> io_service;
    io_service = std::make_shared<boost::asio::io_service>();
    vector<DataListener*> listeners;
    for(size_t nl = 0; nl < n_listeners; nl++) {
        listeners.push_back( new DataListener(ip_address, std::stoi(listen_ports.at(nl)), io_service,
                listener_queues.at(nl), output_l1_queue) );
        io_service->post(boost::bind(&DataListener::listen, listeners.at(nl))); //listeners.at(nl), listen));
    }

    // build the builders
    auto map_cond = std::shared_ptr<std::condition_variable>(new std::condition_variable);
    auto map_mutex = std::shared_ptr<std::mutex>(new std::mutex);


    vector<DataBuilder*> builders;
    for(size_t nl = 0; nl < n_listeners; nl++) {
        builders.push_back( new DataBuilder( listener_queues.at(nl), output_l1_queue, map_cond, map_mutex) );
    }

    string flag;
    std::cin >> flag;

    cout << "Starting IO service" << endl;

    for(size_t nl = 0; nl < n_listeners; nl++) listeners.at(nl)->start();

    std::cin >> flag;
    cout << "Stoping IO service" << endl;
    io_service->stop();
    cout << "io service stopped ? " << io_service->stopped() << endl;

    for(size_t nl = 0; nl < n_listeners; nl++) {
        listeners.at(nl)->stop();
    }

    for(size_t nl = 0; nl < n_listeners; nl++) {
        builders.at(nl)->stop();
    }

    for(size_t nl = 0; nl < n_listeners; nl++) {
        delete listeners.at(nl);
    }

    for(size_t nl = 0; nl < n_listeners; nl++) {
        delete builders.at(nl);
    }

    //for(size_t nl = 0; nl < n_listeners; nl++) {
    //    delete listener_queues.at(nl);
    //}

    output_l1_queue.clear();


 //   ConcurrentQueue<int> q;
 //   int dequeued[100] = { 0 };
 //   std::thread threads[20];
 //   
 //   // Producers
 //   for (int i = 0; i != 10; ++i) {
 //   	threads[i] = std::thread([&](int i) {
 //   		for (int j = 0; j != 10; ++j) {
 //   			q.enqueue(i * 10 + j);
 //   		}
 //   	}, i);
 //   }
 //   
 //   // Consumers
 //   for (int i = 10; i != 20; ++i) {
 //   	threads[i] = std::thread([&]() {
 //   		int item;
 //   		for (int j = 0; j != 20; ++j) {
 //   			if (q.try_dequeue(item)) {
 //   				++dequeued[item];
 //   			}
 //   		}
 //   	});
 //   }
 //   
 //   // Wait for all threads
 //   for (int i = 0; i != 20; ++i) {
 //   	threads[i].join();
 //   }
 //   
 //   // Collect any leftovers (could be some if e.g. consumers finish before producers)
 //   int item;
 //   while (q.try_dequeue(item)) {
 //   	++dequeued[item];
 //   }
 //   
 //   // Make sure everything went in and came back out!
 //   for (int i = 0; i != 100; ++i) {
 //   	assert(dequeued[i] == 1);
 //   }


    return 0;
} // main
