//******************************************************************************
//* File:   DifferenceDetector.cpp
//* Author: Jon Newman <jpnewman snail mit dot edu>
//*
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


#include "DifferenceDetector.h"

#include <string>
#include <opencv2/opencv.hpp>

#include "../../lib/datatypes/Position2D.h"
#include "../../lib/cpptoml/cpptoml.h"

DifferenceDetector2D::DifferenceDetector2D(const std::string& image_source_name, const std::string& position_sink_name) :
Detector2D(image_source_name, position_sink_name)
, tuning_image_title(position_sink_name + "_tuning")
, tuning_windows_created(false)
, last_image_set(false)
, tuning_on(false) {

    // Cannot use initializer because if this is set to 0, erode_on or 
    // dilate_on must be set to false
    set_blur_size(2);
}

oat::Position2D DifferenceDetector2D::detectPosition(cv::Mat& frame) {

    this_image = frame;
    applyThreshold();
    siftBlobs();
    tune();
    
    return object_position;
}

void DifferenceDetector2D::configure(const std::string& config_file, const std::string& config_key) {

    // This will throw cpptoml::parse_exception if a file 
    // with invalid TOML is provided
    cpptoml::table config;
    config = cpptoml::parse_file(config_file);

    // See if a camera configuration was provided
    if (config.contains(config_key)) {

        auto this_config = *config.get_table(config_key);

        if (this_config.contains("blur")) {
            set_blur_size(*this_config.get_as<int64_t>("blur"));

            if (blur_size.height < 0 || !this_config.get("blur")->is_value())
                throw (std::runtime_error(
                    "blur value in " + config_key + 
                    " in" + config_file + " must be > 0.")
                    );
        }

        if (this_config.contains("diff_threshold")) {
            difference_intensity_threshold = *this_config.get_as<int64_t>("diff_threshold");
            
            if (difference_intensity_threshold < 0)
                throw (std::runtime_error(
                    "diff_threshold value in " + config_key + 
                    " in" + config_file + " must be > 0.")
                    );
        }

        if (this_config.contains("tune")) {
            if (*this_config.get_as<bool>("tune")) {
                tuning_on = true;
                createTuningWindows();
            }
        }

    } else {
        throw ( std::runtime_error(
                "No configuration named " + config_key +
                " was provided in the configuration file " + config_file)
                );
    }

}

void DifferenceDetector2D::siftBlobs() {

    cv::Mat thresh_cpy = threshold_image.clone();
    std::vector< std::vector < cv::Point > > contours;
    std::vector< cv::Vec4i > hierarchy;
    cv::Rect objectBoundingRectangle;

    //these two vectors needed for output of findContours
    //find contours of filtered image using openCV findContours function
    //findContours(temp,contours,hierarchy,CV_RETR_CCOMP,CV_CHAIN_APPROX_SIMPLE );// retrieves all contours
    cv::findContours(thresh_cpy, contours, hierarchy, cv::RETR_EXTERNAL, cv::CHAIN_APPROX_SIMPLE); // retrieves external contours

    //if contours vector is not empty, we have found some objects
    if (contours.size() > 0) {
        object_position.position_valid = true;
    } else
        object_position.position_valid = false;

    if (object_position.position_valid) {

        //the largest contour is found at the end of the contours vector
        //we will simply assume that the biggest contour is the object we are looking for.
        std::vector< std::vector<cv::Point> > largestContourVec;
        largestContourVec.push_back(contours.at(contours.size() - 1));

        //make a bounding rectangle around the largest contour then find its centroid
        //this will be the object's final estimated position.
        objectBoundingRectangle = cv::boundingRect(largestContourVec.at(0));
        object_position.position.x = objectBoundingRectangle.x + 0.5 * objectBoundingRectangle.width;
        object_position.position.y = objectBoundingRectangle.y + 0.5 * objectBoundingRectangle.height;
    }

    if (tuning_on) {

        std::string msg = cv::format("Object not found"); // TODO: This default msg will not show up. I have no idea why.

        // Plot a circle representing found object
        if (object_position.position_valid) {
            cv::cvtColor(threshold_image, threshold_image, cv::COLOR_GRAY2BGR);
            cv::rectangle(threshold_image, objectBoundingRectangle.tl(), objectBoundingRectangle.br(), cv::Scalar(0, 0, 255), 2);
            msg = cv::format("(%d, %d) pixels", (int) object_position.position.x, (int) object_position.position.y);

        }

        int baseline = 0;
        cv::Size textSize = cv::getTextSize(msg, 1, 1, 1, &baseline);
        cv::Point text_origin(
                threshold_image.cols - textSize.width - 10,
                threshold_image.rows - 2 * baseline - 10);

        cv::putText(threshold_image, msg, text_origin, 1, 1, cv::Scalar(0, 255, 0));
    }
}

void DifferenceDetector2D::applyThreshold() {

    if (last_image_set) {
        cv::cvtColor(this_image, this_image, cv::COLOR_BGR2GRAY);
        cv::absdiff(this_image, last_image, threshold_image);
        cv::threshold(threshold_image, threshold_image, difference_intensity_threshold, 255, cv::THRESH_BINARY);
        if (blur_on) {
            cv::blur(threshold_image, threshold_image, blur_size);
        }
        cv::threshold(threshold_image, threshold_image, difference_intensity_threshold, 255, cv::THRESH_BINARY);
        last_image = this_image.clone(); // Get a copy of the last image
    } else {
        threshold_image = this_image.clone();
        cv::cvtColor(threshold_image, threshold_image, cv::COLOR_BGR2GRAY);
        last_image = this_image.clone();
        cv::cvtColor(last_image, last_image, cv::COLOR_BGR2GRAY);
        last_image_set = true;
    }
}

void DifferenceDetector2D::tune() {

    if (tuning_on) {
        if (!tuning_windows_created) {
            createTuningWindows();
        }
        cv::imshow(tuning_image_title, threshold_image);
        cv::waitKey(1);

    } else if (!tuning_on && tuning_windows_created) {
        
        // TODO: Window will not actually close!!
        
        // Destroy the tuning windows
        cv::destroyWindow(tuning_image_title);
        tuning_windows_created = false;
    }
}

void DifferenceDetector2D::createTuningWindows() {

    // Create window for sliders
    cv::namedWindow(tuning_image_title, cv::WINDOW_NORMAL);

    // Create sliders and insert them into window
    cv::createTrackbar("THRESH", tuning_image_title, &difference_intensity_threshold, 256);
    cv::createTrackbar("BLUR", tuning_image_title, &blur_size.height, 50, &DifferenceDetector2D::blurSliderChangedCallback, this);

    tuning_windows_created = true;
}

void DifferenceDetector2D::blurSliderChangedCallback(int value, void* object) {
    DifferenceDetector2D* diff_detector = (DifferenceDetector2D*) object;
    diff_detector->set_blur_size(value);
}

void DifferenceDetector2D::set_blur_size(int value) {

    if (value > 0) {
        blur_on = true;
        blur_size = cv::Size(value, value);
    } else {
        blur_on = false;
    }
}
