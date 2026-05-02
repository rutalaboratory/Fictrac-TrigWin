/// FicTrac http://rjdmoore.net/fictrac/
/// \file       configGui.cpp
/// \brief      Interactive GUI for configuring FicTrac.
/// \author     Richard Moore
/// \copyright  CC BY-NC-SA 3.0

//TODO: Add support for edge clicks rather than square corner clicks.

#include "ConfigGui.h"

#include "typesvars.h"
#include "CameraModel.h"
#include "geometry.h"
#include "drawing.h"
#include "Logger.h"
#include "timing.h"
#include "misc.h"
#include "CVSource.h"
#if defined(PGR_USB2) || defined(PGR_USB3)
#include "PGRSource.h"
#endif // PGR_USB2/3

/// OpenCV individual includes required by gcc?
#include <opencv2/highgui.hpp>
#include <opencv2/imgproc.hpp>
#include <opencv2/imgcodecs.hpp>
#include <opencv2/videoio.hpp>

#include <iostream> // getline, stoi
#include <cctype>
#include <exception>
#include <algorithm>
#include <limits>

using cv::Mat;
using cv::Point2d;
using cv::Scalar;
using std::vector;
using std::string;

///
/// Constant variables.
///
const int       ZOOM_DIM    = 600;
const double    ZOOM_SCL    = 1.0 / 10.0;
const int       MAX_DISP_DIM    = -1;

const int NCOLOURS = 6;
cv::Scalar COLOURS[NCOLOURS] = {
    Scalar(255, 0,   0),
    Scalar(0,   255, 0),
    Scalar(0,   0,   255),
    Scalar(255, 255, 0),
    Scalar(0,   255, 255),
    Scalar(255, 0,   255)
};

static void pumpGuiEvents(int iterations = 3)
{
    for (int i = 0; i < iterations; ++i) {
        cv::waitKey(1);
    }
}

enum OverlayAnchor {
    OVERLAY_TOP_LEFT,
    OVERLAY_BOTTOM_LEFT,
    OVERLAY_CENTER,
};

static bool& overlayHelpEnabled()
{
    static bool enabled = false;
    return enabled;
}

static bool handleOverlayToggleKey(int key)
{
    key &= 0xff;
    if ((key == 'h') || (key == 'H')) {
        overlayHelpEnabled() = !overlayHelpEnabled();
        return true;
    }
    return false;
}

static vector<string> wrapOverlayLines(const vector<string>& lines, int max_text_width, double text_scale, int text_thickness)
{
    vector<string> wrapped_lines;
    int baseline = 0;

    for (const auto& line : lines) {
        if (line.empty()) {
            wrapped_lines.push_back(string());
            continue;
        }

        string current_line;
        size_t start = 0;
        while (start < line.size()) {
            size_t end = line.find(' ', start);
            string token = line.substr(start, (end == string::npos) ? string::npos : end - start);
            string candidate = current_line.empty() ? token : current_line + " " + token;
            cv::Size candidate_size = cv::getTextSize(candidate, cv::FONT_HERSHEY_SIMPLEX, text_scale, text_thickness, &baseline);

            if (!current_line.empty() && (candidate_size.width > max_text_width)) {
                wrapped_lines.push_back(current_line);
                current_line = token;
            }
            else {
                current_line = candidate;
            }

            if (end == string::npos) {
                break;
            }
            start = end + 1;
        }

        if (!current_line.empty()) {
            wrapped_lines.push_back(current_line);
        }
    }

    return wrapped_lines;
}

static void drawOverlayPanel(Mat& frame, const string& title, const vector<string>& lines, OverlayAnchor anchor = OVERLAY_TOP_LEFT, double max_width_ratio = 0.34)
{
    const int outer_margin = 10;
    const int inner_padding = 8;
    const int line_gap = 4;
    const double title_scale = 0.52;
    const double body_scale = 0.42;
    const int title_thickness = 1;
    const int body_thickness = 1;
    const int max_width = std::max(static_cast<int>(frame.cols * max_width_ratio), 160);
    const int max_text_width = std::max(max_width - inner_padding * 2, 80);

    int baseline = 0;
    cv::Size title_size = cv::getTextSize(title, cv::FONT_HERSHEY_SIMPLEX, title_scale, title_thickness, &baseline);
    int panel_width = title_size.width;
    const vector<string> wrapped_lines = wrapOverlayLines(lines, max_text_width, body_scale, body_thickness);
    vector<cv::Size> line_sizes;
    line_sizes.reserve(wrapped_lines.size());
    for (const auto& line : wrapped_lines) {
        cv::Size line_size = cv::getTextSize(line, cv::FONT_HERSHEY_SIMPLEX, body_scale, body_thickness, &baseline);
        line_sizes.push_back(line_size);
        panel_width = std::max(panel_width, line_size.width);
    }
    panel_width = std::min(panel_width + inner_padding * 2, max_width);

    int panel_height = inner_padding * 2 + title_size.height;
    if (!wrapped_lines.empty()) {
        panel_height += line_gap;
        for (const auto& line_size : line_sizes) {
            panel_height += std::max(line_size.height, 10) + line_gap;
        }
        panel_height -= line_gap;
    }

    panel_height = std::min(panel_height, std::max(frame.rows - outer_margin * 2, 60));

    int panel_x = outer_margin;
    int panel_y = outer_margin;
    if (anchor == OVERLAY_BOTTOM_LEFT) {
        panel_y = frame.rows - outer_margin - panel_height;
    }
    else if (anchor == OVERLAY_CENTER) {
        panel_x = std::max((frame.cols - panel_width) / 2, outer_margin);
        panel_y = std::max((frame.rows - panel_height) / 2, outer_margin);
    }

    cv::Rect panel_rect(panel_x, panel_y, panel_width, panel_height);
    cv::Mat panel_roi = frame(panel_rect);
    cv::Mat tint(panel_roi.size(), panel_roi.type(), cv::Scalar(24, 24, 24));
    cv::addWeighted(tint, 0.62, panel_roi, 0.38, 0.0, panel_roi);
    cv::rectangle(frame, panel_rect, Scalar(220, 220, 220), 1, cv::LINE_AA);

    int text_x = panel_rect.x + inner_padding;
    int text_y = panel_rect.y + inner_padding + title_size.height;
    cv::putText(frame, title, cv::Point(text_x, text_y), cv::FONT_HERSHEY_SIMPLEX, title_scale, Scalar(255, 255, 255), title_thickness, cv::LINE_AA);

    text_y += line_gap;
    for (size_t index = 0; index < wrapped_lines.size(); ++index) {
        text_y += std::max(line_sizes[index].height, 10) + line_gap;
        if (!wrapped_lines[index].empty()) {
            cv::putText(frame, wrapped_lines[index], cv::Point(text_x, text_y), cv::FONT_HERSHEY_SIMPLEX, body_scale, Scalar(230, 230, 230), body_thickness, cv::LINE_AA);
        }
    }
}

static void drawFooterPanel(Mat& frame, const vector<string>& lines)
{
    if (lines.empty()) {
        return;
    }

    const int outer_margin = 8;
    const int inner_padding = 6;
    const int line_gap = 2;
    const double body_scale = 0.36;
    const int body_thickness = 1;
    const int max_width = std::max(static_cast<int>(frame.cols * 0.70), 180);
    const int max_text_width = std::max(max_width - inner_padding * 2, 80);
    const vector<string> wrapped_lines = wrapOverlayLines(lines, max_text_width, body_scale, body_thickness);

    int baseline = 0;
    int panel_width = 0;
    int panel_height = inner_padding * 2;
    vector<cv::Size> line_sizes;
    line_sizes.reserve(wrapped_lines.size());
    for (const auto& line : wrapped_lines) {
        cv::Size line_size = cv::getTextSize(line, cv::FONT_HERSHEY_SIMPLEX, body_scale, body_thickness, &baseline);
        line_sizes.push_back(line_size);
        panel_width = std::max(panel_width, line_size.width);
        panel_height += std::max(line_size.height, 9) + line_gap;
    }
    panel_width = std::min(panel_width + inner_padding * 2, max_width);
    panel_height -= line_gap;

    const int panel_x = outer_margin;
    const int panel_y = frame.rows - outer_margin - panel_height;
    cv::Rect panel_rect(panel_x, panel_y, panel_width, panel_height);
    cv::Mat panel_roi = frame(panel_rect);
    cv::Mat tint(panel_roi.size(), panel_roi.type(), cv::Scalar(18, 18, 18));
    cv::addWeighted(tint, 0.52, panel_roi, 0.48, 0.0, panel_roi);
    cv::rectangle(frame, panel_rect, Scalar(200, 200, 200), 1, cv::LINE_AA);

    int text_x = panel_rect.x + inner_padding;
    int text_y = panel_rect.y + inner_padding;
    for (size_t index = 0; index < wrapped_lines.size(); ++index) {
        text_y += std::max(line_sizes[index].height, 9);
        if (!wrapped_lines[index].empty()) {
            cv::putText(frame, wrapped_lines[index], cv::Point(text_x, text_y), cv::FONT_HERSHEY_SIMPLEX, body_scale, Scalar(245, 245, 245), body_thickness, cv::LINE_AA);
        }
        text_y += line_gap;
    }
}

static void showConfigGuiFrame(const Mat& disp_frame, float disp_scl, const vector<string>& footer_lines, const string& help_title = string(), const vector<string>& help_lines = {}, OverlayAnchor help_anchor = OVERLAY_TOP_LEFT, double help_width_ratio = 0.34)
{
    Mat frame_to_show = disp_frame.clone();
    drawFooterPanel(frame_to_show, footer_lines);
    if (overlayHelpEnabled() && !help_title.empty()) {
        drawOverlayPanel(frame_to_show, help_title, help_lines, help_anchor, help_width_ratio);
    }
    if (disp_scl > 0) {
        cv::resize(frame_to_show, frame_to_show, cv::Size(), disp_scl, disp_scl);
    }
    cv::imshow("configGUI", frame_to_show);
}

enum PromptChoice {
    PROMPT_KEEP,
    PROMPT_RECONFIGURE,
    PROMPT_CANCEL,
};

static PromptChoice waitForPromptChoice(const Mat& disp_frame, float disp_scl, const string& title, const vector<string>& lines)
{
    const vector<string> footer_lines = {
        title,
        "Enter/Y keep  N redraw  H help  Esc cancel"
    };

    while (true) {
        showConfigGuiFrame(disp_frame, disp_scl, footer_lines, title, lines, OVERLAY_TOP_LEFT, 0.28);
        int key = cv::waitKeyEx(15);
        if (key < 0) {
            continue;
        }

        if (handleOverlayToggleKey(key)) {
            continue;
        }

        key &= 0xff;
        switch (std::tolower(key)) {
        case 'y':
            return PROMPT_KEEP;
        case 'n':
            return PROMPT_RECONFIGURE;
        case 0x0d:
        case 0x0a:
            return PROMPT_KEEP;
        case 0x1b:
            return PROMPT_CANCEL;
        default:
            break;
        }
    }
}

static int waitForMethodSelection(const Mat& disp_frame, float disp_scl)
{
    const vector<string> lines = {
        "Choose the animal-frame method.",
        "1: XY square, camera above or below the animal",
        "2: YZ square, camera in front of or behind the animal",
        "3: XZ square, camera to the left or right of the animal",
        "5: External transform",
        "Esc: cancel"
    };

    while (true) {
        showConfigGuiFrame(disp_frame, disp_scl, {
            "Animal Frame",
            "1 XY  2 YZ  3 XZ  5 external  Esc cancel"
        }, "Animal Frame", lines, OVERLAY_TOP_LEFT, 0.30);
        int key = cv::waitKeyEx(15);
        if (key < 0) {
            continue;
        }

        if (handleOverlayToggleKey(key)) {
            continue;
        }

        key &= 0xff;
        switch (key) {
        case '1':
        case '2':
        case '3':
        case '5':
            return key - '0';
        case 0x1b:
            return 0;
        default:
            break;
        }
    }
}

static Mat makeSelectionBackground(int width = 960, int height = 540)
{
    Mat background(height, width, CV_8UC3, Scalar(20, 20, 20));
    for (int y = 0; y < background.rows; ++y) {
        double blend = static_cast<double>(y) / std::max(background.rows - 1, 1);
        cv::line(background, cv::Point(0, y), cv::Point(background.cols - 1, y), Scalar(18 + 14 * blend, 24 + 18 * blend, 28 + 22 * blend), 1, cv::LINE_8);
    }
    return background;
}

static int waitForListSelection(const string& title, const vector<string>& options, int initial_index, const vector<string>& instructions)
{
    if (options.empty()) {
        return -1;
    }

    int selected_index = clamp(initial_index, 0, static_cast<int>(options.size()) - 1);
    Mat background = makeSelectionBackground();

    while (true) {
        vector<string> lines = instructions;
        lines.push_back("");
        for (size_t index = 0; index < options.size(); ++index) {
            const string prefix = (static_cast<int>(index) == selected_index) ? "> " : "  ";
            lines.push_back(prefix + std::to_string(index) + ": " + options[index]);
        }
        lines.push_back("");
        lines.push_back("Up or Down: move    Enter: select    Esc: cancel");
        if (options.size() <= 10) {
            lines.push_back("Number keys 0-9: jump directly to a camera");
        }

        drawOverlayPanel(background, title, lines, OVERLAY_CENTER, 0.60);
        cv::imshow("configGUI", background);
        int key = cv::waitKeyEx(15);
        if (key < 0) {
            continue;
        }

        if ((key == 0x0d) || (key == 0x0a)) {
            cv::destroyWindow("configGUI");
            return selected_index;
        }
        if (key == 0x1b) {
            cv::destroyWindow("configGUI");
            return -1;
        }
        if ((key == 2490368) || (key == 0x26) || (key == 'w') || (key == 'W')) {
            selected_index = (selected_index + static_cast<int>(options.size()) - 1) % static_cast<int>(options.size());
            continue;
        }
        if ((key == 2621440) || (key == 0x28) || (key == 's') || (key == 'S')) {
            selected_index = (selected_index + 1) % static_cast<int>(options.size());
            continue;
        }
        key &= 0xff;
        if ((key >= '0') && (key <= '9')) {
            const int direct_index = key - '0';
            if (direct_index < static_cast<int>(options.size())) {
                selected_index = direct_index;
            }
        }
    }
}

///
/// Collect mouse events from config GUI window.
///
void onMouseEvent(int event, int x, int y, int f, void* ptr)
{
    ConfigGui::INPUT_DATA* pdata = static_cast<ConfigGui::INPUT_DATA*>(ptr);
    if (pdata->ptScl > 0) {
        x = round(x * pdata->ptScl);
        y = round(y * pdata->ptScl);
    }
    switch(event)
    {
        case cv::EVENT_LBUTTONDOWN:
            break;
            
        case cv::EVENT_LBUTTONUP:
            switch(pdata->mode)
            {
                case ConfigGui::CIRC_PTS:
                    pdata->circPts.push_back(Point2d(x,y));
                    pdata->newEvent = true;
                    break;
                    
                case ConfigGui::IGNR_PTS:
                    // ensure there is at least one active ignore region
                    if (pdata->ignrPts.empty()) { pdata->ignrPts.push_back(vector<Point2d>()); }
                    // add click to the active ignore region
                    pdata->ignrPts.back().push_back(Point2d(x,y));
                    pdata->newEvent = true;
                    break;
                    
                case ConfigGui::R_XY:
                case ConfigGui::R_YZ:
                case ConfigGui::R_XZ:
                    pdata->sqrPts.push_back(Point2d(x,y));
                    pdata->newEvent = true;
                    break;
                    
                default:
                    break;
            }
            break;
        
        case cv::EVENT_RBUTTONUP:
            switch(pdata->mode)
            {
                case ConfigGui::CIRC_PTS:
                    if (pdata->circPts.size() > 0) { pdata->circPts.pop_back(); }
                    pdata->newEvent = true;
                    break;
                    
                case ConfigGui::IGNR_PTS:
                    if (!pdata->ignrPts.empty()) {
                        // if the active ignore region is empty, remove it
                        if (pdata->ignrPts.back().empty()) { pdata->ignrPts.pop_back(); }
                        // otherwise remove points from the active ignore region
                        else { pdata->ignrPts.back().pop_back(); }
                    }
                    pdata->newEvent = true;
                    break;
                    
                case ConfigGui::R_XY:
                case ConfigGui::R_YZ:
                case ConfigGui::R_XZ:
                    if (pdata->sqrPts.size() > 0) { pdata->sqrPts.pop_back(); }
                    pdata->newEvent = true;
                    break;
                    
                default:
                    break;
            }
            break;
        
        case cv::EVENT_MOUSEMOVE:
            pdata->cursorPt.x = x;
            pdata->cursorPt.y = y;
            break;

        default:
            break;
    }
}

///
/// Create a zoomed ROI.
///
void createZoomROI(Mat& zoom_roi, const Mat& frame, const Point2d& pt, int orig_dim)
{
    int x = frame.cols/2;
    if (pt.x >= 0) { x = clamp(int(pt.x - orig_dim/2 + 0.5), int(orig_dim/2), frame.cols - 1 - orig_dim); }
    int y = frame.rows/2;
    if (pt.y >= 0) { y = clamp(int(pt.y - orig_dim/2 + 0.5), 0, frame.rows - 1 - orig_dim); }
    Mat crop_rect = frame(cv::Rect(x, y, orig_dim, orig_dim));
    cv::resize(crop_rect, zoom_roi, zoom_roi.size());
}

///
/// Constructor.
///
ConfigGui::ConfigGui(string config_fn, string src_override)
: _config_fn(config_fn), _cancelled(false)
{
    int first_frame_timeout_ms = 0;
    string configured_fps_control_mode = PGRSource::fpsControlModeName(PGRSource::FPSControlMode::AUTO);
    PGRSource::FPSControlMode fps_control_mode = PGRSource::FPSControlMode::AUTO;

    /// Load and parse config file.
    if (_cfg.read(_config_fn) <= 0) {
        LOG_ERR("Error! Could not read from config file (%s).", _config_fn.c_str());
        return;
    }

    if (_cfg.getInt("src_first_frame_timeout_ms", first_frame_timeout_ms)) {
        // use configured value
    }
    else {
        _cfg.add("src_first_frame_timeout_ms", first_frame_timeout_ms);
    }

    if (_cfg.getStr("src_fps_mode", configured_fps_control_mode)) {
        if (!PGRSource::tryParseFPSControlMode(configured_fps_control_mode, fps_control_mode)) {
            LOG_WRN("Unrecognized src_fps_mode (%s); defaulting to auto.", configured_fps_control_mode.c_str());
            fps_control_mode = PGRSource::FPSControlMode::AUTO;
            configured_fps_control_mode = PGRSource::fpsControlModeName(fps_control_mode);
        }
    }
    else {
        _cfg.add("src_fps_mode", configured_fps_control_mode);
    }

    /// Read source file name.
    string input_fn = _cfg("src_fn");
    if (!src_override.empty()) {
        // override src_fn in config file with cli arg
        input_fn = src_override;
        LOG("Using input_fn=%s", input_fn.c_str());
    } else if (input_fn.empty()) {
        LOG_ERR("Error! No src_fn defined in config file.");
        return;
    }

#if defined(PGR_USB2) || defined(PGR_USB3)
    if (src_override.empty()) {
        try {
            size_t parsed_length = 0;
            std::stoi(input_fn, &parsed_length);
            if (parsed_length == input_fn.size()) {
                input_fn = chooseLiveCamera(input_fn);
                if (input_fn.empty()) {
                    _cancelled = true;
                    LOG_WRN("No live camera selected. Cancelling configuration.");
                    return;
                }
            }
        }
        catch (...) {
        }
    }
#endif // PGR_USB2/3

    /// Open the image source.
    if (!openInputSource(input_fn, first_frame_timeout_ms)) {
        LOG_ERR("Error! Could not open input frame source (%s)!", input_fn.c_str());
        return;
    }

    /// Load the source camera model.
    _w = _source->getWidth();
    _h = _source->getHeight();
    _disp_scl = -1;
    if ((MAX_DISP_DIM > 0) && (std::max(_w,_h) > MAX_DISP_DIM)) {
        _disp_scl = MAX_DISP_DIM / static_cast<float>(std::max(_w,_h));
        _input_data.ptScl = 1.0 / _disp_scl;
    }

    double vfov = 0;
    _cfg.getDbl("vfov", vfov);

    if (vfov <= 0) {
        LOG_ERR("Error! vfov parameter must be > 0 (%f)", vfov);
        return;
    }

    LOG("Using vfov: %f deg", vfov);

    bool fisheye = false;
    if (_cfg.getBool("fisheye", fisheye) && fisheye) {
        _cam_model = CameraModel::createFisheye(_w, _h, vfov * CM_D2R / (double)_h, 360 * CM_D2R);
    }
    else {
        // default to rectilinear
        _cam_model = CameraModel::createRectilinear(_w, _h, vfov * CM_D2R);
    }

    /// Create base file name for output files.
    _base_fn = _cfg("output_fn");
    if (_base_fn.empty()) {
        if (_source->isLive()) {
            _base_fn = "fictrac";
        } else {
            _base_fn = input_fn.substr(0, input_fn.length() - 4);
        }
    }
}

///
/// Destructor.
///
ConfigGui::~ConfigGui()
{}

bool ConfigGui::openInputSource(const string& input_fn, int first_frame_timeout_ms)
{
#if defined(PGR_USB2) || defined(PGR_USB3)
    try {
        size_t parsed_length = 0;
        int id = std::stoi(input_fn, &parsed_length);
        if (parsed_length == input_fn.size()) {
            PGRSource::FPSControlMode fps_control_mode = PGRSource::FPSControlMode::AUTO;
            string configured_fps_control_mode = PGRSource::fpsControlModeName(fps_control_mode);
            if (_cfg.getStr("src_fps_mode", configured_fps_control_mode)) {
                if (!PGRSource::tryParseFPSControlMode(configured_fps_control_mode, fps_control_mode)) {
                    LOG_WRN("Unrecognized src_fps_mode (%s); defaulting to auto.", configured_fps_control_mode.c_str());
                    fps_control_mode = PGRSource::FPSControlMode::AUTO;
                }
            }
            _source = std::make_shared<PGRSource>(id, static_cast<long int>(first_frame_timeout_ms), fps_control_mode);
        }
        else {
            _source = std::make_shared<CVSource>(input_fn);
        }
    }
    catch (...) {
        _source = std::make_shared<CVSource>(input_fn);
    }
#else // !PGR_USB2/3
    _source = std::make_shared<CVSource>(input_fn);
#endif // PGR_USB2/3
    return _source && _source->isOpen();
}

#if defined(PGR_USB2) || defined(PGR_USB3)
string ConfigGui::chooseLiveCamera(const string& initial_input_fn)
{
    vector<string> cameras = PGRSource::describeAvailableCameras();
    if (cameras.empty()) {
        LOG_ERR("Error! Could not find any connected PGR cameras!");
        return string();
    }

    int initial_index = 0;
    try {
        initial_index = std::stoi(initial_input_fn);
    }
    catch (...) {
        initial_index = 0;
    }

    const int selected_index = waitForListSelection("Choose Live Camera", cameras, initial_index, {
        "Select the camera to use for FicTrac configuration.",
        "This picker replaces the terminal camera prompt.",
        "The selected camera is used for this config session."
    });
    if (selected_index < 0) {
        return string();
    }
    return std::to_string(selected_index);
}
#endif // PGR_USB2/3

///
/// Write camera-animal transform to config file.
/// Warning: input R+t is animal to camera frame transform!
///
bool ConfigGui::saveC2ATransform(const string& ref_str, const Mat& R, const Mat& t)
{
	// dump corner points to config file
	vector<int> cfg_pts;
	for (auto p : _input_data.sqrPts) {
		cfg_pts.push_back(static_cast<int>(p.x + 0.5));		// these are just ints in a double object anyway
		cfg_pts.push_back(static_cast<int>(p.y + 0.5));
	}

	// write to config file
	LOG("Adding c2a_src and %s to config file and writing to disk (%s) ..", ref_str.c_str(), _config_fn.c_str());
    _cfg.add("c2a_src", ref_str);
    _cfg.add(ref_str, cfg_pts);

	// dump R to config file
	vector<double> cfg_r, cfg_t;
	CmPoint angleAxis = CmPoint64f::matrixToOmega(R.t());   // transpose to get camera-animal transform
	for (int i = 0; i < 3; i++) {
		cfg_r.push_back(angleAxis[i]);
		cfg_t.push_back(t.at<double>(i, 0));
	}

	// write to config file
	LOG("Adding c2a_r and c2a_t to config file and writing to disk (%s) ..", _config_fn.c_str());
	_cfg.add("c2a_r", cfg_r);
	_cfg.add("c2a_t", cfg_t);

	if (_cfg.write() <= 0) {
		LOG_ERR("Bad write!");
		return false;
	}

	//// test read
	//LOG_DBG("Re-loading config file and reading %s, c2a_r, c2a_t ..", sqr_type.c_str());
	//_cfg.read(_config_fn);

	//if (!_cfg.getVecInt(sqr_type, cfg_pts) || !_cfg.getVecDbl("c2a_r", cfg_r) || !_cfg.getVecDbl("c2a_t", cfg_t)) {
	//	LOG_ERR("Bad read!");
	//	return false;
	//}

	return true;
}

///
/// Update animal coordinate frame estimate.
///
bool ConfigGui::updateRt(const string& ref_str, Mat& R, Mat& t)
{
    //FIXME: also support edge clicks! e.g.:
    //  double x1 = click[2 * i + 0].x;     double y1 = click[2 * i + 0].y;
    //  double x2 = click[2 * i + 1].x;     double y2 = click[2 * i + 1].y;
    //  double x3 = click[2 * i + 2].x;     double y3 = click[2 * i + 2].y;
    //  double x4 = click[2 * i + 3].x;     double y4 = click[2 * i + 3].y;
    //  double px = ((x1*y2 - y1 * x2)*(x3 - x4) - (x1 - x2)*(x3*y4 - y3 * x4)) / ((x1 - x2)*(y3 - y4) - (y1 - y2)*(x3 - x4));
    //  double py = ((x1*y2 - y1 * x2)*(y3 - y4) - (y1 - y2)*(x3*y4 - y3 * x4)) / ((x1 - x2)*(y3 - y4) - (y1 - y2)*(x3 - x4));

    bool ret = false;
    if (ref_str == "c2a_cnrs_xy") {
        ret = computeRtFromSquare(_cam_model, XY_CNRS, _input_data.sqrPts, R, t);
    } else if (ref_str == "c2a_cnrs_yz") {
        ret = computeRtFromSquare(_cam_model, YZ_CNRS, _input_data.sqrPts, R, t);
    } else if (ref_str == "c2a_cnrs_xz") {
        ret = computeRtFromSquare(_cam_model, XZ_CNRS, _input_data.sqrPts, R, t);
    }
    return ret;
}

/////
///// Draw animal coordinate frame axes.
/////
//void ConfigGui::drawC2ATransform(Mat& disp_frame, const Mat& ref_cnrs, const Mat& R, const Mat& t, const double& r, const CmPoint& c)
//{
//	// make x4 mat for projecting corners
//	Mat T(3, 4, CV_64F);
//	for (int i = 0; i < 4; i++) { t.copyTo(T.col(i)); }
//
//	// project reference corners
//	Mat p = R * ref_cnrs + T;
//
//	// draw re-projected reference corners
//	drawRectCorners(disp_frame, _cam_model, p, Scalar(0, 255, 0));
//
//	// draw re-projected animal axes.
//	if (r > 0) {
//		double scale = 1.0 / tan(r);
//		Mat so = (cv::Mat_<double>(3, 1) << c.x, c.y, c.z) * scale;
//		drawAxes(disp_frame, _cam_model, R, so, Scalar(0, 0, 255));
//	}
//}

///
///
///
void ConfigGui::drawC2ACorners(Mat& disp_frame, const string& ref_str, const Mat& R, const Mat& t)
{
    // make x4 mat for projecting corners
    Mat T(3, 4, CV_64F);
    for (int i = 0; i < 4; i++) { t.copyTo(T.col(i)); }

    Mat ref_cnrs;
    if (ref_str == "c2a_cnrs_xy") {
        ref_cnrs = XY_CNRS;
    } else if (ref_str == "c2a_cnrs_yz") {
        ref_cnrs = YZ_CNRS;
    } else if (ref_str == "c2a_cnrs_xz") {
        ref_cnrs = XZ_CNRS;
    } else {
        return;
    }

    // project reference corners
    Mat p = R * ref_cnrs + T;

    // draw re-projected reference corners
    drawRectCorners(disp_frame, _cam_model, p, Scalar(0, 255, 0));
}

///
///
///
void ConfigGui::drawC2AAxes(Mat& disp_frame, const Mat& R, const Mat& t, const double& r, const CmPoint& c)
{
    // draw re-projected animal axes.
    if (r > 0) {
        double scale = 1.0 / tan(r);
        Mat so = (cv::Mat_<double>(3, 1) << c.x, c.y, c.z) * scale;
        drawAxes(disp_frame, _cam_model, R, so, Scalar(0, 0, 255));
        drawAnimalAxis(disp_frame, _cam_model, R, so, r, Scalar(255, 0, 0));
    }
}

///
/// Utility function for changing state machine state.
///
void ConfigGui::changeState(INPUT_MODE new_state)
{
	_input_data.newEvent = true;
	LOG_DBG("New state: %s", INPUT_MODE_STR[static_cast<int>(new_state)].c_str());
	_input_data.mode = new_state;
}

///
///
///
bool ConfigGui::is_open()
{
    return _source && _source->isOpen();
}

///
/// Run user input program or configuration.
///
bool ConfigGui::run()
{
    if (!is_open()) { return false; }

    /// Interactive window.
    cv::namedWindow("configGUI", cv::WINDOW_AUTOSIZE);
    cv::setMouseCallback("configGUI", onMouseEvent, &_input_data);

    /// If reconfiguring, then delete pre-computed values.
    bool reconfig = false;
    _cfg.getBool("reconfig", reconfig);

    /// Get a frame.
    Mat frame;
    if (!_source->grab(frame)) {
        if (_source->isLive()) {
            LOG_ERR("Error! Could not grab input frame. Live triggered cameras must already be receiving trigger pulses before configGui starts.");
        }
        else {
            LOG_ERR("Error! Could not grab input frame.");
        }
        return false;
    }
    if ((frame.cols != _w) || (frame.rows != _h)) {
        LOG_ERR("Error! Unexpected image size (%dx%d).", frame.cols, frame.rows);
        return false;
    }

    // convert to RGB
    if (frame.channels() == 1) {
        cv::cvtColor(frame, frame, cv::COLOR_GRAY2BGR);
    }

    /// Optionally enhance frame for config
    bool do_enhance = false;
    _cfg.getBool("enh_cfg_disp", do_enhance);
    if (do_enhance) {
        LOG("Enhancing config image ..");
        Mat maximg = frame.clone();
        Mat minimg = frame.clone();
        auto t0 = elapsed_secs();
        while (_source->grab(frame)) {
            for (int i = 0; i < _h; i++) {
                uint8_t* pmin = minimg.ptr(i);
                uint8_t* pmax = maximg.ptr(i);
                const uint8_t* pimg = frame.ptr(i);
                for (int j = 0; j < _w * 3; j++) {
                    uint8_t p = pimg[j];
                    if (p > pmax[j]) { pmax[j] = p; }
                    if (p < pmin[j]) { pmin[j] = p; }
                }
            }

            // Drop out after max 30s (avoid infinite loop when running live)
            auto t1 = elapsed_secs();
            if ((t1 - t0) > 30) { break; }
        }
        frame = maximg - minimg;
    }
    
    /// Display/input loop.
	Mat R, t;
    CmPoint c;
    double r = -1;
    char key = 0;
    string val;
	string c2a_src;
    vector<int> cfg_pts;
    vector<double> cfg_vec;
    vector<vector<int>> cfg_polys;
	changeState(CIRC_INIT);
    const int click_rad = std::max(int(_w/150+0.5), 5);
    Mat disp_frame, zoom_frame(ZOOM_DIM, ZOOM_DIM, CV_8UC3);
    const int scaled_zoom_dim = static_cast<int>(ZOOM_DIM * ZOOM_SCL + 0.5);
    bool open = true;
    bool had_error = false;
    bool user_cancelled = false;
    while (open && (key != 0x1b)) {    // esc
        /// Create frame for drawing.
        //cv::cvtColor(_frame, disp_frame, CV_GRAY2RGB);
        disp_frame = frame.clone();

        // normalise displayed image
        {
            double min, max;
            cv::minMaxLoc(disp_frame, &min, &max);
            disp_frame = (disp_frame - min) * 255 / (max - min);
        }
        
        switch (_input_data.mode)
        {
            /// Check for existing circumference points.
            case CIRC_INIT:
            
                // test read
                cfg_pts.clear();
                if (!reconfig && _cfg.getVecDbl("roi_c", cfg_vec) && _cfg.getDbl("roi_r", r)) {
                    c.copy(cfg_vec.data());
                    LOG_DBG("Found roi_c = [%f %f %f] and roi_r = %f rad.", c[0], c[1], c[2], r);
                    LOG_WRN("Warning! When roi_c and roi_r are specified in the config file, roi_circ will be ignored.\nTo re-compute roi_c and roi_r, please delete these values or set reconfig : y in the config file and reconfigure.");
                } 
                else if (_cfg.getVecInt("roi_circ", cfg_pts)) {

                    /// Load circumference points from config file.
                    _input_data.circPts.clear();
                    for (unsigned int i = 1; i < cfg_pts.size(); i += 2) {
                        _input_data.circPts.push_back(Point2d(cfg_pts[i - 1], cfg_pts[i]));
                    }

                    /// Fit circular FoV to sphere.
                    if (_input_data.circPts.size() >= 3) {
                        if (circleFit_camModel(_input_data.circPts, _cam_model, c, r)) {

                            LOG_DBG("Computed roi_c = [%f %f %f] and roi_r = %f rad from %d roi_circ points.", c[0], c[1], c[2], r, _input_data.circPts.size());

                            // save re-computed values
                            cfg_vec.clear();
                            cfg_vec.push_back(c[0]);
                            cfg_vec.push_back(c[1]);
                            cfg_vec.push_back(c[2]);

                            // write to config file
                            LOG("Adding roi_c and roi_r to config file and writing to disk (%s) ..", _config_fn.c_str());
                            _cfg.add("roi_c", cfg_vec);
                            _cfg.add("roi_r", r);
                            if (_cfg.write() <= 0) {
                                LOG_ERR("Error writing to config file (%s)!", _config_fn.c_str());
                                had_error = true;
                                open = false;  // will cause exit
                            }
                        }
                    }
                }
                else {
                    LOG_DBG("No circumference points or sphere ROI specified in configuration file.");
                    r = -1;
                }
                        
                /// Draw fitted circumference.
                if (r > 0) {
                    drawCircle_camModel(disp_frame, _cam_model, c, r, Scalar(255,0,0), false);
        
                    /// Display.
                    switch (waitForPromptChoice(disp_frame, _disp_scl, "Existing Sphere ROI", {
                        "A sphere ROI was found in the config file.",
                        "Keep it to reuse the saved fit, or reconfigure it from new clicks."
                    }))
                    {
                        case PROMPT_KEEP:
							changeState(IGNR_INIT);
                            break;
                        case PROMPT_RECONFIGURE:
                            break;
                        case PROMPT_CANCEL:
                            user_cancelled = true;
                            open = false;
                            key = 0x1b;
                            break;
                    }
                }
                
                if (_input_data.mode == CIRC_INIT) {
                    _input_data.circPts.clear();
					changeState(CIRC_PTS);
                }
                break;
            
            /// Input circumference points.
            case CIRC_PTS:

                /// Fit circular FoV to sphere.
                if (_input_data.newEvent) {
                    if (_input_data.circPts.size() >= 3) {
                        circleFit_camModel(_input_data.circPts, _cam_model, c, r);
                    } else {
                        r = -1;
                    }
                    _input_data.newEvent = false;
                }
                
                /// Draw previous clicks.
                for (auto click : _input_data.circPts) {
                    cv::circle(disp_frame, click, click_rad, Scalar(255,255,0), 1, cv::LINE_AA);
                }
                
                /// Draw fitted circumference.
                if (r > 0) { drawCircle_camModel(disp_frame, _cam_model, c, r, Scalar(255,0,0), false); }
                
                /// Draw cursor location.
                drawCursor(disp_frame, _input_data.cursorPt, Scalar(0,255,0));
                
                /// Create zoomed window.
                createZoomROI(zoom_frame, disp_frame, _input_data.cursorPt, scaled_zoom_dim);
                
                /// Display.
                cv::imshow("zoomROI", zoom_frame);
                showConfigGuiFrame(disp_frame, _disp_scl, {
                    string("Sphere ROI  points ") + std::to_string(_input_data.circPts.size()),
                    "L add  R undo  Enter accept  H help  Esc cancel"
                }, "Sphere ROI", {
                    "Left click adds a circumference point.",
                    "Right click removes the most recent point.",
                    "Pick at least 3 points. Six or more usually fits better.",
                    "Avoid the visible rim where the true ball edge is occluded."
                }, OVERLAY_TOP_LEFT, 0.28);
                key = cv::waitKeyEx(5);
                if (handleOverlayToggleKey(key)) {
                    break;
                }
                
                /// State machine logic.
                if ((key == 0x0d) || (key == 0x0a)) {   // return
                    if (_input_data.circPts.size() >= 3) {
                        // dump circumference points, c, and r to config file
                        cfg_pts.clear();
                        for (auto p : _input_data.circPts) {
                            cfg_pts.push_back(static_cast<int>(p.x + 0.5));
                            cfg_pts.push_back(static_cast<int>(p.y + 0.5));
                        }

                        cfg_vec.clear();
                        cfg_vec.push_back(c[0]);
                        cfg_vec.push_back(c[1]);
                        cfg_vec.push_back(c[2]);
                        
                        // write to config file
                        LOG("Adding roi_circ, roi_c, and roi_r to config file and writing to disk (%s) ..", _config_fn.c_str());
                        _cfg.add("roi_circ", cfg_pts);
                        _cfg.add("roi_c", cfg_vec);
                        _cfg.add("roi_r", r);
                        if (_cfg.write() <= 0) {
                            LOG_ERR("Error writing to config file (%s)!", _config_fn.c_str());
                            had_error = true;
                            open = false;  // will cause exit
                        }
                        
                        //// test read
                        //LOG_DBG("Re-loading config file and reading roi_circ ..");
                        //_cfg.read(_config_fn);
                        //assert(_cfg.getVecInt("roi_circ", cfg_pts));
                        
                        // advance state
                        cv::destroyWindow("zoomROI");
						changeState(IGNR_INIT);
                    } else {
                        LOG_WRN("You must select at least 3 circumference points (you have selected %d points)!", _input_data.circPts.size());
                    }
                }
                break;
            
            /// Check for existing ignore points.
            case IGNR_INIT:
                
                // test read
                cfg_polys.clear();
                if (_cfg.getVVecInt("roi_ignr", cfg_polys)) {
                    
                    /// Load ignore polys from config file.
                    _input_data.ignrPts.clear();
                    for (auto poly : cfg_polys) {
                        vector<cv::Point2d> tmp;
                        for (unsigned int i = 1; i < poly.size(); i+=2) {
                            tmp.push_back(cv::Point2d(poly[i-1],poly[i]));
                        }
                        if (!tmp.empty()) { _input_data.ignrPts.push_back(tmp); }
                    }
                    
                    /// Draw previous clicks.
                    for (unsigned int i = 0; i < _input_data.ignrPts.size(); i++) {
                        for (unsigned int j = 0; j < _input_data.ignrPts[i].size(); j++) {
                            if (i == _input_data.ignrPts.size()-1) {
                                cv::circle(disp_frame, _input_data.ignrPts[i][j], click_rad, COLOURS[i%NCOLOURS], 1, cv::LINE_AA);
                            }
                            cv::line(disp_frame, _input_data.ignrPts[i][j], _input_data.ignrPts[i][(j+1)%_input_data.ignrPts[i].size()], COLOURS[i%NCOLOURS], 1, cv::LINE_AA);
                        }
                    }
                    
                    /// Display.
                    switch (waitForPromptChoice(disp_frame, _disp_scl, "Existing Ignore Regions", {
                        "Ignore regions were found in the config file.",
                        "Keep them to reuse the saved polygons, or redraw them now."
                    }))
                    {
                        case PROMPT_KEEP:
							changeState(R_INIT);
                            break;
                        case PROMPT_RECONFIGURE:
                            break;
                        case PROMPT_CANCEL:
                            user_cancelled = true;
                            open = false;
                            key = 0x1b;
                            break;
                    }
                }
                
                if (_input_data.mode == IGNR_INIT) {
                    _input_data.ignrPts.clear();
					changeState(IGNR_PTS);
                }
                break;
            
            /// Input ignore regions.
            case IGNR_PTS:
                /// Draw previous clicks.
                for (unsigned int i = 0; i < _input_data.ignrPts.size(); i++) {
                    for (unsigned int j = 0; j < _input_data.ignrPts[i].size(); j++) {
                        if (i == _input_data.ignrPts.size()-1) {
                            cv::circle(disp_frame, _input_data.ignrPts[i][j], click_rad, COLOURS[i%NCOLOURS], 1, cv::LINE_AA);
                        }
                        cv::line(disp_frame, _input_data.ignrPts[i][j], _input_data.ignrPts[i][(j+1)%_input_data.ignrPts[i].size()], COLOURS[i%NCOLOURS], 1, cv::LINE_AA);
                    }
                }
                
                /// Draw fitted circumference.
                if (r > 0) { drawCircle_camModel(disp_frame, _cam_model, c, r, Scalar(255,0,0), false); }
                
                /// Draw cursor location.
                drawCursor(disp_frame, _input_data.cursorPt, Scalar(0,255,0));
                
                /// Create zoomed window.
                createZoomROI(zoom_frame, disp_frame, _input_data.cursorPt, scaled_zoom_dim);
                
                /// Display.
                cv::imshow("zoomROI", zoom_frame);
                showConfigGuiFrame(disp_frame, _disp_scl, {
                    string("Ignore Regions  polys ") + std::to_string(_input_data.ignrPts.size()),
                    "L add  R undo  Enter next/finish  H help  Esc cancel"
                }, "Ignore Regions", {
                    "Left click adds points to the active polygon.",
                    "Right click removes the most recent point.",
                    "Press Enter to start a new polygon.",
                    "Press Enter on an empty polygon to finish ignore regions."
                }, OVERLAY_TOP_LEFT, 0.28);
                key = cv::waitKeyEx(5);
                if (handleOverlayToggleKey(key)) {
                    break;
                }
                
                /// State machine logic.
                if ((key == 0x0d) || (key == 0x0a)) {  // return
                    // if current poly is empty, assume we've finished
                    if (_input_data.ignrPts.empty() || _input_data.ignrPts.back().empty()) {
                        if (!_input_data.ignrPts.empty()) { _input_data.ignrPts.pop_back(); }
                        
                        // dump ignore region polys to config file
                        cfg_polys.clear();
                        for (auto poly : _input_data.ignrPts) {
                            cfg_polys.push_back(vector<int>());
                            for (auto pt : poly) {
                                cfg_polys.back().push_back(static_cast<int>(pt.x + 0.5));
                                cfg_polys.back().push_back(static_cast<int>(pt.y + 0.5));
                            }
                        }
                        
                        // write to config file
                        LOG("Adding roi_ignr to config file and writing to disk (%s) ..", _config_fn.c_str());
                        _cfg.add("roi_ignr", cfg_polys);
                        if (_cfg.write() <= 0) {
                            LOG_ERR("Error writing to config file (%s)!", _config_fn.c_str());
                            had_error = true;
                            open = false;      // will cause exit
                        }
                        
                        //// test read
                        //LOG_DBG("Re-loading config file and reading roi_ignr ..");
                        //_cfg.read(_config_fn);
                        //assert(_cfg.getVVecInt("roi_ignr", cfg_polys));
                        
                        // advance state
                        cv::destroyWindow("zoomROI");
						changeState(R_INIT);
                    }
                    // otherwise, start a new poly
                    else {
                        _input_data.addPoly();
                        LOG("New ignore region added!");
                    }
                }
                break;
            
            /// Choose method for defining animal frame.
            case R_INIT:
                /// Check if corners specified (optional).
                _input_data.sqrPts.clear();
                if (_cfg.getStr("c2a_src", c2a_src)) {
                    LOG_DBG("Found c2a_src: %s", c2a_src.c_str());

                    /// Load square corners from config file.
                    cfg_pts.clear();
                    if (_cfg.getVecInt(c2a_src, cfg_pts)) {
                        for (unsigned int i = 1; i < cfg_pts.size(); i += 2) {
                            _input_data.sqrPts.push_back(cv::Point2d(cfg_pts[i - 1], cfg_pts[i]));
                        }
                    }
                }

                /// Load R+t transform from config file.
                R.release();    // clear mat
                cfg_vec.clear();
                if (!reconfig && _cfg.getVecDbl("c2a_r", cfg_vec)) {
                    LOG_DBG("Read c2a_r = [%f %f %f]", cfg_vec[0], cfg_vec[1], cfg_vec[2]);
                    R = CmPoint64f::omegaToMatrix(CmPoint(cfg_vec[0], cfg_vec[1], cfg_vec[2])).t();     // transpose to lab-camera transform
                }
                else {
                    LOG_WRN("Warning! c2a_r missing from config file. Looking for corner points..");
                }

                t.release();    // clear mat
                cfg_vec.clear();
                if (!reconfig && _cfg.getVecDbl("c2a_t", cfg_vec)) {
                    LOG_DBG("Read c2a_t = [%f %f %f]", cfg_vec[0], cfg_vec[1], cfg_vec[2]);
                    t = (cv::Mat_<double>(3, 1) << cfg_vec[0], cfg_vec[1], cfg_vec[2]);
                }
                else {
                    LOG_WRN("Warning! c2a_t missing from config file. Looking for corner points..");
                }

                if (R.empty() || t.empty()) {
                    if (!_input_data.sqrPts.empty()) {
                        LOG_DBG("Recomputing R+t from specified corner points...");

                        /// Recompute R+t
                        if (updateRt(c2a_src, R, t)) {
                            saveC2ATransform(c2a_src, R, t);
                        }
                    }
                }
                else {
                    LOG_WRN("Warning! When c2a_r and c2a_t are specified in the config file, c2a_src and associated corners points will be ignored.\nTo re-compute c2a_r and c2a_t, please delete these values or set reconfig : y in the config file and reconfigure.");
                }

                /// If c2a_r/t missing and couldn't re-compute from specified corners points.
                if (R.empty() || t.empty()) {
                    LOG_ERR("Error! Could not read or compute c2a_r and/or c2a_t. Re-running configuration..");
                    changeState(R_SLCT);
                    break;
                }

                /// Draw previous clicks.
                for (auto click : _input_data.sqrPts) {
                    cv::circle(disp_frame, click, click_rad, Scalar(255, 255, 0), 1, cv::LINE_AA);
                }

                /// Draw reference corners.
                drawC2ACorners(disp_frame, c2a_src, R, t);

                /// Draw axes.
                drawC2AAxes(disp_frame, R, t, r, c);

				/// Display.
                switch (waitForPromptChoice(disp_frame, _disp_scl, "Existing Camera-Animal Transform", {
                    "A saved camera-animal transform was found in the config file.",
                    "Keep it to reuse the current calibration, or reconfigure it now."
                }))
                {
                    case PROMPT_KEEP:
                        changeState(EXIT);
                        break;
                    case PROMPT_RECONFIGURE:
                        break;
                    case PROMPT_CANCEL:
                        user_cancelled = true;
                        open = false;
                        key = 0x1b;
                        break;
                }

				if (_input_data.mode == R_INIT) {
					changeState(R_SLCT);
				}
				break;
            
            /// Choose method for defining animal frame.
            case R_SLCT:
                switch (waitForMethodSelection(disp_frame, _disp_scl))
                {
                    case 1:
                        c2a_src = "c2a_cnrs_xy";
						changeState(R_XY);
                        break;
                    case 2:
                        c2a_src = "c2a_cnrs_yz";
						changeState(R_YZ);
                        break;
                    case 3:
                        c2a_src = "c2a_cnrs_xz";
						changeState(R_XZ);
                        break;
                    case 5:
                        c2a_src = "ext";
						changeState(R_EXT);
                        break;
                    default:
                        user_cancelled = true;
                        open = false;
                        key = 0x1b;
                        break;
                }
                break;
            
            /// Define animal coordinate frame.
            case R_XY:
            
                /// Draw previous clicks.
                for (auto click : _input_data.sqrPts) {
                    cv::circle(disp_frame, click, click_rad, Scalar(255,255,0), 1, cv::LINE_AA);
                }
                
                /// Draw axes.
                if (_input_data.sqrPts.size() == 4) {
                    if (_input_data.newEvent) {
                        updateRt(c2a_src, R, t);
                        _input_data.newEvent = false;
                    }
                    drawC2ACorners(disp_frame, c2a_src, R, t);
                    drawC2AAxes(disp_frame, R, t, r, c);
                }
                
                /// Draw cursor location.
                drawCursor(disp_frame, _input_data.cursorPt, Scalar(0,255,0));
                
                /// Create zoomed window.
                createZoomROI(zoom_frame, disp_frame, _input_data.cursorPt, scaled_zoom_dim);
                
                /// Display.
                cv::imshow("zoomROI", zoom_frame);
                showConfigGuiFrame(disp_frame, _disp_scl, {
                    string("Animal Frame XY  corners ") + std::to_string(_input_data.sqrPts.size()),
                    "L add  R undo  F mirror  Enter accept  H help"
                }, "Animal Frame: XY", {
                    "Click corners in order: (+X,-Y), (+X,+Y), (-X,+Y), (-X,-Y).",
                    "If the camera is above the animal: TL, TR, BR, BL.",
                    "If the camera is below the animal: TR, TL, BL, BR."
                }, OVERLAY_TOP_LEFT, 0.30);
                key = cv::waitKeyEx(5);
                if (handleOverlayToggleKey(key)) {
                    break;
                }
                
                /// State machine logic.
                if ((key == 0x0d) || (key == 0x0a)) {   // return
                    if ((_input_data.sqrPts.size() == 4) && !R.empty()) {
                        // dump corner points to config file
						if (!saveC2ATransform(c2a_src, R, t)) {
							LOG_ERR("Error writing coordinate transform to config file!");
                                had_error = true;
                            open = false;      // will cause exit
						}
                        
                        // advance state
                        cv::destroyWindow("zoomROI");
						changeState(EXIT);
                    } else {
                        LOG_WRN("You must select exactly 4 corners (you have selected %d points)!", _input_data.sqrPts.size());
                    }
                } else if (key == 0x66) {   // f
                    /// Reflect R and re-minimise.
                    if (!R.empty()) {
                        R.col(2) *= -1;
                        _input_data.newEvent = true;
                    }
                }
                break;
            
            /// Define animal coordinate frame.
            case R_YZ:
                
                /// Draw previous clicks.
                for (auto click : _input_data.sqrPts) {
                    cv::circle(disp_frame, click, click_rad, Scalar(255,255,0), 1, cv::LINE_AA);
                }
                
                /// Draw axes.
                if (_input_data.sqrPts.size() == 4) {
                    if (_input_data.newEvent) {
                        updateRt(c2a_src, R, t);
                        _input_data.newEvent = false;
                    }
                    drawC2ACorners(disp_frame, c2a_src, R, t);
                    drawC2AAxes(disp_frame, R, t, r, c);
                }
                
                /// Draw cursor location.
                drawCursor(disp_frame, _input_data.cursorPt, Scalar(0,255,0));
                
                /// Create zoomed window.
                createZoomROI(zoom_frame, disp_frame, _input_data.cursorPt, scaled_zoom_dim);
                
                /// Display.
                cv::imshow("zoomROI", zoom_frame);
                showConfigGuiFrame(disp_frame, _disp_scl, {
                    string("Animal Frame YZ  corners ") + std::to_string(_input_data.sqrPts.size()),
                    "L add  R undo  F mirror  Enter accept  H help"
                }, "Animal Frame: YZ", {
                    "Click corners in order: (-Y,-Z), (+Y,-Z), (+Y,+Z), (-Y,+Z).",
                    "If the camera is behind the animal: TL, TR, BR, BL.",
                    "If the camera is in front of the animal: TR, TL, BL, BR."
                }, OVERLAY_TOP_LEFT, 0.30);
                key = cv::waitKeyEx(5);
                if (handleOverlayToggleKey(key)) {
                    break;
                }
                
                /// State machine logic.
                if ((key == 0x0d) || (key == 0x0a)) {   // return
					if ((_input_data.sqrPts.size() == 4) && !R.empty()) {
                        // dump corner points to config file
						if (!saveC2ATransform(c2a_src, R, t)) {
							LOG_ERR("Error writing coordinate transform to config file!");
                                had_error = true;
                            open = false;      // will cause exit
						}
                        
                        // advance state
                        cv::destroyWindow("zoomROI");
						changeState(EXIT);
                    } else {
                        LOG_WRN("You must select exactly 4 corners (you have selected %d points)!", _input_data.sqrPts.size());
                    }
                } else if (key == 0x66) {   // f
                    /// Reflect R and re-minimise.
                    if (!R.empty()) {
                        R.col(2) *= -1;
                        _input_data.newEvent = true;
                    }
                }
                break;
            
            /// Define animal coordinate frame.
            case R_XZ:
                
                /// Draw previous clicks.
                for (auto click : _input_data.sqrPts) {
                    cv::circle(disp_frame, click, click_rad, Scalar(255,255,0), 1, cv::LINE_AA);
                }
                
                /// Draw axes.
                if (_input_data.sqrPts.size() == 4) {
                    if (_input_data.newEvent) {
                        updateRt(c2a_src, R, t);
                        _input_data.newEvent = false;
                    }
                    drawC2ACorners(disp_frame, c2a_src, R, t);
                    drawC2AAxes(disp_frame, R, t, r, c);
                }
                
                /// Draw cursor location.
                drawCursor(disp_frame, _input_data.cursorPt, Scalar(0,255,0));
                
                /// Create zoomed window.
                createZoomROI(zoom_frame, disp_frame, _input_data.cursorPt, scaled_zoom_dim);
                
                /// Display.
                cv::imshow("zoomROI", zoom_frame);
                showConfigGuiFrame(disp_frame, _disp_scl, {
                    string("Animal Frame XZ  corners ") + std::to_string(_input_data.sqrPts.size()),
                    "L add  R undo  F mirror  Enter accept  H help"
                }, "Animal Frame: XZ", {
                    "Click corners in order: (+X,-Z), (-X,-Z), (-X,+Z), (+X,+Z).",
                    "If the camera is to the animal's left: TL, TR, BR, BL.",
                    "If the camera is to the animal's right: TR, TL, BL, BR."
                }, OVERLAY_TOP_LEFT, 0.30);
                key = cv::waitKeyEx(5);
                if (handleOverlayToggleKey(key)) {
                    break;
                }
                
                /// State machine logic.
                if ((key == 0x0d) || (key == 0x0a)) {   // return
					if ((_input_data.sqrPts.size() == 4) && !R.empty()) {
                        // dump corner points to config file
						if (!saveC2ATransform(c2a_src, R, t)) {
							LOG_ERR("Error writing coordinate transform to config file!");
                                had_error = true;
                            open = false;      // will cause exit
						}
                        
                        // advance state
                        cv::destroyWindow("zoomROI");
						changeState(EXIT);
                    } else {
                        LOG_WRN("You must select exactly 4 corners (you have selected %d points)!", _input_data.sqrPts.size());
                    }
                } else if (key == 0x66) {   // f
                    /// Reflect R and re-minimise.
                    if (!R.empty()) {
                        R.col(2) *= -1;
                        _input_data.newEvent = true;
                    }
                }
                break;
            
            // /// Define animal coordinate frame.
            // case R_MAN:
            
                // /// Draw axes.
                
                
                // // draw re-projected animal axes.
                // if (r > 0) {
                    // double scale = 1.0/tan(r);
                    // Mat so = (cv::Mat_<double>(3,1) << c.x, c.y, c.z) * scale;
                    // drawAxes(disp_frame, _cam_model, R, so, Scalar(0,0,255));
                // }
                
                // // advance state
                // BOOST_LOG_TRIVIAL(debug) << "New state: EXIT";
                // _input_data.mode = EXIT;
                // break;
            
            /// Define animal coordinate frame.
            case R_EXT:

                // ensure c2a_r exists in config file
                if (!_cfg.getStr("c2a_r", val)) {
                    cfg_vec.clear();
                    cfg_vec.resize(3, 0);

                    // write to config file
                    LOG("Adding c2a_r to config file and writing to disk (%s) ..", _config_fn.c_str());
                    _cfg.add("c2a_r", cfg_vec);
                }
                _cfg.add("c2a_src", string("ext"));

                if (_cfg.write() <= 0) {
                    LOG_ERR("Error writing to config file (%s)!", _config_fn.c_str());
                    had_error = true;
                    open = false;      // will cause exit
                }
            
                // advance state
				changeState(EXIT);
                break;
            
            default:
                LOG_WRN("Unexpected state encountered!");
                _input_data.mode = EXIT;
                // break;
            
            /// Exit config.
            case EXIT:
                key = 0x1b; // esc
                break;
        }
    }

	cv::destroyAllWindows();

	/// Save config image
	//cv::cvtColor(_frame, disp_frame, CV_GRAY2RGB);
    disp_frame = frame.clone();

	// draw fitted circumference
	if (r > 0) {
		drawCircle_camModel(disp_frame, _cam_model, c, r, Scalar(255, 0, 0), false);
	}

	// draw ignore regions
	for (unsigned int i = 0; i < _input_data.ignrPts.size(); i++) {
		for (unsigned int j = 0; j < _input_data.ignrPts[i].size(); j++) {
			if (i == _input_data.ignrPts.size() - 1) {
				cv::circle(disp_frame, _input_data.ignrPts[i][j], click_rad, COLOURS[i%NCOLOURS], 1, cv::LINE_AA);
			}
			cv::line(disp_frame, _input_data.ignrPts[i][j], _input_data.ignrPts[i][(j + 1) % _input_data.ignrPts[i].size()], COLOURS[i%NCOLOURS], 1, cv::LINE_AA);
		}
	}

	// draw animal coordinate frame
	if (_input_data.sqrPts.size() == 4) {
        drawC2ACorners(disp_frame, c2a_src, R, t);
	}
    drawC2AAxes(disp_frame, R, t, r, c);

	// write image to disk
	string cfg_img_fn = _base_fn + "-configImg.png";
    LOG("Writing config image to disk (%s)..", cfg_img_fn.c_str());
	if (!cv::imwrite(cfg_img_fn, disp_frame)) {
		LOG_ERR("Error writing config image to disk!");
        had_error = true;
	}

    //// compute thresholding priors
    //auto thr_mode = static_cast<THR_MODE>(_cfg.get<int>("thr_mode"));   // 0 = default (adapt); 1 = norm w/ priors
    //if (thr_mode == NORM_PRIORS) {

    //    LOG("Computing ROI thresholding priors ..");

        //// rewind source
        //_source->rewind();

        //vector<vector<uint16_t>> hist;

        //Mat maximg = frame.clone();
        //Mat minimg = frame.clone();
        //auto t0 = elapsed_secs();
        //while (_source->grab(frame)) {
        //    for (int i = 0; i < _h; i++) {
        //        uint8_t* pmin = minimg.ptr(i);
        //        uint8_t* pmax = maximg.ptr(i);
        //        const uint8_t* pimg = frame.ptr(i);
        //        for (int j = 0; j < _w * 3; j++) {
        //            uint8_t p = pimg[j];
        //            if (p > pmax[j]) { pmax[j] = p; }
        //            if (p < pmin[j]) { pmin[j] = p; }
        //        }
        //    }
        
    //        // Drop out after max 30s (avoid infinite loop when running live)
    //        auto t1 = elapsed_secs();
    //        if ((t1 - t0) > 30) { break; }
    //    }
    //    frame = maximg - minimg;
    //}
    _cancelled = user_cancelled;

    if (had_error) {
        LOG_WRN("\n\nWarning! There were errors and the configuration file may not have been properly updated. Please run configuration again.");
    } else if (user_cancelled) {
        LOG("Configuration cancelled by user.");
    } else {
        LOG("Configuration complete!");
    }
    
    LOG("Exiting configuration!");
    return !had_error;
}
