

#include <opencv2/opencv.hpp>
#include <vector>
#include <cmath>
#include <iostream>
#include <string>
#include <chrono>
#include <algorithm>

using namespace cv;
using namespace std;

struct UFLD_Lane
{
    vector<Point> points;
    float confidence;
    int id;
};

struct UFLD_Result
{
    vector<UFLD_Lane> lanes;
    double inference_time;
    bool detected;
    UFLD_Result() : detected(false), inference_time(0) {}
};

class UltraFastLaneDetector
{
private:
    bool model_loaded;
    bool debug_mode;

    float roi_top_ratio = 0.4f;
    float roi_bottom_ratio = 1.0f;

    int canny_low = 50;
    int canny_high = 150;

    int hough_threshold = 50;
    int min_line_length = 50;
    int max_line_gap = 50;

    float min_slope = 0.3f;
    float max_slope = 3.0f;

public:
    UltraFastLaneDetector() : model_loaded(false), debug_mode(false) {}

    bool initialize(const string &model_path = "")
    {
        cout << "Initializing Classical Lane Detection..." << endl;
        cout << "  Using: Edge Detection + Hough Lines" << endl;
        cout << "  No neural network required!" << endl;
        model_loaded = true;
        return true;
    }

    void setDebugMode(bool enable) { debug_mode = enable; }

    UFLD_Result detectLanes(const Mat &frame)
    {
        UFLD_Result result;
        auto start = chrono::high_resolution_clock::now();

        if (frame.empty())
            return result;

        int roi_top = frame.rows * roi_top_ratio;
        int roi_height = frame.rows * (roi_bottom_ratio - roi_top_ratio);
        Rect roi(0, roi_top, frame.cols, roi_height);
        Mat roi_frame = frame(roi);

        Mat gray;
        cvtColor(roi_frame, gray, COLOR_BGR2GRAY);

        Mat blurred;
        GaussianBlur(gray, blurred, Size(5, 5), 0);

        Mat edges;
        Canny(blurred, edges, canny_low, canny_high);

        Mat mask = Mat::zeros(edges.size(), edges.type());
        Point pts[4] = {
            Point(edges.cols * 0.1, edges.rows),
            Point(edges.cols * 0.4, edges.rows * 0.3),
            Point(edges.cols * 0.6, edges.rows * 0.3),
            Point(edges.cols * 0.9, edges.rows)
        };
        fillConvexPoly(mask, pts, 4, Scalar(255));

        Mat masked_edges;
        bitwise_and(edges, mask, masked_edges);

        vector<Vec4i> lines;
        HoughLinesP(masked_edges, lines, 1, CV_PI / 180,
                    hough_threshold, min_line_length, max_line_gap);

        if (debug_mode)
        {
            cout << "Detected " << lines.size() << " raw lines" << endl;
        }

        vector<Vec4i> left_lines, right_lines;
        int center_x = frame.cols / 2;

        for (const auto &line : lines)
        {
            int x1 = line[0], y1 = line[1];
            int x2 = line[2], y2 = line[3];

            float slope = (y2 - y1) / (float)(x2 - x1 + 1e-6);

            if (abs(slope) < min_slope || abs(slope) > max_slope)
                continue;

            int mid_x = (x1 + x2) / 2;
            if (slope < 0 && mid_x < center_x)
            {
                left_lines.push_back(line);
            }
            else if (slope > 0 && mid_x > center_x)
            {
                right_lines.push_back(line);
            }
        }

        result.lanes.clear();

        if (!left_lines.empty())
        {
            UFLD_Lane left_lane = averageLines(left_lines, roi, roi_top);
            left_lane.id = 0;
            left_lane.confidence = min(1.0f, left_lines.size() / 10.0f);
            result.lanes.push_back(left_lane);
        }

        if (!right_lines.empty())
        {
            UFLD_Lane right_lane = averageLines(right_lines, roi, roi_top);
            right_lane.id = 1;
            right_lane.confidence = min(1.0f, right_lines.size() / 10.0f);
            result.lanes.push_back(right_lane);
        }

        result.detected = !result.lanes.empty();

        auto end = chrono::high_resolution_clock::now();
        result.inference_time = chrono::duration<double, milli>(end - start).count();

        if (debug_mode)
        {
            cout << "Left lines: " << left_lines.size()
                 << ", Right lines: " << right_lines.size() << endl;
            cout << "Final lanes: " << result.lanes.size() << endl;
        }

        return result;
    }
    bool checkLaneDeparture(const UFLD_Result &result, const Mat &frame,
                            string &direction, double &deviation)
    {
        if (!result.detected || result.lanes.empty())
        {
            direction = "CENTERED";
            deviation = 0.0;
            return false;
        }

        int center_x = frame.cols / 2;

        const UFLD_Lane *left_lane = nullptr;
        const UFLD_Lane *right_lane = nullptr;

        for (const auto &lane : result.lanes)
        {
            if (lane.points.empty())
                continue;

            int bottom_x = lane.points.back().x;
            if (bottom_x < center_x)
            {
                left_lane = &lane;
            }
            else
            {
                right_lane = &lane;
            }
        }

        if (!left_lane && !right_lane)
        {
            direction = "CENTERED";
            deviation = 0.0;
            return false;
        }

        float left_x = left_lane ? left_lane->points.back().x : 0.0f;
        float right_x = right_lane ? right_lane->points.back().x : (float)frame.cols;

        float lane_center = (left_x + right_x) / 2.0f;
        float lane_width = right_x - left_x;

        if (lane_width < 100.0f)
        {
            direction = "CENTERED";
            deviation = 0.0;
            return false;
        }

        deviation = (center_x - lane_center) / lane_width;

        if (std::abs(deviation) < 0.15)
        {
            direction = "CENTERED";
            return false;
        }
        else if (deviation > 0)
        {
            direction = "RIGHT";
        }
        else
        {
            direction = "LEFT";
            deviation = -deviation;
        }

        return (deviation > 0.25);
    }

    void drawLanes(Mat &frame, const UFLD_Result &result, bool show_details = false)
    {
        if (!result.detected || result.lanes.empty())
        {
            putText(frame, "NO LANES DETECTED",
                    Point(frame.cols / 2 - 120, frame.rows / 2),
                    FONT_HERSHEY_SIMPLEX, 0.8, Scalar(0, 0, 255), 2);
            return;
        }

        vector<Scalar> colors = {
            Scalar(0, 255, 0),
            Scalar(0, 0, 255),
            Scalar(255, 255, 0),
            Scalar(255, 0, 255)
        };

        for (size_t i = 0; i < result.lanes.size(); i++)
        {
            const auto &lane = result.lanes[i];
            if (lane.points.size() < 2)
                continue;

            Scalar color = colors[i % colors.size()];

            for (size_t j = 0; j < lane.points.size() - 1; j++)
            {
                line(frame, lane.points[j], lane.points[j + 1], color, 8, LINE_AA);
            }

            if (show_details && !lane.points.empty())
            {
                string label = "Lane " + to_string(i) +
                               " (" + to_string((int)(lane.confidence * 100)) + "%)";
                putText(frame, label, lane.points[0] + Point(10, -10),
                        FONT_HERSHEY_SIMPLEX, 0.6, color, 2);
            }
        }

        int center_x = frame.cols / 2;
        line(frame, Point(center_x, frame.rows),
             Point(center_x, frame.rows * 2 / 3),
             Scalar(255, 255, 255), 3);

        if (show_details)
        {
            string info = "Classical CV | " + to_string((int)result.inference_time) + "ms";
            putText(frame, info, Point(10, 30),
                    FONT_HERSHEY_SIMPLEX, 0.6, Scalar(0, 255, 0), 2);
        }
    }

    void drawDepartureWarning(Mat &frame, const string &direction,
                              double deviation, bool warning)
    {
        if (!warning)
            return;

        static int pulse = 0;
        pulse = (pulse + 1) % 60;
        float alpha = 0.4f + 0.4f * abs(sin(pulse * 0.1f));

        Mat overlay = frame.clone();
        rectangle(overlay, Point(0, 0), Point(frame.cols, 80),
                  Scalar(0, 0, 255), FILLED);
        addWeighted(frame, 1.0 - alpha, overlay, alpha, 0, frame);

        string text = "⚠ LANE DEPARTURE: " + direction + " (" +
                      to_string((int)(deviation * 100)) + "%)";

        Point pos(frame.cols / 2 - 220, 50);
        putText(frame, text, pos + Point(2, 2),
                FONT_HERSHEY_DUPLEX, 1.0, Scalar(0, 0, 0), 3);
        putText(frame, text, pos,
                FONT_HERSHEY_DUPLEX, 1.0, Scalar(255, 255, 255), 2);
    }

private:
    UFLD_Lane averageLines(const vector<Vec4i> &lines, const Rect &roi, int roi_offset)
    {
        UFLD_Lane lane;

        if (lines.empty())
            return lane;

        vector<Point> all_points;
        for (const auto &line : lines)
        {
            all_points.push_back(Point(line[0], line[1]));
            all_points.push_back(Point(line[2], line[3]));
        }

        Vec4f fitted_line;
        fitLine(all_points, fitted_line, DIST_L2, 0, 0.01, 0.01);

        float vx = fitted_line[0];
        float vy = fitted_line[1];
        float x0 = fitted_line[2];
        float y0 = fitted_line[3];

        int y_start = 0;
        int y_end = roi.height;
        int num_points = 20;

        for (int i = 0; i < num_points; i++)
        {
            float y = y_start + (y_end - y_start) * i / (float)num_points;
            float t = (y - y0) / (vy + 1e-6);
            float x = x0 + vx * t;

            int full_x = (int)x;
            int full_y = (int)y + roi_offset;

            if (full_x >= 0 && full_x < roi.width && full_y >= roi_offset)
            {
                lane.points.push_back(Point(full_x, full_y));
            }
        }

        return lane;
    }
};

