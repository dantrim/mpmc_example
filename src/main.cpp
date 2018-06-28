// std/stl
#include <iostream>
#include <string>
#include <vector>
#include <thread>
#include <chrono>
#include <map>
#include <condition_variable>
#include <mutex>
#include <memory>
#include <atomic>
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
#include "event_builder.h"

//logging
//#include "spdlog/spdlog.h"


void help()
{
    return;
}

int main(int argc, char* argv[]) {

    // create the logger
    vector<spdlog::sink_ptr> sinks;
    auto file_sink = std::make_shared<spdlog::sinks::simple_file_sink_mt>("run.log", /*overwrite existing*/true);
    auto color_sink = std::make_shared<spdlog::sinks::ansicolor_stdout_sink_mt>();
    sinks.push_back(file_sink);
    sinks.push_back(color_sink);
    auto logger = std::make_shared<spdlog::logger>("mm_ddaq", begin(sinks), end(sinks));
    logger->set_pattern("[%D %H:%M:%S] [%t] [%^%l%$] %v");
    spdlog::register_logger(logger);

    int n_listeners = 1;

    int optin(1);
    while(optin < argc) {
        string in = argv[optin];
        if      (in == "-n" || in == "--n-listeners") { n_listeners = std::atoi(argv[++optin]); }
        else if (in == "-h" || in == "--help") { help(); return 0; }
        else {
            logger->error("unknown input argument ({}) provided", in);
            return 1;
        }
        optin++;
    } // while
    
    if(n_listeners > 3) {
        logger->error("currently we only handle <=3 listeners, you requested {}", n_listeners);
        return 1;
    }

    string ip_address = "127.0.0.1";
    vector<string> listen_ports = { "1234", "1235", "1236" };

    using moodycamel::ConcurrentQueue;

    // this is the main collector queue that everything ends up in
    std::map< unsigned int, ConcurrentQueue<DataFragment>>* output_l1_queue =
        new  std::map<unsigned int, ConcurrentQueue<DataFragment> >();

    // build the queues that will be used by the listeners
    ConcurrentQueue<DataFragment>* listener_queue = new ConcurrentQueue< DataFragment >(4096);

    // build the listeners
    std::shared_ptr<boost::asio::io_service> io_service;
    io_service = std::make_shared<boost::asio::io_service>();
    vector<DataListener*> listeners;
    std::atomic_bool listen_flag(true);

    cout << "Main listen_flag = " << &listen_flag << endl;

    for(size_t nl = 0; nl < n_listeners; nl++) {
        listeners.push_back( new DataListener(ip_address, std::stoi(listen_ports.at(nl)), io_service,
                listener_queue, std::ref(listen_flag) ));
        io_service->post(boost::bind(&DataListener::listen, listeners.at(nl))); //listeners.at(nl), listen));
    }

    // build the builders
    auto map_cond = std::shared_ptr<std::condition_variable>(new std::condition_variable);
    auto map_mutex = std::shared_ptr<std::mutex>(new std::mutex);

    vector<DataBuilder*> builders;
    std::atomic_bool build_flag(true);
    for(size_t nl = 0; nl < n_listeners; nl++) {
        builders.push_back( new DataBuilder( listener_queue,  output_l1_queue, map_cond, map_mutex, io_service, std::ref(build_flag)) );
        io_service->post(boost::bind(&DataBuilder::build, builders.at(nl)));
    }


    string flag;
    std::cin >> flag;

    logger->info("starting io services");

    for(size_t nl = 0; nl < n_listeners; nl++) listeners.at(nl)->start();
    for(size_t nl = 0; nl < n_listeners; nl++) builders.at(nl)->start();

    // build the L1 indexer
    std::shared_ptr<EventBuilder> indexer;
    std::atomic_int indexer_flag(0);
    indexer = std::make_shared<EventBuilder>( n_listeners, output_l1_queue, map_mutex, map_cond, std::ref(indexer_flag) );

    std::cin >> flag;
    logger->info("Starting indexer");
    indexer->start();

    std::cin >> flag;
    logger->info("stopping io services");
    io_service->stop();
    //logger->info("io service stopped? {0}", io_service->stopped());

    listen_flag.store(false);

    for(size_t nl = 0; nl < n_listeners; nl++) {
        listeners.at(nl)->stop();
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    build_flag.store(false);

    for(size_t nl = 0; nl < n_listeners; nl++) {
        builders.at(nl)->stop();
    }

    //logger->info("press a button to stop the indexer");
    //std::cin >> flag;

    indexer_flag.store(1);
    indexer->stop();

    //std::this_thread::sleep_for(std::chrono::milliseconds(100));
    //indexer->flush();
    logger->info("indexer stopped with {0:d} L1IDs left unprocessed", indexer->n_in_map());

    //for(size_t nl = 0; nl < n_listeners; nl++) {
    //    delete listeners.at(nl);
    //}

    //delete listener_queue;

    //for(size_t nl = 0; nl < n_listeners; nl++) {
    //    delete builders.at(nl);
    //}


    //for(size_t nl = 0; nl < n_listeners; nl++) {
    //    delete listener_queues.at(nl);
    //}

    //output_l1_queue->clear();

    logger->info("Event building stats: frac OK {0:f}\%, BAD {1:f}\% [TOTAL = {2:d}, OK = {3:d}, BAD = {4:d}, MORE = {5:f}%, LESS = {6:f}%, AMB ={7:f}%]", indexer->ok_frac() * 100., indexer->bad_frac() * 100.,  indexer->n_total(), indexer->n_ok(), indexer->n_bad(),indexer->more_frac()*100., indexer->less_frac()*100., indexer->amb_frac()*100. );


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
