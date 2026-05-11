#include "gui.h"
#include <iostream>

int main(int argc, char* argv[]) {
    GUI gui;

    if (!gui.initialize()) {
        std::cerr << "Failed to initialize GUI" << std::endl;
        return 1;
    }

    gui.run();
    gui.shutdown();

    return 0;
}
