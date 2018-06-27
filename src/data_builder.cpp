#include "data_builder.h"

//std/stl
#include <sstream>
#include <iostream>
#include <string>
using namespace std;

DataBuilder::DataBuilder(moodycamel::ConcurrentQueue<uint8_t*>* input_queue,
        std::map<unsigned int, moodycamel::ConcurrentQueue<DataFragment*>*> & output_queue,
        std::shared_ptr<std::condition_variable> map_condition,
        std::shared_ptr<std::mutex> map_mutex) :
            m_in_queue(input_queue),
            m_out_queue(output_queue),
            m_active(true)
{

    m_map_cond = map_condition;
    m_map_mutex = map_mutex;

    m_thread = std::thread( [this] () {
        build();
    });
}

void DataBuilder::build()
{
    cout << "DataBuilder::build    [" << std::this_thread::get_id() << "] building starts" << endl;

    uint8_t* data_in = nullptr;

    uint32_t current_l1id = 0;
    bool is_start = true;

    while(m_active) {

        //m_map_cond->notify_all();
        if(m_in_queue->try_dequeue(data_in)) {
            //m_map_cond->notify_all();

//            cout << "DataBuilder::build    [" << std::this_thread::get_id() << "]   receive: " << std::hex << (unsigned)data_in[0] << std::dec << "(queue size = " << m_in_queue->size_approx() << ")" <<   endl;

            unsigned int l1id = data_in[0];
            DataFragment* frag = new DataFragment(l1id);
            if(!is_start) {
                if(l1id != (current_l1id + 1)) {
                    cout << "DataBuilder::build    [" << std::this_thread::get_id() << "]    L1ID out of sync (expected = " << std::hex << (current_l1id + 1) << ", got = " << l1id << ")" << endl;
                }
            }

            current_l1id = l1id;

//            std::unique_lock<std::mutex> lock(*m_map_mutex);
            std::lock_guard<std::mutex> lock(*m_map_mutex);
 //           bool have_map = !(m_map_cond->wait_for(lock, std::chrono::milliseconds(1000))==std::cv_status::timeout);
//            if(!have_map) {
            if(false) {
                cout << "DataBuilder::build    [" << std::this_thread::get_id() << "] failed to access L1 queue!" << endl;
            }
            else {
                if(m_out_queue.count(l1id)) {
                    m_out_queue[l1id]->enqueue(frag);
                    //cout << "DataBuilder::build    L1Queue size for L1ID " << std::hex << (unsigned)l1id << " : " << m_out_queue[l1id]->size_approx() << endl;
                }
                else {
                    m_out_queue[l1id] = new moodycamel::ConcurrentQueue<DataFragment*>();
                    m_out_queue[l1id]->enqueue(frag);
                    //cout << "DataBuilder::build    L1Queue size for L1ID " << std::hex << (unsigned)l1id << " : " << m_out_queue[l1id]->size_approx() << endl;
                }
                //m_map_cond->notify_one();
            }
            //lock.unlock();
        }
    } // while active

}

void DataBuilder::stop()
{
    m_active = false;
    if(m_thread.joinable()) {
        m_thread.join();
    }
}

