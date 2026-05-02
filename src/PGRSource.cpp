/// FicTrac http://rjdmoore.net/fictrac/
/// \file       PGRSource.cpp
/// \brief      PGR USB2/3 sources (FlyCapture/Spinnaker SDK).
/// \author     Richard Moore
/// \copyright  CC BY-NC-SA 3.0

#if defined(PGR_USB2) || defined(PGR_USB3)

#include "PGRSource.h"

#include "Logger.h"
#include "timing.h"

#include <algorithm>
#include <string>

#if defined(PGR_USB3)
#include "SpinGenApi/SpinnakerGenApi.h"
using namespace Spinnaker;
#elif defined(PGR_USB2)
using namespace FlyCapture2;
#endif // PGR_USB2/3

using cv::Mat;

std::vector<std::string> PGRSource::describeAvailableCameras()
{
    std::vector<std::string> cameras;

    try {
#if defined(PGR_USB3)
        SystemPtr system = System::GetInstance();
        CameraList camList = system->GetCameras();
        unsigned int numCameras = camList.GetSize();

        for (unsigned int index = 0; index < numCameras; ++index) {
            CameraPtr cam = camList.GetByIndex(index);
            GenApi::INodeMap& nodeMap = cam->GetTLDeviceNodeMap();

            std::string model = "Unknown model";
            GenApi::CStringPtr modelNode = nodeMap.GetNode("DeviceModelName");
            if (IsAvailable(modelNode) && IsReadable(modelNode)) {
                model = modelNode->GetValue().c_str();
            }

            std::string serial = "unknown serial";
            GenApi::CStringPtr serialNode = nodeMap.GetNode("DeviceSerialNumber");
            if (IsAvailable(serialNode) && IsReadable(serialNode)) {
                serial = serialNode->GetValue().c_str();
            }

            cameras.push_back("Camera " + std::to_string(index) + ": " + model + " [" + serial + "]");
        }

        camList.Clear();
        system->ReleaseInstance();
#elif defined(PGR_USB2)
        BusManager busMgr;
        unsigned int numCameras = 0;
        if (busMgr.GetNumOfCameras(&numCameras) == PGRERROR_OK) {
            for (unsigned int index = 0; index < numCameras; ++index) {
                cameras.push_back("Camera " + std::to_string(index));
            }
        }
#endif // PGR_USB3
    }
#if defined(PGR_USB3)
    catch (Spinnaker::Exception& e) {
        LOG_ERR("Error enumerating cameras! Error was: %s", e.what());
    }
#endif // PGR_USB3
    catch (...) {
        LOG_ERR("Error enumerating cameras!");
    }

    return cameras;
}

bool PGRSource::tryParseFPSControlMode(const std::string& value, FPSControlMode& mode)
{
    if ((value == "auto") || (value == "AUTO") || (value == "Auto")) {
        mode = FPSControlMode::AUTO;
        return true;
    }
    if ((value == "device") || (value == "DEVICE") || (value == "Device") || (value == "camera_framerate")) {
        mode = FPSControlMode::DEVICE;
        return true;
    }
    if ((value == "hardware_triggered") || (value == "HARDWARE_TRIGGERED") || (value == "HardwareTriggered") || (value == "timing_hint")) {
        mode = FPSControlMode::HARDWARE_TRIGGERED;
        return true;
    }
    return false;
}

const char* PGRSource::fpsControlModeName(FPSControlMode mode)
{
    switch (mode) {
    case FPSControlMode::DEVICE:
        return "device";
    case FPSControlMode::HARDWARE_TRIGGERED:
        return "hardware_triggered";
    case FPSControlMode::AUTO:
    default:
        return "auto";
    }
}

PGRSource::PGRSource(int index, long int first_frame_timeout_ms, FPSControlMode fps_control_mode)
{
    try {
#if defined(PGR_USB3)
    _first_frame_timeout_ms = first_frame_timeout_ms;
    _fps_control_mode = fps_control_mode;

        // Retrieve singleton reference to system object
        _system = System::GetInstance();

        // Print out current library version
        const LibraryVersion spinnakerLibraryVersion = _system->GetLibraryVersion();
        LOG("Opening PGR camera using Spinnaker SDK (version %d.%d.%d.%d)",
            spinnakerLibraryVersion.major, spinnakerLibraryVersion.minor,
            spinnakerLibraryVersion.type, spinnakerLibraryVersion.build);

        // Retrieve list of cameras from the system
        _camList = _system->GetCameras();

        unsigned int numCameras = _camList.GetSize();

        if (numCameras == 0) {
            LOG_ERR("Error! Could not find any connected PGR cameras!");
            return;
        }
        else {
            LOG_DBG("Found %d PGR cameras. Connecting to camera %d..", numCameras, index);
        }

        // Select camera
        _cam = _camList.GetByIndex(index);

        // Initialize camera
        _cam->Init();

        // set acquisition mode - needed?
        {
            // Retrieve GenICam nodemap
            Spinnaker::GenApi::INodeMap& nodeMap = _cam->GetNodeMap();

            // Retrieve enumeration node from nodemap
            Spinnaker::GenApi::CEnumerationPtr ptrAcquisitionMode = nodeMap.GetNode("AcquisitionMode");
            if (!IsAvailable(ptrAcquisitionMode) || !IsWritable(ptrAcquisitionMode)) {
                LOG_ERR("Unable to set acquisition mode to continuous (enum retrieval)!");
                return;
            }

            // Retrieve entry node from enumeration node
            Spinnaker::GenApi::CEnumEntryPtr ptrAcquisitionModeContinuous = ptrAcquisitionMode->GetEntryByName("Continuous");
            if (!IsAvailable(ptrAcquisitionModeContinuous) || !IsReadable(ptrAcquisitionModeContinuous)) {
                LOG_ERR("Unable to set acquisition mode to continuous (entry retrieval)!");
                return;
            }

            // Retrieve integer value from entry node
            int64_t acquisitionModeContinuous = ptrAcquisitionModeContinuous->GetValue();

            // Set integer value from entry node as new value of enumeration node
            ptrAcquisitionMode->SetIntValue(acquisitionModeContinuous);

            LOG_DBG("Acquisition mode set to continuous.");
        }

        // Begin acquiring images
        _cam->BeginAcquisition();

        // Get some params
        _width = _cam->Width();
        _height = _cam->Height();
        _fps = getFPS();

        LOG("PGR source fps control mode: %s", fpsControlModeName(_fps_control_mode));

        if (_first_frame_timeout_ms <= 0) {
            LOG("PGR first-frame wait configured to wait indefinitely for the first trigger");
        }
        else {
            LOG("PGR first-frame wait configured to %ld ms", _first_frame_timeout_ms);
        }
#elif defined(PGR_USB2)
        LOG_DBG("Looking for camera at index %d...", index);

        BusManager busMgr;
        PGRGuid guid;
        Error error = busMgr.GetCameraFromIndex(index, &guid);
        if (error != PGRERROR_OK) {
            LOG_ERR("Error reading camera GUID!");
            return;
        }

        _cam = std::make_shared<Camera>();
        error = _cam->Connect(&guid);
        if (error != PGRERROR_OK) {
            LOG_ERR("Error connecting to camera!");
            return;
        }

        CameraInfo camInfo;
        error = _cam->GetCameraInfo(&camInfo);
        if (error != PGRERROR_OK) {
            LOG_ERR("Error retrieving camera information!");
            return;
        }
        else {
            LOG_DBG("Connected to PGR camera (%s/%s max res: %s)", camInfo.modelName, camInfo.sensorInfo, camInfo.sensorResolution);
        }

        error = _cam->StartCapture();
        if (error != PGRERROR_OK) {
            LOG_ERR("Error starting video capture!");
            return;
        }

        Image::SetDefaultColorProcessing(ColorProcessingAlgorithm::NEAREST_NEIGHBOR);

        // capture test image
        Image testImg;
        error = _cam->RetrieveBuffer(&testImg);
        if (error != PGRERROR_OK) {
            LOG_ERR("Error capturing image!");
            return;
        }
        _width = testImg.GetCols();
        _height = testImg.GetRows();
        _fps = getFPS();
#endif // PGR_USB2/3

        LOG("PGR camera initialised (%dx%d @ %.3f fps)!", _width, _height, _fps);

        _open = true;
        _live = true;
    }
#if defined(PGR_USB3)
    catch (Spinnaker::Exception& e) {
        LOG_ERR("Error opening capture device! Error was: %s", e.what());
    }
#endif // PGR_USB3
    catch (...) {
        LOG_ERR("Error opening capture device!");
    }
}

PGRSource::~PGRSource()
{
    if (_open) {
        try {
#if defined(PGR_USB3)
            _cam->EndAcquisition();
#elif defined(PGR_USB2)
            _cam->StopCapture();
#endif // PGR_USB2/3
        }
#if defined(PGR_USB3)
        catch (Spinnaker::Exception& e) {
            LOG_ERR("Error ending acquisition! Error was: %s", e.what());
        }
#endif // PGR_USB3
        catch (...) {
            LOG_ERR("Error ending acquisition!");
        }
        _open = false;
    }

#if defined(PGR_USB3)
    if ((_cam != NULL) && !_camera_deinitialized) {
        try {
            _cam->DeInit();
            _camera_deinitialized = true;
        }
        catch (Spinnaker::Exception& e) {
            LOG_ERR("Error deinitializing camera! Error was: %s", e.what());
        }
        catch (...) {
            LOG_ERR("Error deinitializing camera!");
        }
    }
#endif // PGR_USB3

#if defined(PGR_USB2)
    _cam->Disconnect();
#endif // PGR_USB2

    _cam = NULL;

#if defined(PGR_USB3)
    // Clear camera list before releasing system
    _camList.Clear();

    // Release system
    _system->ReleaseInstance();
#endif // PGR_USB3
    
}

double PGRSource::getFPS()
{
    double fps = _fps;
    if (_open) {
#if defined(PGR_USB3)
        try {
            fps = _cam->AcquisitionResultingFrameRate();
        }
        catch (Spinnaker::Exception& e) {
            LOG_ERR("Error retrieving camera frame rate! Error was: %s", e.what());
        }
        catch (...) {
            LOG_ERR("Error retrieving camera frame rate!");
        }
#endif // PGR_USB3
    }
    return fps;
}

bool PGRSource::setFPS(double fps)
{
    bool ret = false;
    if (_open && (fps > 0)) {
#if defined(PGR_USB3)
        try {
            bool trigger_mode_on = false;

            Spinnaker::GenApi::INodeMap& nodeMap = _cam->GetNodeMap();
            Spinnaker::GenApi::CEnumerationPtr ptrTriggerMode = nodeMap.GetNode("TriggerMode");
            if (IsAvailable(ptrTriggerMode) && IsReadable(ptrTriggerMode)) {
                Spinnaker::GenApi::CEnumEntryPtr ptrTriggerModeCurrent = ptrTriggerMode->GetCurrentEntry();
                if (IsAvailable(ptrTriggerModeCurrent) && IsReadable(ptrTriggerModeCurrent)) {
                    trigger_mode_on = (ptrTriggerModeCurrent->GetSymbolic() == "On");
                }
            }

            const bool use_hardware_triggered_mode =
                (_fps_control_mode == FPSControlMode::HARDWARE_TRIGGERED)
                || ((_fps_control_mode == FPSControlMode::AUTO) && trigger_mode_on);

            if (use_hardware_triggered_mode) {
                if (IsWritable(_cam->AcquisitionFrameRateEnable)) {
                    _cam->AcquisitionFrameRateEnable.SetValue(false);
                }
                _fps = fps;
                if ((_fps_control_mode == FPSControlMode::HARDWARE_TRIGGERED) && !trigger_mode_on) {
                    LOG_WRN("Configured src_fps_mode=hardware_triggered but camera TriggerMode is not currently On; using source fps %.2f as a timing hint only.", _fps);
                }
                else {
                    LOG("Using configured source fps %.2f for timing only and leaving device frame-rate control disabled.", _fps);
                }
                return true;
            }

            _cam->AcquisitionFrameRateEnable.SetValue(true);
            _cam->AcquisitionFrameRate.SetValue(fps);
        }
        catch (Spinnaker::Exception& e) {
            LOG_ERR("Error setting frame rate! Error was: %s", e.what());
        }
        catch (...) {
            LOG_ERR("Error setting frame rate!");
        }
#endif // PGR_USB3
        _fps = getFPS();
        LOG("Device frame rate is now %.2f", _fps);
        ret = true;
    }
    return ret;
}

bool PGRSource::grab(cv::Mat& frame)
{
	if( !_open ) { return false; }

#if defined(PGR_USB3)
    ImagePtr pgr_image = NULL;
    bool allow_terminal_drain_retry = _received_first_frame;

    try {
        // Retrieve next received image
        long int timeout = _fps > 0 ? std::max(static_cast<long int>(1000), static_cast<long int>(1000. / _fps)) : 1000; // set capture timeout to at least 1000 ms
        while (true) {
            try {
                if (!_received_first_frame && (_first_frame_timeout_ms <= 0)) {
                    pgr_image = _cam->GetNextImage();
                }
                else {
                    long int request_timeout = timeout;
                    if (!_received_first_frame) {
                        request_timeout = std::max(request_timeout, _first_frame_timeout_ms);
                    }
                    else if (!allow_terminal_drain_retry) {
                        request_timeout = 20;
                    }
                    pgr_image = _cam->GetNextImage(request_timeout);
                }
                break;
            }
            catch (Spinnaker::Exception& e) {
                const std::string message = e.what();
                if (message.find("NEW_BUFFER_DATA") != std::string::npos) {
                    if (allow_terminal_drain_retry) {
                        LOG("PGR trigger stream reported end-of-buffer; making one short drain attempt before closing.");
                        allow_terminal_drain_retry = false;
                        continue;
                    }
                    LOG("PGR trigger stream ended; closing camera cleanly.");
                    try {
                        _cam->EndAcquisition();
                    }
                    catch (...) {
                    }
                    if (!_camera_deinitialized) {
                        try {
                            _cam->DeInit();
                            _camera_deinitialized = true;
                        }
                        catch (...) {
                        }
                    }
                    _open = false;
                    if (pgr_image != NULL) {
                        pgr_image->Release();
                    }
                    return false;
                }
                throw;
            }
        }
        double ts = ts_ms();    // backup, in case the device timestamp is junk
        _ms_since_midnight = ms_since_midnight();
        _timestamp = pgr_image->GetTimeStamp();
        LOG_DBG("Frame captured %dx%d%d @ %f (t_sys: %f ms, t_day: %f ms)", pgr_image->GetWidth(), pgr_image->GetHeight(), pgr_image->GetNumChannels(), _timestamp, ts, _ms_since_midnight);
        if (_timestamp <= 0) {
            _timestamp = ts;
        }

        // Ensure image completion
        if (pgr_image->IsIncomplete()) {
            // Retreive and print the image status description
            LOG_ERR("Error! Image capture incomplete (%s).", Image::GetImageStatusDescription(pgr_image->GetImageStatus()));
            pgr_image->Release();
            return false;
        }

        _received_first_frame = true;
        _grabbed_frame_count++;
        if (_grabbed_frame_count == 1) {
            _first_grabbed_timestamp = _timestamp;
            _first_grabbed_ms_since_midnight = _ms_since_midnight;
        }
        _last_grabbed_timestamp = _timestamp;
        _last_grabbed_ms_since_midnight = _ms_since_midnight;
    }
    catch (Spinnaker::Exception& e) {
        LOG_ERR("Error grabbing frame! Error was: %s", e.what());
        if (pgr_image != NULL) {
            pgr_image->Release();
        }
        return false;
    }
    catch (...) {
        LOG_ERR("Error grabbing frame!");
        if (pgr_image != NULL) {
            pgr_image->Release();
        }
        return false;
    }

    try {
        // Convert image
        ImageProcessor processor;
        processor.SetColorProcessing(SPINNAKER_COLOR_PROCESSING_ALGORITHM_NEAREST_NEIGHBOR);
        ImagePtr bgr_image = processor.Convert(pgr_image, PixelFormat_BGR8);

        Mat tmp(_height, _width, CV_8UC3, bgr_image->GetData(), bgr_image->GetStride());
        tmp.copyTo(frame);

        // We have to release our original image to clear space on the buffer
        pgr_image->Release();

        return true;
    }
    catch (Spinnaker::Exception& e) {
        LOG_ERR("Error converting frame! Error was: %s", e.what());
        if (pgr_image != NULL) {
            pgr_image->Release();
        }
        return false;
    }
    catch (...) {
        LOG_ERR("Error converting frame!");
        if (pgr_image != NULL) {
            pgr_image->Release();
        }
        return false;
    }
#elif defined(PGR_USB2)
    Image frame_raw;
    Error error = _cam->RetrieveBuffer(&frame_raw);
    double ts = ts_ms();    // backup, in case the device timestamp is junk
    //LOG_DBG("Frame captured %dx%d%d @ %f (%f)", pgr_image->GetWidth(), pgr_image->GetHeight(), pgr_image->GetNumChannels(), _timestamp, ts);
    if (error != PGRERROR_OK) {
        LOG_ERR("Error grabbing image frame!");
        return false;
    }
    auto timestamp = frame_raw.GetTimeStamp();
    _timestamp = timestamp.seconds * 1e3 + timestamp.microSeconds / (double)1e3;
    if (_timestamp <= 0) {
        _timestamp = ts;
    }

    Image frame_bgr;
    error = frame_raw.Convert(PIXEL_FORMAT_BGR, &frame_bgr);
    if (error != PGRERROR_OK) {
        LOG_ERR("Error converting image format!");
        return false;
    }
    Mat frame_cv(frame_bgr.GetRows(), frame_bgr.GetCols(), CV_8UC3, frame_bgr.GetData(), frame_bgr.GetStride());
    frame_cv.copyTo(frame);
    return true;
#endif // PGR_USB2/3
}

#endif // PGR_USB2/3
