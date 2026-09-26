// SPDX-License-Identifier: MIT OR Apache-2.0
#include <lasrs/lasrs.hpp>

#include <iostream>
#include <sstream>

int main()
{
    if (!las::abi_compatible())
    {
        std::cerr << "library and headers do not match\n";
        return 1;
    }
    std::stringstream stream;
    {
        las::Writer writer(stream, las::Header());
        writer.write_point(las::Point());
        writer.close();
    }
    stream.seekg(0);
    las::Reader reader(stream);
    const auto points = reader.read_all().len();
    std::cout << "read " << points << " point\n";
    return points == 1 ? 0 : 1;
}
