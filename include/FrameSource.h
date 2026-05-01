/// FicTrac http://rjdmoore.net/fictrac/
/// \file       FrameSource.h
/// \brief      Abstract template for frame sources.
/// \author     Richard Moore
/// \copyright  CC BY-NC-SA 3.0

#pragma once

#include <opencv2/opencv.hpp>

enum BAYER_TYPE { BAYER_NONE, BAYER_RGGB, BAYER_GRBG, BAYER_GBRG, BAYER_BGGR };

class FrameSource {
public:
	FrameSource() : _open(false), _bayerType(BAYER_NONE), _width(-1), _height(-1), _timestamp(-1), _fps(-1), _ms_since_midnight(-1), _live(true), _grabbed_frame_count(0), _first_grabbed_timestamp(-1), _last_grabbed_timestamp(-1), _first_grabbed_ms_since_midnight(-1), _last_grabbed_ms_since_midnight(-1) {}
	virtual ~FrameSource() {}

    virtual double getFPS() { return _fps; }
    virtual bool setFPS(double fps) {
        _fps = fps;
        return false;   // we haven't actually done anything
    }
	virtual bool rewind()=0;
	virtual bool grab(cv::Mat& frame)=0;

	bool isOpen() { return _open; }
	int getWidth() { return _width; }
	int getHeight() { return _height; }
	double getTimestamp() { return _timestamp; }
    double getMsSinceMidnight() { return _ms_since_midnight; }
	long long getGrabbedFrameCount() { return _grabbed_frame_count; }
	double getFirstGrabbedTimestamp() { return _first_grabbed_timestamp; }
	double getLastGrabbedTimestamp() { return _last_grabbed_timestamp; }
	double getFirstGrabbedMsSinceMidnight() { return _first_grabbed_ms_since_midnight; }
	double getLastGrabbedMsSinceMidnight() { return _last_grabbed_ms_since_midnight; }
	void setBayerType(BAYER_TYPE bayer_type) { _bayerType = bayer_type; }
    bool isLive() { return _live; }

protected:
	bool _open;
	BAYER_TYPE _bayerType;
	int _width, _height;
	double _timestamp, _fps, _ms_since_midnight;
    bool _live;
	long long _grabbed_frame_count;
	double _first_grabbed_timestamp;
	double _last_grabbed_timestamp;
	double _first_grabbed_ms_since_midnight;
	double _last_grabbed_ms_since_midnight;
};
