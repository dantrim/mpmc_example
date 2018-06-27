#ifndef MPMC_DATA_FRAGMENT_H
#define MPMC_DATA_FRAGMENT_H

//std/stl
#include <vector>
#include <cstdint>
#include <iostream>
#include <iomanip>

class DataFragment {

    public :
        DataFragment(unsigned int l1id) :
            m_l1id(l1id) {}

        unsigned int l1id() const { return m_l1id; }

    private :
        unsigned int m_l1id;
        std::vector<unsigned char*> m_packet;


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
