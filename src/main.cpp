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
//#include "concurrentqueue/concurrentqueue.h"
//using moodycamel::ConcurrentQueue;

// mpmc
#include "data_listener.h"
#include "data_builder.h"
#include "data_fragment.h"
#include "event_builder.h"

//logging
//#include "spdlog/spdlog.h"

// concurrent_maps
#include "map_defs.h"

void help()
{
    cout << "options:" << endl;
    cout << " -n|--n-listeners        number of listeners [default: 1]" << endl;
    cout << " -m|--map-test           run the test map functions instead of the listeners" << endl;
    cout << " -r|--n-rec              number of events to receive/expect, and exit after [default:-1 (no limit)]" << endl;
    cout << " -h|--help               print this help message" << endl;
    return;
}

void test_map()
{
    auto logger = spdlog::get("mm_ddaq");
    logger->info("testing map");

    typedef junction::ConcurrentMap_Grampa<turf::u32, turf::u32 > ConcurrentMap;
    ConcurrentMap l1map;

    for(int i = 0; i < 10; i++) {
        uint32_t init = l1map.get(i);
        l1map.assign(i, i*2);
        uint32_t post = l1map.get(i);
        uint32_t post_post = l1map.assign(i, i*3);
        uint32_t final_val = l1map.get(i);
        l1map.erase(i);
        uint32_t post_erase = l1map.get(i);
        logger->info("l1map init = {0:d}, post = {1:d}, post post = {2:d}, final = {3:d}, post_erase = {4:d}", init, post, post_post, final_val, post_erase);
    }
}

void test_map_cuckoo()
{
    auto logger = spdlog::get("mm_ddaq");
    logger->info("test cuckoo map");
    typedef cuckoohash_map<uint32_t, uint32_t> testMap;

    testMap l1hash;
    logger->info("init hash size = {0:d}", l1hash.size());
    logger->info("init hash cap  = {0:d}", l1hash.capacity());

    l1hash.insert(0, 42);
    l1hash.insert(1, 42);
    l1hash.insert(3, 42);
    logger->info("hash size = {0:d}", l1hash.size());
    return;
    l1hash.insert(4, 9);
    logger->info("find 0? {}", l1hash.find(0));
    l1hash.erase(0);
    logger->info("find 0 post erase? {}", l1hash.contains(0));
    bool yup = l1hash.insert_or_assign(0, 99);
    if(yup) {
        uint32_t val = l1hash.find(0);
        logger->info("after insert_or_assign  yup = {}, val at 0 = {}", yup,  val);
    }
    uint32_t val = 0;
    if(l1hash.find(0, val)) {
        logger->info("after find(0,33) val at 0 = {}", val);
    }

    l1hash.insert(33, 8);
    l1hash.insert(99999, 2);
    //l1hash.insert(4999999, 99);
    l1hash.insert(1, 2);
    l1hash.insert(2, 4);
    l1hash.insert(3, 9);

    {
        auto lt = l1hash.lock_table();
        for(const auto & it : lt) {
            logger->info("iterating over l1hash : (key,val)=({0:x},{1:d})", it.first, it.second);
        }
    }
    

    //typedef cuckoohash_map<uint32_t, moodycamel::ConcurrentQueue<uint32>*> QueueHash;
    //QueueHash qHash;
    //uint32_t l1id = 4;
    //uint32_t data_to_add = 72;
    //auto updatefn = [] (uint32_t & v) { ++v; };
    //logger->info("l1hash at four before updatefn= {}", l1hash.find(4));
    //l1hash.upsert(4, updatefn, 7);
    //logger->info("l1hash at four after updatefn = {}", l1hash.find(4));
    //l1hash.erase(4);
    //l1hash.upsert(4, updatefn, 7);
    //logger->info("l1hash at four after updatefn & erase = {}", l1hash.find(4));

    typedef cuckoohash_map<uint32_t, moodycamel::ConcurrentQueue<uint32_t>*> QueueHash;
    QueueHash qHash;

    moodycamel::ConcurrentQueue<uint32_t>* qval = nullptr;
    if(qHash.find(4, qval)) {
        cout << "qHash has key 4 with val = " << val << endl;
    }
    else logger->info("qHash does not have key 4");
    //logger->info("qHash at four before update fn = {}", qHash.find(4));
    auto updatel1 = [] (moodycamel::ConcurrentQueue<uint32_t>* qq) { qq->try_enqueue(82); };
    qHash.upsert(4, updatel1, new moodycamel::ConcurrentQueue<uint32_t>());
    qHash.upsert(4, updatel1, new moodycamel::ConcurrentQueue<uint32_t>());
    qHash.upsert(4, updatel1, new moodycamel::ConcurrentQueue<uint32_t>());
    qHash.upsert(4, updatel1, new moodycamel::ConcurrentQueue<uint32_t>());
    if(qHash.find(4, qval)) {
        cout << "qHash has key 4 with val = " << qval->size_approx() << endl;
        uint32_t frag;
        int n_dq = 0;        
        while(qval->try_dequeue(frag)) {
            n_dq++;
            logger->info("qHash vals in q for L1ID 4: {0:d} = {1:d}", n_dq, frag);
        }
        cout << "qHash size after dequeue'ing = " << qval->size_approx() << endl;
    }
    else logger->info("qHash does not have key 4");
    

    
    



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
    int n_to_rec = -1;

    int optin(1);
    while(optin < argc) {
        string in = argv[optin];
        if      (in == "-n" || in == "--n-listeners") { n_listeners = std::atoi(argv[++optin]); }
        else if (in == "-m" || in == "--map-test") { test_map_cuckoo(); return 0; }
        else if (in == "-r" || in == "--n-rec") { n_to_rec = std::stoi(argv[++optin]); }
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

    //typedef junction::ConcurrentMap_Grampa<turf::u32, ConcurrentQueue<DataFragment>* > L1IndexMap;
    //L1IndexMap* l1_index = new L1IndexMap();
    L1IndexHash l1_hash(4092); // = new L1IndexHash();

    // this is the main collector queue that everything ends up in
    //std::map< unsigned int, ConcurrentQueue<DataFragment>>* output_l1_queue =
    //    new  std::map<unsigned int, ConcurrentQueue<DataFragment> >();

    // build the queues that will be used by the listeners
    //ConcurrentQueue<DataFragment*>* listener_queue = new ConcurrentQueue< DataFragment* >(1023);
    vector<ConcurrentQueue<DataFragment*>*> listener_queues;
    for(int i = 0; i < 3; i++) {
        listener_queues.push_back( new ConcurrentQueue<DataFragment*>(1023) );
    }

    // build the listeners
    std::shared_ptr<boost::asio::io_service> io_service;
    io_service = std::make_shared<boost::asio::io_service>();
    vector<DataListener*> listeners;
    std::atomic_bool listen_flag(true);

    cout << "Main listen_flag = " << &listen_flag << endl;

    for(size_t nl = 0; nl < n_listeners; nl++) {
        listeners.push_back( new DataListener(ip_address, std::stoi(listen_ports.at(nl)), io_service,
                listener_queues.at(nl), std::ref(listen_flag) ));
        io_service->post(boost::bind(&DataListener::listen, listeners.at(nl))); //listeners.at(nl), listen));
    }

    // build the builders
    auto map_cond = std::shared_ptr<std::condition_variable>(new std::condition_variable);
    auto map_mutex = std::shared_ptr<std::mutex>(new std::mutex);

    vector<DataBuilder*> builders;
    std::atomic_bool build_flag(true);

    for(size_t nl = 0; nl < n_listeners; nl++) {
        builders.push_back( new DataBuilder( listener_queues.at(nl), std::ref(l1_hash), map_cond, map_mutex, io_service, std::ref(build_flag)) );
        //builders.push_back( new DataBuilder( listener_queue,  output_l1_queue, map_cond, map_mutex, io_service, std::ref(build_flag)) );
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
    indexer = std::make_shared<EventBuilder>( n_listeners, std::ref(l1_hash), map_mutex, map_cond, std::ref(indexer_flag), n_to_rec );
    //indexer = std::make_shared<EventBuilder>( n_listeners, output_l1_queue, map_mutex, map_cond, std::ref(indexer_flag) );

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

    //std::this_thread::sleep_for(std::chrono::milliseconds(10));
    build_flag.store(false);

    for(size_t nl = 0; nl < n_listeners; nl++) {
        builders.at(nl)->stop();
    }

    //indexer_flag.store(1);
    indexer->stop();

    //std::this_thread::sleep_for(std::chrono::milliseconds(100));
    //indexer->flush();
    logger->info("indexer stopped with {0:d} L1IDs left unprocessed", indexer->n_in_map());

    //for(size_t nl = 0; nl < n_listeners; nl++) {
    //    delete listeners.at(nl);
    //}


    //for(size_t nl = 0; nl < n_listeners; nl++) {
    //    delete builders.at(nl);
    //}



    //output_l1_queue->clear();

    logger->info("Event building stats: frac OK {0:f}\%, BAD {1:f}\% [TOTAL = {2:d}, OK = {3:d}, BAD = {4:d}, MORE = {5:f}%, LESS = {6:f}%, AMB ={7:f}%]", indexer->ok_frac() * 100., indexer->bad_frac() * 100.,  indexer->n_total(), indexer->n_ok(), indexer->n_bad(),indexer->more_frac()*100., indexer->less_frac()*100., indexer->amb_frac()*100. );

    return 0;
} // main
