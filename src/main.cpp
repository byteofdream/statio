#include "statio/system_info.hpp"

#include <exception>
#include <iostream>

int main() {
    try {
        statio::SystemSnapshot snapshot = statio::collectSystemSnapshot();
        std::cout << statio::renderReport(snapshot);
    } catch (const std::exception& e) {
        std::cerr << "statio error: " << e.what() << '\n';
        return 1;
    }

    return 0;
}
