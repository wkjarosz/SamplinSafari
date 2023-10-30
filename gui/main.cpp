/*
    main.cpp -- Samplin' Safari application entry point

    All rights reserved. Use of this source code is governed by a
    BSD-style license that can be found in the LICENSE.txt file.
*/

#include "SampleViewer.h"
#include <cstdlib>
#include <iostream>
#include <thread>

/* Force usage of discrete GPU on laptops */
NANOGUI_FORCE_DISCRETE_GPU();

int nprocs = -1;

int main(int argc, char **argv)
{
    vector<string> args;
    bool           help  = false;
    bool           error = false;

#if defined(__APPLE__)
    bool launched_from_finder = false;
#endif

    try
    {
        for (int i = 1; i < argc; ++i)
        {
            if (strcmp("--help", argv[i]) == 0 || strcmp("-h", argv[i]) == 0)
            {
                help = true;
#if defined(__APPLE__)
            }
            else if (strncmp("-psn", argv[i], 4) == 0)
            {
                launched_from_finder = true;
#endif
            }
            else
            {
                if (strncmp(argv[i], "-", 1) == 0)
                {
                    cerr << "Invalid argument: \"" << argv[i] << "\"!" << endl;
                    help  = true;
                    error = true;
                }
                args.push_back(argv[i]);
            }
        }
    }
    catch (const std::exception &e)
    {
        cout << "Error: " << e.what() << endl;
        help  = true;
        error = true;
    }

    if (help)
    {
        cout << "Syntax: " << argv[0] << endl;
        cout << "Options:" << endl;
        cout << "   -h, --help                Display this message" << endl;
        return error ? EXIT_FAILURE : EXIT_SUCCESS;
    }

    try
    {
        nanogui::init();

#if defined(__APPLE__)
        if (launched_from_finder)
            nanogui::chdir_to_bundle_parent();
#endif

        {
            nanogui::ref<SampleViewer> viewer = new SampleViewer();
            viewer->set_visible(true);
            nanogui::mainloop();
        }

        nanogui::shutdown();
    }
    catch (const std::runtime_error &e)
    {
        std::cerr << "Caught a fatal error: " << e.what() << endl;
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
