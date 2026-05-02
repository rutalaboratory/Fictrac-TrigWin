/// FicTrac http://rjdmoore.net/fictrac/
/// \file       PGRSource.h
/// \brief      PGR USB2/3 sources (FlyCapture/Spinnaker SDK).
/// \author     Richard Moore
/// \copyright  CC BY-NC-SA 3.0

#if defined(PGR_USB2) || defined(PGR_USB3)

#pragma once

#include "FrameSource.h"

#if defined(PGR_USB3)
#include <Spinnaker.h>
#elif defined(PGR_USB2)
#include <FlyCapture2.h>
#include <memory>
#endif // PGR_USB2/3

#include <opencv2/opencv.hpp>

#include <string>
#include <vector>

class PGRSource : public FrameSource {
public:
    static constexpr long int DEFAULT_FIRST_FRAME_TIMEOUT_MS = 30000;
    static std::vector<std::string> describeAvailableCameras();

    enum class FPSControlMode {
        AUTO,
        DEVICE,
        HARDWARE_TRIGGERED
    };

    static bool tryParseFPSControlMode(const std::string& value, FPSControlMode& mode);
    static const char* fpsControlModeName(FPSControlMode mode);

    PGRSource(int index=0, long int first_frame_timeout_ms=30000, FPSControlMode fps_control_mode=FPSControlMode::AUTO);
	virtual ~PGRSource();

    virtual double getFPS();
	virtual bool setFPS(double fps);
    virtual bool rewind() { return false; };
	virtual bool grab(cv::Mat& frame);

private:
#if defined(PGR_USB3)
    Spinnaker::SystemPtr _system;
    Spinnaker::CameraList _camList;
    Spinnaker::CameraPtr _cam;
    long int _first_frame_timeout_ms = DEFAULT_FIRST_FRAME_TIMEOUT_MS;
    FPSControlMode _fps_control_mode = FPSControlMode::AUTO;
    bool _received_first_frame = false;
    bool _camera_deinitialized = false;
#elif defined(PGR_USB2)
    std::shared_ptr<FlyCapture2::Camera> _cam;
#endif // PGR_USB2/3
};

#endif // PGR_USB2/3
