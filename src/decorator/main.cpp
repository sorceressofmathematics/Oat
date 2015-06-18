//******************************************************************************
//* Copyright (c) Jon Newman (jpnewman at mit snail edu) 
//* All right reserved.
//* This file is part of the Simple Tracker project.
//* This is free software: you can redistribute it and/or modify
//* it under the terms of the GNU General Public License as published by
//* the Free Software Foundation, either version 3 of the License, or
//* (at your option) any later version.
//* This software is distributed in the hope that it will be useful,
//* but WITHOUT ANY WARRANTY; without even the implied warranty of
//* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//* GNU General Public License for more details.
//* You should have received a copy of the GNU General Public License
//* along with this source code.  If not, see <http://www.gnu.org/licenses/>.
//******************************************************************************

#include "Decorator.h"

#include <string>
#include <csignal>
#include <boost/program_options.hpp>

namespace po = boost::program_options;

volatile sig_atomic_t quit = 0;
volatile sig_atomic_t source_eof = 0;

void run(Decorator* decorator) {

    while (!quit && !source_eof) {
        source_eof = decorator->decorateFrame();
    }
}

void printUsage(po::options_description options) {
    std::cout << "Usage: decorate [INFO]\n"
            << "   or: decorate SOURCE SINK [CONFIGURATION]\n"
            << "Decorate the frames from SOURCE with, e.g., object positions and sample number.\n"
            << "Publish decorated frames to SINK.\n\n"
            << "SOURCE:\n"
            << "  User supplied frame source name (e.g. raw).\n\n"
            << "SINK:\n"
            << "  User supplied frame sink name (e.g. dec).\n\n"
            << options << "\n";
}

// Signal handler to ensure shared resources are cleaned on exit due to ctrl-c
void sigHandler(int s) {
    quit = 1;
}

int main(int argc, char *argv[]) {
    
    std::signal(SIGINT, sigHandler);

    // The image source to which the viewer will be attached
    std::vector<std::string> position_sources;
    std::string frame_source;
    std::string frame_sink;
    bool print_timestamp = false;
    bool print_sample_number = false;
    bool encode_sample_number = false;
    po::options_description visible_options("OPTIONS");

    try {

        po::options_description options("INFO");
        options.add_options()
                ("help", "Produce help message.")
                ("version,v", "Print version information.")
                ;

        po::options_description configuration("CONFIGURATION");
        configuration.add_options()
                ("positionsources,p", po::value< std::vector<std::string> >()->multitoken(),
                "The name of the server(s) that supply object position information."
                "The server(s) must be of type SMServer<Position>\n")
                ("timestamp,t", "Write the current date and time on each frame.\n")
                ("sample,s", "Write the frame sample number on each frame.\n")
                ("samplecode,S", "Write the binary encoded sample on each frame.\n")
                ;

        po::options_description hidden("POSITIONAL OPTIONS");
        hidden.add_options() ("framesource", po::value<std::string>(&frame_source),
                "The name of the server that supplies images to decorate."
                "The server must be of type SMServer<SharedCVMatHeader>\n")
                ("framesink", po::value<std::string>(&frame_sink),
                "The name of the sink to which decorated images will be published."
                "The server must be of type SMServer<SharedCVMatHeader>\n")
                ;

        po::positional_options_description positional_options;

        // If not overridden by explicit --imagesource or --sink, 
        // last positional arguments are imagesource and sink in that order
        positional_options.add("framesource", 1);
        positional_options.add("framesink", 1);

        po::options_description visible_options("OPTIONS");
        visible_options.add(options).add(configuration);

        po::options_description all_options("ALL OPTIONS");
        all_options.add(visible_options).add(hidden);

        po::variables_map variable_map;
        po::store(po::command_line_parser(argc, argv)
                .options(all_options)
                .positional(positional_options)
                .run(),
                variable_map);
        po::notify(variable_map);

        // Use the parsed options
        if (variable_map.count("help")) {
            printUsage(visible_options);
            return 0;
        }

        if (variable_map.count("version")) {
            std::cout << "Simple-Tracker Decorator, version 1.0\n"; //TODO: Cmake managed versioning
            std::cout << "Written by Jonathan P. Newman in the MWL@MIT.\n";
            std::cout << "Licensed under the GPL3.0.\n";
            return 0;
        }

        if (!variable_map.count("framesource")) {
            printUsage(visible_options);
            std::cout << "Error: at least a single FRAME_SOURCE must be specified. Exiting.\n";
            return -1;
        }

        if (!variable_map.count("framesink")) {
            printUsage(visible_options);
            std::cout << "Error: at least a single FRAME_SINK must be specified. Exiting.\n";
            return -1;
        }
        
        if (variable_map.count("positionsources")) {
            position_sources = variable_map["positionsources"].as< std::vector<std::string> >();
        }

        if (variable_map.count("timestamp")) {
            print_timestamp = true;
        }

        if (variable_map.count("sample")) {
            print_sample_number = true;
        }

        if (variable_map.count("samplecode")) {
            encode_sample_number = true;
        }

    } catch (std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    } catch (...) {
        std::cerr << "Exception of unknown type! " << std::endl;
    }

    // Make the decorator
    Decorator decorator(position_sources, frame_source, frame_sink);
    decorator.set_print_timestamp(print_timestamp);
    decorator.set_print_sample_number(print_sample_number);
    decorator.set_encode_sample_number(encode_sample_number);
    
     // Tell user
    std::cout << oat::whoMessage(decorator.get_name(),
            "Listening to source " + oat::sourceText(frame_source) + ".\n")
            << oat::whoMessage(decorator.get_name(),
            "Steaming to sink " + oat::sinkText(frame_sink) + ".\n")
            << oat::whoMessage(decorator.get_name(),
            "Press CTRL+C to exit.\n");

    run(&decorator);
    
    // Tell user
    std::cout << oat::whoMessage(decorator.get_name(), "Exiting.\n");

    // Exit
    return 0;
}
