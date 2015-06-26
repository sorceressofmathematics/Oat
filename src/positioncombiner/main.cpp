//******************************************************************************
//* File:   oat posicom main.cpp
//* Author: Jon Newman <jpnewman snail mit dot edu>
//
//* Copyright (c) Jon Newman (jpnewman snail mit dot edu) 
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
//****************************************************************************

#include "OatConfig.h" // Generated by CMake

#include <csignal>
#include <unordered_map>
#include <boost/program_options.hpp>

#include "../../lib/utility/IOFormat.h"
#include "../../lib/cpptoml/cpptoml.h"

#include "PositionCombiner.h"
#include "MeanPosition.h"

namespace po = boost::program_options;

volatile sig_atomic_t quit = 0;
volatile sig_atomic_t source_eof = 0;

void printUsage(po::options_description options) {
    std::cout << "Usage: posicom [INFO]\n"
              << "   or: posicom TYPE SOURCES SINK [CONFIGURATION]\n"
              << "Combine positional information from two or more SOURCES.\n"
              << "Publish combined position to SINK.\n\n"
              << "TYPE\n"
              << "  mean: Geometric mean of SOURCE positions\n\n"
              << "SOURCES:\n"
              << "  User supplied position source names (e.g. pos1 pos2).\n\n"
              << "SINK:\n"
              << "  User supplied position sink name (e.g. pos).\n\n"
              << options << "\n";
}

// Signal handler to ensure shared resources are cleaned on exit due to ctrl-c
void sigHandler(int s) {
    quit = 1;
}

void run(PositionCombiner* combiner) {
    
    while (!quit && !source_eof) { 
        
        source_eof = combiner->process(); 
    }
}

int main(int argc, char *argv[]) {
    
    std::signal(SIGINT, sigHandler);

    std::vector<std::string> sources;
    std::string sink;
    std::string type;
    std::string config_file;
    std::string config_key;
    bool config_used = false;
    po::options_description visible_options("OPTIONS");
    
    
    std::unordered_map<std::string, char> type_hash;
    type_hash["mean"] = 'a';

    try {

        po::options_description options("INFO");
        options.add_options()
                ("help", "Produce help message.")
                ("version,v", "Print version information.")
                ;
        
        po::options_description config("CONFIGURATION");
        config.add_options()
                ("config-file,c", po::value<std::string>(&config_file), "Configuration file.")
                ("config-key,k", po::value<std::string>(&config_key), "Configuration key.")
                ;
        po::options_description hidden("HIDDEN OPTIONS");
        hidden.add_options()
                ("type", po::value<std::string>(&type), 
                "Type of test position combiner to use.")
                ("sources", po::value< std::vector<std::string> >(),
                "The names the SOURCES supplying the Position2D objects to be combined.")
                ("sink", po::value<std::string>(&sink),
                "The name of the SINK to which combined position Position2D objects will be published.")
                ;
        
        po::positional_options_description positional_options;
        positional_options.add("type", 1);
        positional_options.add("sources", -1); // If not overridden by explicit --sink, last positional argument is sink.

        po::options_description all_options("OPTIONS");
        all_options.add(options).add(config).add(hidden);
        
        visible_options.add(options).add(config);

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
            std::cout << "Oat Position Combiner version "
                      << Oat_VERSION_MAJOR
                      << "." 
                      << Oat_VERSION_MINOR 
                      << "\n";
            std::cout << "Written by Jonathan P. Newman in the MWL@MIT.\n";
            std::cout << "Licensed under the GPL3.0.\n";
            return 0;
        }
        
        if (!variable_map.count("type")) {
            printUsage(visible_options);
            std::cerr << oat::Error("A TYPE must be specified.\n");
            return -1;
        }
        
        if (!variable_map.count("sources")) {
            printUsage(visible_options);
            std::cerr << oat::Error("At least two SOURCES and a SINK must be specified.\n");
            return -1;
        }
        
        sources = variable_map["sources"].as< std::vector<std::string> >();
        if (sources.size() < 3) {
            printUsage(visible_options);
            std::cerr << oat::Error("At least two SOURCES and a SINK must be specified.\n");
            return -1;
        }
        
        if (!variable_map.count("sink")) {
            
            // If not overridden by explicit --sink, last positional argument is the sink.
            sink = sources.back();
            sources.pop_back(); 
        }
        
        if ((variable_map.count("config-file") && !variable_map.count("config-key")) ||
                (!variable_map.count("config-file") && variable_map.count("config-key"))) {
            printUsage(visible_options);
            std::cerr << oat::Error("A configuration file must be supplied with a corresponding config-key.\n");
            return -1;
        } else if (variable_map.count("config-file")) {
            config_used = true;
        }

    } catch (std::exception& e) {
        std::cerr << oat::Error(e.what()) << "\n";
        return -1;
    } catch (...) {
        std::cerr << oat::Error("Exception of unknown type.\n");
        return -1;
    }

    PositionCombiner* combiner; //(ant_source, pos_source, sink);
    switch (type_hash[type]) {
        case 'a':
        {
            combiner = new MeanPosition(sources, sink);
            break;
        }
        default:
        {
            printUsage(visible_options);
            std::cerr << oat::Error("Invalid TYPE specified.\n");
            return -1;
        }
    }
    
    // At this point, the new'ed component exists and must be deleted in the case
    // any exception
    try { 

        if (config_used)
            combiner->configure(config_file, config_key);

        // Tell user
        std::cout << oat::whoMessage(combiner->get_name(), "Listening to sources ");
        for (auto s : sources)
            std::cout << oat::sourceText(s) << " ";
        std::cout << ".\n"
                << oat::whoMessage(combiner->get_name(),
                "Steaming to sink " + oat::sinkText(sink) + ".\n")
                << oat::whoMessage(combiner->get_name(),
                "Press CTRL+C to exit.\n");

        // Infinite loop until ctrl-c or end of stream signal
        run(combiner);

        // Tell user
        std::cout << oat::whoMessage(combiner->get_name(), "Exiting.\n");

        // Free heap memory allocated to combiner
        delete combiner;

        // Exit
        return 0;

    } catch (const cpptoml::parse_exception& ex) {
        std::cerr << oat::whoError(combiner->get_name(), "Failed to parse configuration file " + config_file + "\n")
                  << oat::whoError(combiner->get_name(), ex.what())
                  << "\n";
    } catch (const std::runtime_error ex) {
        std::cerr << oat::whoError(combiner->get_name(),ex.what())
                  << "\n";
    } catch (const cv::Exception ex) {
        std::cerr << oat::whoError(combiner->get_name(), ex.msg)
                  << "\n";
    } catch (...) {
        std::cerr << oat::whoError(combiner->get_name(), "Unknown exception.\n");
    }
    
    // Free heap memory allocated to filter
    delete combiner;

    // Exit failure
    return -1;
}


