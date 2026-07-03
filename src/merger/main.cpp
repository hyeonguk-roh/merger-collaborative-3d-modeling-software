#include <iostream>
#include "merger.h"

const int WIN_WIDTH = 1920;
const int WIN_HEIGHT = 1080;

int main(int argc, char** argv)
{

    bool start_server = true;
    if (argc > 1 && (strcmp(argv[1], "-no-server") == 0)) start_server = false;

    Merger::App app;
    if (!app.init(WIN_WIDTH, WIN_HEIGHT, "Merger", start_server))
    {
        exit(EXIT_FAILURE);
    }
    app.run();
    app.deinit();
}