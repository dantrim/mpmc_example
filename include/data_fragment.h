#ifndef MPMC_DATA_FRAGMENT_H
#define MPMC_DATA_FRAGMENT_H

//std/stl
#include <vector>
#include <cstdint>
#include <iostream>
#include <iomanip>

class DataFragment {

    public :
        DataFragment() {
            m_init_time = std::chrono::system_clock::now();
            clear();
        };

        void set_l1id(uint32_t l1) { m_l1id = l1; }
        void set_link(int32_t link) { m_link = link; }

        uint32_t l1id() const { return m_l1id; }
        int32_t link_id() const { return m_link; }

        std::vector<uint32_t> m_packet;
        void clear() {
            m_l1id = 0x0;
            m_packet.clear();
            m_link = -1;
        }

        void update_time_stamp() {
            m_init_time = std::chrono::system_clock::now();
        }
        const std::chrono::time_point<std::chrono::system_clock> & init_time() { return m_init_time; }

        // timestamp of when this object was initialized
        
    private :
        uint32_t m_l1id;
        int32_t m_link;

        std::chrono::time_point<std::chrono::system_clock> m_init_time;


};
inline std::ostream& operator <<(std::ostream& stream,
            const DataFragment& fragment) {

    auto old_fill = stream.fill('0');
    auto old_flags = stream.flags();

    stream << "DataFragment    l1Id: " << std::setw(8) << fragment.l1id() << "\n";

    stream.flags(old_flags);
    stream.fill(old_fill);
    return stream;

} // << operator overload


#endif
