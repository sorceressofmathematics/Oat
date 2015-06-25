//******************************************************************************
//* File:   oat posidet main.cpp
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

#include <csignal>
#include <unordered_map>
#include <boost/program_options.hpp>

#include "../../lib/utility/IOFormat.h"
#include "../../lib/cpptoml/cpptoml.h"

#include "Detector2D.h"
#include "HSVDetector2D.h"
#include "DifferenceDetector2D.h"

namespace po = boost::program_options;

volatile sig_atomic_t quit = 0;
volatile sig_atomic_t source_eof = 0;

void printUsage(po::options_description options) {
    std::cout << "Usage: posidet [INFO]\n"
            << "   or: posidet TYPE SOURCE SINK [CONFIGURATION]\n"
            << "Perform TYPE object detection on frames from SOURCE.\n"
            << "Publish detected object positions to SINK.\n\n"
            << "TYPE\n"
            << "  diff: Difference detector (grey-scale, motion)\n"
            << "  hsv : HSV detector (color)\n\n"
            << "SOURCE:\n"
            << "  User supplied source name (e.g. raw).\n\n"
            << "SINK:\n"
            << "  User supplied sink name (e.g. pos).\n\n"
            << options << "\n";
}

// Signal handler to ensure shared resources are cleaned on exit due to ctrl-c

void sigHandler(int s) {
    quit = 1;
}

void run(Detector2D* detector) {

    while (!quit && !source_eof) {
        source_eof = detector->process();
    }
}

int main(int argc, char *argv[]) {

    std::signal(SIGINT, sigHandler);

    std::string source;
    std::string sink;
    std::string type;
    std::string config_file;
    std::string config_key;
    bool config_used = false;
    po::options_description visible_options("OPTIONS");

    std::unordered_map<std::string, char> type_hash;
    type_hash["diff"] = 'a';
    type_hash["hsv"] = 'b';

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
                ("type,t", po::value<std::string>(&type), "Detector type.\n\n"
                "Values:\n"
                "  diff: Difference detector (motion).\n"
                "  hsv: HSV detector (color).")
                ("source", po::value<std::string>(&source),
                "The name of the SOURCE that supplies images on which hsv-filter object detection will be performed."
                "The server must be of type SMServer<SharedCVMatHeader>\n")
                ("sink", po::value<std::string>(&sink),
                "The name of the SINK to which position background subtracted images will be served.")
                ;

        po::positional_options_description positional_options;
        positional_options.add("type", 1);
        positional_options.add("source", 1);
        positional_options.add("sink", 1);

        po::options_description visible_options("OPTIONS");
        visible_options.add(options).add(config);

        po::options_description all_options("ALL OPTIONS");
        all_options.add(options).add(config).add(hidden);

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
            std::cout << "Oat Object Position Detector version 1.0\n"; //TODO: Cmake managed versioning
            std::cout << "Written by Jonathan P. Newman in the MWL@MIT.\n";
            std::cout << "Licensed under the GPL3.0.\n";
            return 0;
        }

        if (!variable_map.count("type")) {
            printUsage(visible_options);
            std::cerr << oat::Error("A TYPE must be specified.\n");
            return -1;
        }

        if (!variable_map.count("source")) {
            printUsage(visible_options);
            std::cerr << oat::Error("A SOURCE must be specified.\n");
            return -1;
        }

        if (!variable_map.count("sink")) {
            printUsage(visible_options);
            std::cerr << oat::Error("A SINK name must be specified.\n");
            return -1;
        }

        if ((variable_map.count("config-file") && !variable_map.count("config-key")) ||
                (!variable_map.count("config-file") && variable_map.count("config-key"))) {
            printUsage(visible_options);
            std::cerr << oat::Error("A config file must be supplied with a corresponding config-key.\n");
            return -1;
        } else if (variable_map.count("config-file")) {
            config_used = true;
        }

    } catch (std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return -1;
    } catch (...) {
        std::cerr << "Exception of unknown type." << std::endl;
        return -1;
    }

    // Create the specified TYPE of detector
    Detector2D* detector;
    switch (type_hash[type]) {
        case 'a':
        {
            detector = new DifferenceDetector2D(source, sink);
            break;
        }
        case 'b':
        {
            detector = new HSVDetector2D(source, sink);
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
            detector->configure(config_file, config_key);

        // Tell user
        std::cout << oat::whoMessage(detector->get_name(),
                "Listening to source " + oat::sourceText(source) + ".\n")
                << oat::whoMessage(detector->get_name(),
                "Steaming to sink " + oat::sinkText(sink) + ".\n")
                << oat::whoMessage(detector->get_name(),
                "Press CTRL+C to exit.\n");


        // Infinite loop until ctrl-c or end of stream signal
        run(detector);

        // Tell user
        std::cout << oat::whoMessage(detector->get_name(), "Exiting.\n");

        // Free heap memory allocated to detector 
        delete detector;

        // Exit
        return 0;

    } catch (const cpptoml::parse_exception& ex) {
        std::cerr << oat::whoError(detector->get_name(), "Failed to parse configuration file " + config_file + "\n")
                << oat::whoError(detector->get_name(), ex.what())
                << "\n";
    } catch (const std::runtime_error ex) {
        std::cerr << oat::whoError(detector->get_name(), ex.what())
                << "\n";
    } catch (const cv::Exception ex) {
        std::cerr << oat::whoError(detector->get_name(), ex.msg)
                << "\n";
    } catch (...) {
        std::cerr << oat::whoError(detector->get_name(), "Unknown exception.\n");
    }

    // Free heap memory allocated to filter
    delete detector;

    // Exit failure
    return -1;
}


