// main.cpp - FIXED: Stable capture with proper error handling
#include "camera.h"
#include "lane_detector.h"

#include <iostream>
#include <thread>
#include <atomic>
#include <memory>
#include <queue>
#include <mutex>
#include <chrono>
#include <iomanip>
#include <sstream>
#include <csignal>
#include <condition_variable>

using namespace cv;
using namespace std;
using namespace chrono;

// Global flag for signal handling
atomic<bool> g_should_exit(false);

void signalHandler(int signum)
{
    cout << "\nReceived signal " << signum << ", shutting down gracefully..." << endl;
    g_should_exit = true;
}

// Enhanced FPS counter with smoothing
class FPSCounter
{
private:
    steady_clock::time_point last_time;
    int frame_count;
    double current_fps;
    deque<double> fps_history;
    const size_t history_size = 10;

public:
    FPSCounter() : frame_count(0), current_fps(0.0)
    {
        last_time = steady_clock::now();
    }

    double update()
    {
        frame_count++;
        auto current_time = steady_clock::now();
        auto elapsed = duration_cast<milliseconds>(current_time - last_time).count();

        if (elapsed >= 1000)
        {
            double fps = frame_count * 1000.0 / elapsed;
            fps_history.push_back(fps);

            if (fps_history.size() > history_size)
            {
                fps_history.pop_front();
            }

            current_fps = 0;
            for (double f : fps_history)
            {
                current_fps += f;
            }
            current_fps /= fps_history.size();

            frame_count = 0;
            last_time = current_time;
        }
        return current_fps;
    }

    double getFPS() const { return current_fps; }
    void reset()
    {
        frame_count = 0;
        current_fps = 0.0;
        fps_history.clear();
        last_time = steady_clock::now();
    }
};

struct SystemConfig
{
    enum CameraMode
    {
        CAMERA_AUTO_DETECT,
        CAMERA_PHONE_USB,
        CAMERA_PHONE_WIFI,
        CAMERA_WEBCAM
    };

    string lane_model = "tusimple_res18";

    CameraMode camera_mode = CAMERA_AUTO_DETECT;
    string camera_source = "auto";
    int camera_width = 640; // REDUCED for stability
    int camera_height = 480;
    int target_fps = 30;

    string yolo_model_path = "../../models/yolov8n.onnx";
    string landmarks_model_path = "../../models/shape_predictor_68_face_landmarks.dat";

    bool enable_lane_detection = true;
    bool enable_object_detection = true;
    bool enable_drowsiness_detection = true;

    bool use_gpu = false;
    int processing_interval = 1;
    int detection_interval = 1; // INCREASED to reduce load

    string phone_ip = "192.168.18.76";
    int phone_port = 4747;

    bool show_fps = true;
    bool show_processing_times = true;
    bool fullscreen = false;

    bool use_v4l2 = true;
    bool use_mjpeg = true;
    int v4l2_buffer_size = 30; // Minimal buffers

    bool enable_usb_gps = true;
    int gps_usb_port = 5555;
    bool enable_gps_alerts = true;

    float rapid_accel_threshold = 3.0f;
    float hard_brake_threshold = -4.0f;
    float emergency_brake_threshold = -6.0f;
    float aggressive_accel_threshold = 4.5f;
};

template <typename T>
class ThreadSafeQueue
{
private:
    queue<T> queue_;
    mutable mutex mutex_;
    condition_variable cond_;
    size_t max_size_;

public:
    ThreadSafeQueue(size_t max_size = 2) : max_size_(max_size) {}

    void push(T value)
    {
        lock_guard<mutex> lock(mutex_);
        while (queue_.size() >= max_size_)
        {
            queue_.pop();
        }
        queue_.push(move(value));
        cond_.notify_one();
    }

    bool try_pop(T &value)
    {
        lock_guard<mutex> lock(mutex_);
        if (queue_.empty())
        {
            return false;
        }
        value = move(queue_.front());
        queue_.pop();
        return true;
    }

    bool wait_and_pop(T &value, int timeout_ms = 100)
    {
        unique_lock<mutex> lock(mutex_);
        if (!cond_.wait_for(lock, milliseconds(timeout_ms),
                            [this]
                            { return !queue_.empty(); }))
        {
            return false;
        }
        value = move(queue_.front());
        queue_.pop();
        return true;
    }

    size_t size() const
    {
        lock_guard<mutex> lock(mutex_);
        return queue_.size();
    }

    bool empty() const
    {
        lock_guard<mutex> lock(mutex_);
        return queue_.empty();
    }

    void clear()
    {
        lock_guard<mutex> lock(mutex_);
        queue<T> empty;
        swap(queue_, empty);
    }
};

class SmartDriveManager
{
private:
    SystemConfig config;
    bool phone_camera_used;
    string actual_camera_device;

    unique_ptr<CameraManager> camera;
    unique_ptr<UltraFastLaneDetector> lane_detector;


    atomic<bool> running;
    atomic<bool> processing_active;

    ThreadSafeQueue<Mat> capture_queue;
    ThreadSafeQueue<Mat> display_queue;

    thread processing_thread;
    thread capture_thread;

    FPSCounter total_fps_counter;
    FPSCounter capture_fps_counter;
    FPSCounter display_fps_counter;

    atomic<double> lane_processing_time;
    atomic<double> yolo_processing_time;
    atomic<double> drowsiness_processing_time;
    atomic<double> total_processing_time;

    atomic<int> frame_count;

    atomic<bool> lane_departure_alert;
    atomic<bool> drowsiness_alert;
    atomic<bool> collision_warning;

    atomic<bool> acceleration_alert;
    atomic<bool> braking_alert;

    double simulated_target_speed;
    bool simulation_running;

    struct PerformanceStats
    {
        double avg_latency = 0;
        double max_latency = 0;
        int dropped_frames = 0;
        steady_clock::time_point start_time;
    } perf_stats;

public:
    SmartDriveManager(const SystemConfig &cfg)
        : config(cfg),
          phone_camera_used(false),
          running(false),
          processing_active(false),
          capture_queue(2),
          display_queue(2),
          frame_count(0),
          lane_processing_time(0),
          simulated_target_speed(0),
          simulation_running(false),
          acceleration_alert(false),
          braking_alert(false),
          yolo_processing_time(0),
          drowsiness_processing_time(0),
          total_processing_time(0),
          lane_departure_alert(false),
          drowsiness_alert(false),
          collision_warning(false)
    {
        perf_stats.start_time = steady_clock::now();
    }

    ~SmartDriveManager()
    {
        stop();
    }

    bool initialize()
    {
        printHeader();
        cout << "Initializing Smart Drive Manager..." << endl;

        if (!initializeCamera())
        {
            cerr << "Failed to initialize camera" << endl;
            return false;
        }

        // Wait for camera to stabilize
        cout << "Waiting for camera to stabilize..." << endl;
        this_thread::sleep_for(chrono::seconds(2));

        /*
            if (config.enable_object_detection)
        {
            cout << "[1/5] Initializing object detection..." << endl;
            yolo_detector = make_unique<YOLOv8>();

            ifstream model_file(config.yolo_model_path);
            if (!model_file.good())
            {
                cerr << "  ✗ Model file not found: " << config.yolo_model_path << endl;
                config.enable_object_detection = false;
                cout << "  ⚠ Continuing without object detection" << endl;
            }
            else if (!yolo_detector->initialize(config.yolo_model_path, config.use_gpu))
            {
                cerr << "  ✗ Failed to initialize YOLOv8" << endl;
                config.enable_object_detection = false;
                cout << "  ⚠ Continuing without object detection" << endl;
            }
            else
            {
                cout << "  ✓ YOLOv8 initialized" << endl;
            }
        }

        if (config.enable_drowsiness_detection)
        {
            cout << "[2/5] Initializing drowsiness detection..." << endl;
            drowsiness_detector = make_unique<DrowsinessDetector>();

            ifstream model_file(config.landmarks_model_path);
            if (!model_file.good())
            {
                cerr << "  ✗ Model file not found: " << config.landmarks_model_path << endl;
                config.enable_drowsiness_detection = false;
                cout << "  ⚠ Continuing without drowsiness detection" << endl;
            }
            else if (!drowsiness_detector->initialize(config.landmarks_model_path))
            {
                cerr << "  ✗ Failed to initialize drowsiness detector" << endl;
                config.enable_drowsiness_detection = false;
                cout << "  ⚠ Continuing without drowsiness detection" << endl;
            }
            else
            {
                cout << "  ✓ Drowsiness detector initialized" << endl;
            }
        }
        */

        if (config.enable_lane_detection)
        {
            cout << "[3/5] Initializing Ultra-Fast Lane Detection..." << endl;
            lane_detector = make_unique<UltraFastLaneDetector>();

            string model_path;
            if (config.lane_model == "tusimple_res18")
            {
                model_path = "../../models/ufldv2_tusimple_res18_320x800.onnx";
            }
            else if (config.lane_model == "tusimple_res34")
            {
                model_path = "../../models/ufldv2_tusimple_res34_320x800.onnx";
            }
            else if (config.lane_model == "culane_res18")
            {
                model_path = "../../models/ufldv2_culane_res18_320x1600.onnx";
            }
            else if (config.lane_model == "culane_res34")
            {
                model_path = "../../models/ufldv2_culane_res34_320x1600.onnx";
            }
            else
            {
                model_path = "../../models/ufldv2_tusimple_res18_320x800.onnx";
            }

            if (!lane_detector->initialize(model_path))
            {
                cerr << "  ✗ Failed to initialize UFLD lane detector" << endl;
                config.enable_lane_detection = false;
                cout << "  ⚠ Continuing without lane detection" << endl;
            }
            else
            {
                cout << "  ✓ UFLD lane detector initialized" << endl;
                cout << "  Model: " << model_path << endl;
                lane_detector->setDebugMode(true);
            }
        }
        /*

        cout << "[4/5] Initializing acceleration detector..." << endl;
        accel_detector = make_unique<AccelerationDetector>();

        accel_detector->initialize(
            config.rapid_accel_threshold,
            config.hard_brake_threshold,
            config.aggressive_accel_threshold,
            config.emergency_brake_threshold);
        accel_detector->setMinimumSpeed(5.0);

        gps_logger = make_unique<GPSLogger>("driving_log.csv");
        gps_logger->enableLogging(true);

        if (config.enable_usb_gps)
        {
            cout << "[5/5] Setting up USB GPS connection..." << endl;
            if (accel_detector->connectToPhoneGPS_USB(config.gps_usb_port))
            {
                cout << "  ✓ USB GPS connected via same USB cable!" << endl;
                simulation_running = false;
            }
            else
            {
                cout << "  ⚠ USB GPS failed, using simulation mode" << endl;
                accel_detector->enableSimulation(true);
                simulation_running = true;
            }
        }
        else
        {
            cout << "  ⚠ GPS disabled, using simulation mode" << endl;
            accel_detector->enableSimulation(true);
            simulation_running = true;
        }

        cout << "  ✓ Acceleration detector initialized" << endl;*/

        printSystemReady();
        return true;
    }

    void start()
    {
        running = true;
        processing_active = true;

        capture_thread = thread(&SmartDriveManager::captureLoop, this);
        processing_thread = thread(&SmartDriveManager::processingLoop, this);

        displayLoop();

        if (processing_thread.joinable())
        {
            processing_thread.join();
        }
        if (capture_thread.joinable())
        {
            capture_thread.join();
        }
    }

    void stop()
    {
        running = false;
        processing_active = false;

        if (camera)
        {
            camera->release();
        }

        capture_queue.clear();
        display_queue.clear();

        printPerformanceStats();
    }

private:
    void printHeader()
    {
        cout << "╔════════════════════════════════════╗" << endl;
        cout << "║   SMART DRIVE MANAGER              ║" << endl;
        cout << "║   Data Structures Project          ║" << endl;
        cout << "║   BSCS-24063 - Mubeen Butt         ║" << endl;
        cout << "╚════════════════════════════════════╝" << endl;
        cout << endl;
    }

    void printSystemReady()
    {
        cout << endl;
        cout << "✓ All components initialized successfully" << endl;
        cout << "╔════════════════════════════════════╗" << endl;
        cout << "║   SYSTEM READY                     ║" << endl;
        cout << "╚════════════════════════════════════╝" << endl;
        cout << endl;
        cout << "Camera: " << (phone_camera_used ? "📱 Phone Camera (DroidCam)" : "🎥 Webcam") << endl;
        cout << "Device: " << actual_camera_device << endl;
        cout << "Resolution: " << config.camera_width << "x" << config.camera_height << endl;
        cout << "Features enabled:" << endl;
        cout << "  - Lane Detection: " << (config.enable_lane_detection ? "✓ YES" : "✗ NO") << endl;
        cout << "  - Object Detection: " << (config.enable_object_detection ? "✓ YES" : "✗ NO") << endl;
        cout << "  - Drowsiness Detection: " << (config.enable_drowsiness_detection ? "✓ YES" : "✗ NO") << endl;
        cout << "  - Acceleration Detection: " << (config.enable_usb_gps ? "✓ YES (USB GPS)" : "✓ YES (Simulation)") << endl;
        cout << endl;
        printControls();
    }

    void printControls()
    {
        cout << "Controls:" << endl;
        cout << "  ESC    - Exit program" << endl;
        cout << "  F      - Toggle fullscreen" << endl;
        cout << "  S      - Save screenshot" << endl;
        cout << "  P      - Pause/Resume processing" << endl;
        cout << "  R      - Reset performance stats" << endl;
        cout << "  H      - Show/Hide FPS" << endl;
        cout << "  1/2/3  - Toggle lane/object/drowsiness detection" << endl;
        cout << "  4      - Reset acceleration statistics" << endl;
        cout << "  5      - Show driving statistics" << endl;
        cout << "  A/Z    - Accelerate/Brake (simulation)" << endl;
        cout << "  X      - Emergency brake (simulation)" << endl;
        cout << "  +/-    - Adjust detection interval" << endl;
        cout << endl;
        cout << "Press ESC to exit at any time" << endl;
        cout << "════════════════════════════════════" << endl;
    }

    void printPerformanceStats()
    {
        auto elapsed = duration_cast<seconds>(
                           steady_clock::now() - perf_stats.start_time)
                           .count();

        cout << endl;
        cout << "╔════════════════════════════════════╗" << endl;
        cout << "║   PERFORMANCE STATISTICS           ║" << endl;
        cout << "╚════════════════════════════════════╝" << endl;
        cout << "Runtime: " << elapsed << " seconds" << endl;
        cout << "Average Latency: " << fixed << setprecision(2)
             << perf_stats.avg_latency << "ms" << endl;
        cout << "Max Latency: " << perf_stats.max_latency << "ms" << endl;
        cout << "Dropped Frames: " << perf_stats.dropped_frames << endl;
        cout << "Average FPS: " << total_fps_counter.getFPS() << endl;
    }

    bool initializeCamera()
    {
        cout << "[0/5] Initializing camera system..." << endl;

        camera = make_unique<CameraManager>();

        string camera_source;
        CameraManager::CameraType camera_type;

        switch (config.camera_mode)
        {
        case SystemConfig::CAMERA_PHONE_USB:
            cout << "  Mode: Phone Camera (USB - DroidCam)" << endl;
            phone_camera_used = true;
            camera_source = CameraManager::findDroidCamDevice();
            camera_type = CameraManager::CAMERA_V4L2;
            break;

        case SystemConfig::CAMERA_PHONE_WIFI:
            cout << "  Mode: Phone Camera (WiFi)" << endl;
            phone_camera_used = true;
            camera_source = "http://" + config.phone_ip +
                            ":" + to_string(config.phone_port) + "/video";
            camera_type = CameraManager::CAMERA_GSTREAMER;
            break;

        case SystemConfig::CAMERA_WEBCAM:
            cout << "  Mode: Webcam" << endl;
            phone_camera_used = false;
            camera_source = "/dev/video0";
            camera_type = config.use_v4l2 ? CameraManager::CAMERA_V4L2 : CameraManager::CAMERA_OPENCV;
            break;

        case SystemConfig::CAMERA_AUTO_DETECT:
        default:
            cout << "  Mode: Auto-detect" << endl;
            camera_source = CameraManager::findDroidCamDevice();
            phone_camera_used = true;
            camera_type = CameraManager::CAMERA_V4L2;
            break;
        }

        actual_camera_device = camera_source;

        CameraManager::CameraConfig cam_config;
        cam_config.source = camera_source;
        cam_config.width = config.camera_width;
        cam_config.height = config.camera_height;
        cam_config.fps = config.target_fps;
        cam_config.type = camera_type;
        cam_config.use_mjpeg = config.use_mjpeg;
        cam_config.buffer_size = config.v4l2_buffer_size;
        cam_config.low_latency = true;

        if (camera->initialize(cam_config))
        {
            cout << "  ✓ Camera initialized successfully" << endl;

            Size actual_size = camera->getFrameSize();
            if (actual_size.width != config.camera_width ||
                actual_size.height != config.camera_height)
            {
                cout << "  ⚠ Camera using " << actual_size.width << "x" << actual_size.height << endl;
                config.camera_width = actual_size.width;
                config.camera_height = actual_size.height;
            }

            return true;
        }

        return false;
    }

    // FIXED: Much more conservative capture loop
    void captureLoop()
    {
        Mat frame;
        int consecutive_failures = 0;
        const int max_failures = 50; // Increased tolerance

        cout << "Capture thread started" << endl;

        while (running && !g_should_exit)
        {
            auto capture_start = steady_clock::now();

            if (!camera->grabFrame(frame))
            {
                consecutive_failures++;
                if (consecutive_failures >= max_failures)
                {
                    cerr << "Camera capture failed " << consecutive_failures
                         << " times, stopping..." << endl;
                    running = false;
                    break;
                }

                // FIXED: Progressive backoff
                if (consecutive_failures < 10)
                {
                    this_thread::sleep_for(chrono::milliseconds(50));
                }
                else if (consecutive_failures < 30)
                {
                    this_thread::sleep_for(chrono::milliseconds(100));
                }
                else
                {
                    this_thread::sleep_for(chrono::milliseconds(500));
                }
                continue;
            }

            consecutive_failures = 0;

            if (frame.empty())
            {
                this_thread::sleep_for(chrono::milliseconds(10));
                continue;
            }

            capture_fps_counter.update();
            capture_queue.push(frame.clone());

            auto capture_end = steady_clock::now();
            double latency = duration_cast<microseconds>(
                                 capture_end - capture_start)
                                 .count() /
                             1000.0;

            perf_stats.avg_latency = (perf_stats.avg_latency * 0.9) + (latency * 0.1);
            if (latency > perf_stats.max_latency)
            {
                perf_stats.max_latency = latency;
            }

        }

        cout << "Capture thread stopped" << endl;
    }

    void processingLoop()
    {
        Mat frame;
        int processed_frames = 0;

        cout << "Processing thread started" << endl;

        while (processing_active && !g_should_exit)
        {
            if (!capture_queue.wait_and_pop(frame, 200))
            {
                continue;
            }

            if (frame.empty())
                continue;

            auto process_start = steady_clock::now();

            Mat processed_frame = frame.clone();
            processed_frames++;

            bool run_heavy_detection = (processed_frames % config.detection_interval == 0);

            if (config.enable_lane_detection && lane_detector && processed_frames % 2 == 0)
            {
                auto lane_start = chrono::high_resolution_clock::now();

                auto result = lane_detector->detectLanes(processed_frame);

                // ALWAYS draw lanes, even if empty (for debugging)
                lane_detector->drawLanes(processed_frame, result, true);

                string direction;
                double deviation;
                bool departure = lane_detector->checkLaneDeparture(
                    result, processed_frame, direction, deviation);

                if (departure)
                {
                    lane_detector->drawDepartureWarning(
                        processed_frame, direction, deviation, true);

                    if (!lane_departure_alert.exchange(true))
                    {
                        cout << "\a";
                        cout << "🚨 LANE DEPARTURE: " << direction << " ("
                             << fixed << setprecision(1) << abs(deviation * 100) << "%)" << endl;
                    }
                }
                else
                {
                    lane_departure_alert = false;
                }

                auto lane_end = chrono::high_resolution_clock::now();
                lane_processing_time = duration_cast<milliseconds>(
                                           lane_end - lane_start)
                                           .count();
            }
            
            addOverlays(processed_frame);
            display_queue.push(processed_frame);

            auto process_end = steady_clock::now();
            total_processing_time = duration_cast<milliseconds>(
                                        process_end - process_start)
                                        .count();

            total_fps_counter.update();

        }

        cout << "Processing thread stopped" << endl;
    }

    void addOverlays(Mat &frame)
    {
        string camera_source_text = phone_camera_used ? "📱 DROIDCAM" : "🎥 WEBCAM";
        putText(frame, camera_source_text,
                Point(10, frame.rows - 30),
                FONT_HERSHEY_SIMPLEX, 0.6, Scalar(255, 255, 0), 2);

        auto now = system_clock::now();
        auto now_time_t = system_clock::to_time_t(now);
        stringstream ss;
        ss << put_time(localtime(&now_time_t), "%H:%M:%S");
        putText(frame, ss.str(), Point(frame.cols - 100, 30),
                FONT_HERSHEY_SIMPLEX, 0.5, Scalar(255, 255, 255), 1);
    }

    void displayLoop()
    {
        Mat display_frame;
        auto last_fps_update = steady_clock::now();
        int frames_displayed = 0;
        double current_display_fps = 0;

        namedWindow("Smart Drive Manager", WINDOW_NORMAL);
        resizeWindow("Smart Drive Manager", config.camera_width, config.camera_height);

        if (config.fullscreen)
        {
            setWindowProperty("Smart Drive Manager", WND_PROP_FULLSCREEN, WINDOW_FULLSCREEN);
        }

        cout << "Display loop started" << endl;

        while (running && !g_should_exit)
        {
            if (display_queue.try_pop(display_frame))
            {
                if (!display_frame.empty())
                {
                    frames_displayed++;

                    auto current_time = steady_clock::now();
                    auto elapsed = duration_cast<milliseconds>(
                                       current_time - last_fps_update)
                                       .count();

                    if (elapsed >= 1000)
                    {
                        current_display_fps = frames_displayed * 1000.0 / elapsed;
                        frames_displayed = 0;
                        last_fps_update = current_time;
                    }

                    if (config.show_fps && display_frame.cols > 0 && display_frame.rows > 0)
                    {
                        stringstream fps_ss;
                        fps_ss << "FPS: " << (int)current_display_fps;
                        putText(display_frame, fps_ss.str(),
                                Point(display_frame.cols - 120, 30),
                                FONT_HERSHEY_SIMPLEX, 0.7, Scalar(0, 255, 0), 2);
                    }

                    if (config.show_processing_times)
                    {
                        stringstream times_ss;
                        times_ss << fixed << setprecision(1);
                        times_ss << "Capture: " << (int)capture_fps_counter.getFPS() << " fps";

                        if (config.enable_lane_detection)
                        {
                            times_ss << " | Lane: " << lane_processing_time.load() << "ms";
                        }
                        if (config.enable_object_detection)
                        {
                            times_ss << " | YOLO: " << yolo_processing_time.load() << "ms";
                        }
                        if (config.enable_drowsiness_detection)
                        {
                            times_ss << " | Face: " << drowsiness_processing_time.load() << "ms";
                        }
                        times_ss << " | Total: " << total_processing_time.load() << "ms";

                        putText(display_frame, times_ss.str(),
                                Point(10, display_frame.rows - 10),
                                FONT_HERSHEY_SIMPLEX, 0.4, Scalar(255, 255, 255), 1);
                    }

                    imshow("Smart Drive Manager", display_frame);
                }
            }
            else
            {
                // Wait a bit if no frame available
                this_thread::sleep_for(chrono::milliseconds(10));
            }

            int key = waitKey(1);
            if (key != -1)
            {
                handleKeyPress(key);
            }
        }

        destroyAllWindows();
        cout << "Display loop stopped" << endl;
    }

    void handleKeyPress(int key)
    {
        switch (key)
        {
        case 'a':
        case 'A':
            if (simulation_running)
            {
                simulated_target_speed = min(120.0, simulated_target_speed + 10.0);
                cout << "Target speed: " << simulated_target_speed << " km/h" << endl;
            }
            break;

        case 'z':
        case 'Z':
            if (simulation_running)
            {
                simulated_target_speed = max(0.0, simulated_target_speed - 10.0);
                cout << "Target speed: " << simulated_target_speed << " km/h" << endl;
            }
            break;

        case 'x':
        case 'X':
            if (simulation_running)
            {
                simulated_target_speed = 0;
                cout << "Emergency brake!" << endl;
            }
            break;

        case '4':
            
            break;

        case '5':
            
            break;

        case 27: // ESC
            cout << "Exiting..." << endl;
            running = false;
            g_should_exit = true;
            break;

        case 'f':
        case 'F':
            config.fullscreen = !config.fullscreen;
            setWindowProperty("Smart Drive Manager", WND_PROP_FULLSCREEN,
                              config.fullscreen ? WINDOW_FULLSCREEN : WINDOW_NORMAL);
            cout << "Fullscreen: " << (config.fullscreen ? "ON" : "OFF") << endl;
            break;

        case 's':
        case 'S':
        {
            Mat screenshot;
            if (display_queue.try_pop(screenshot))
            {
                static int screenshot_count = 0;
                stringstream filename;
                time_t now = time(nullptr);
                struct tm *t = localtime(&now);
                char time_str[20];
                strftime(time_str, sizeof(time_str), "%Y%m%d_%H%M%S", t);
                filename << "screenshot_" << time_str << "_" << screenshot_count++ << ".jpg";
                imwrite(filename.str(), screenshot);
                cout << "📸 Screenshot saved: " << filename.str() << endl;
            }
        }
        break;

        case 'p':
        case 'P':
            processing_active = !processing_active;
            cout << "Processing: " << (processing_active ? "RESUMED" : "PAUSED") << endl;
            break;

        case 'r':
        case 'R':
            total_fps_counter.reset();
            capture_fps_counter.reset();
            display_fps_counter.reset();
            perf_stats = PerformanceStats();
            perf_stats.start_time = steady_clock::now();
            cout << "Performance stats reset" << endl;
            break;

        case 'h':
        case 'H':
            config.show_fps = !config.show_fps;
            cout << "FPS display: " << (config.show_fps ? "ON" : "OFF") << endl;
            break;

        case '1':
            config.enable_lane_detection = !config.enable_lane_detection;
            cout << "Lane detection: " << (config.enable_lane_detection ? "ON" : "OFF") << endl;
            break;

        case '2':
            config.enable_object_detection = !config.enable_object_detection;
            cout << "Object detection: " << (config.enable_object_detection ? "ON" : "OFF") << endl;
            break;

        case '3':
            config.enable_drowsiness_detection = !config.enable_drowsiness_detection;
            cout << "Drowsiness detection: " << (config.enable_drowsiness_detection ? "ON" : "OFF") << endl;
            break;

        case '+':
        case '=':
            config.detection_interval = min(10, config.detection_interval + 1);
            cout << "Detection interval: " << config.detection_interval << endl;
            break;

        case '-':
        case '_':
            config.detection_interval = max(1, config.detection_interval - 1);
            cout << "Detection interval: " << config.detection_interval << endl;
            break;
        }
    }
};

void printUsage(const char *program_name)
{
    cout << "Smart Drive Manager - Advanced Driver Assistance System" << endl;
    cout << "With DroidCam Phone Camera Support" << endl;
    cout << endl;
    cout << "Usage: " << program_name << " [OPTIONS]" << endl;
    cout << endl;
    cout << "Camera Options:" << endl;
    cout << "  --phone-usb, -pu          Use phone camera via USB (DroidCam)" << endl;
    cout << "  --phone-wifi, -pw IP      Use phone camera via WiFi at IP address" << endl;
    cout << "  --webcam, -w              Use built-in webcam" << endl;
    cout << "  --auto, -a                Auto-detect camera (default)" << endl;
    cout << "  --width, -W N             Set camera width (default: 640)" << endl;
    cout << "  --height, -H N            Set camera height (default: 480)" << endl;
    cout << "  --fps, -f N               Set target FPS (default: 30)" << endl;
    cout << endl;
    cout << "Feature Options:" << endl;
    cout << "  --no-lane, -nl            Disable lane detection" << endl;
    cout << "  --no-object, -no          Disable object detection" << endl;
    cout << "  --no-drowsiness, -nd      Disable drowsiness detection" << endl;
    cout << "  --no-gps                  Disable USB GPS acceleration detection" << endl;
    cout << "  --gpu                     Use GPU acceleration (if available)" << endl;
    cout << endl;
    cout << "Performance Options:" << endl;
    cout << "  --interval, -i N          Processing interval (default: 1)" << endl;
    cout << "  --detection-interval N    Heavy detection interval (default: 3)" << endl;
    cout << endl;
    cout << "Display Options:" << endl;
    cout << "  --fullscreen              Start in fullscreen mode" << endl;
    cout << "  --no-fps                  Hide FPS display" << endl;
    cout << "  --no-times                Hide processing times" << endl;
    cout << endl;
    cout << "GPS Options:" << endl;
    cout << "  --gps-port N              Set USB GPS port (default: 5555)" << endl;
    cout << "  --no-gps-alerts           Disable GPS acceleration alerts" << endl;
    cout << "  --rapid-accel N           Rapid acceleration threshold (default: 3.0 m/s²)" << endl;
    cout << "  --hard-brake N            Hard brake threshold (default: -4.0 m/s²)" << endl;
    cout << endl;
    cout << "Lane Detection Options:" << endl;
    cout << "  --lane-model, -lm TYPE    Lane detection model type:" << endl;
    cout << "                            tusimple_res18 (default)" << endl;
    cout << "                            tusimple_res34" << endl;
    cout << "                            culane_res18" << endl;
    cout << "                            culane_res34" << endl;
    cout << endl;
    cout << "Other Options:" << endl;
    cout << "  --help, -h                Show this help message" << endl;
    cout << "  --version, -v             Show version information" << endl;
    cout << endl;
}

int main(int argc, char **argv)
{
    signal(SIGINT, signalHandler);
    signal(SIGTERM, signalHandler);

    SystemConfig config;

    // Parse command line arguments
    for (int i = 1; i < argc; i++)
    {
        string arg = argv[i];

        if (arg == "--phone-usb" || arg == "-pu")
        {
            config.camera_mode = SystemConfig::CAMERA_PHONE_USB;
        }
        else if (arg == "--phone-wifi" || arg == "-pw")
        {
            config.camera_mode = SystemConfig::CAMERA_PHONE_WIFI;
            if (i + 1 < argc && argv[i + 1][0] != '-')
            {
                config.phone_ip = argv[++i];
            }
        }
        else if (arg == "--webcam" || arg == "-w")
        {
            config.camera_mode = SystemConfig::CAMERA_WEBCAM;
        }
        else if (arg == "--auto" || arg == "-a")
        {
            config.camera_mode = SystemConfig::CAMERA_AUTO_DETECT;
        }
        else if (arg == "--width" || arg == "-W")
        {
            if (i + 1 < argc)
                config.camera_width = stoi(argv[++i]);
        }
        else if (arg == "--height" || arg == "-H")
        {
            if (i + 1 < argc)
                config.camera_height = stoi(argv[++i]);
        }
        else if (arg == "--fps" || arg == "-f")
        {
            if (i + 1 < argc)
                config.target_fps = stoi(argv[++i]);
        }
        // In the argument parsing section, add:
        else if (arg == "--lane-model" || arg == "-lm")
        {
            if (i + 1 < argc)
                config.lane_model = argv[++i];
        }
        else if (arg == "--no-lane" || arg == "-nl")
        {
            config.enable_lane_detection = false;
        }
        else if (arg == "--no-object" || arg == "-no")
        {
            config.enable_object_detection = false;
        }
        else if (arg == "--no-drowsiness" || arg == "-nd")
        {
            config.enable_drowsiness_detection = false;
        }
        else if (arg == "--no-gps")
        {
            config.enable_usb_gps = false;
        }
        else if (arg == "--gps-port")
        {
            if (i + 1 < argc)
                config.gps_usb_port = stoi(argv[++i]);
        }
        else if (arg == "--no-gps-alerts")
        {
            config.enable_gps_alerts = false;
        }
        else if (arg == "--rapid-accel")
        {
            if (i + 1 < argc)
                config.rapid_accel_threshold = stof(argv[++i]);
        }
        else if (arg == "--hard-brake")
        {
            if (i + 1 < argc)
                config.hard_brake_threshold = stof(argv[++i]);
        }
        else if (arg == "--gpu")
        {
            config.use_gpu = true;
        }
        else if (arg == "--interval" || arg == "-i")
        {
            if (i + 1 < argc)
                config.processing_interval = stoi(argv[++i]);
        }
        else if (arg == "--detection-interval")
        {
            if (i + 1 < argc)
                config.detection_interval = stoi(argv[++i]);
        }
        else if (arg == "--fullscreen")
        {
            config.fullscreen = true;
        }
        else if (arg == "--no-fps")
        {
            config.show_fps = false;
        }
        else if (arg == "--no-times")
        {
            config.show_processing_times = false;
        }
        else if (arg == "--help" || arg == "-h")
        {
            printUsage(argv[0]);
            return 0;
        }
        else if (arg == "--version" || arg == "-v")
        {
            cout << "Smart Drive Manager v2.0" << endl;
            cout << "Data Structures Project - BSCS-24063" << endl;
            return 0;
        }
        else
        {
            cerr << "Unknown option: " << arg << endl;
            cerr << "Use --help for usage information" << endl;
            return 1;
        }
    }

    try
    {
        SmartDriveManager system(config);

        if (!system.initialize())
        {
            cerr << "✗ Failed to initialize system" << endl;
            return -1;
        }

        system.start();
        system.stop();

        cout << endl;
        cout << "╔════════════════════════════════════╗" << endl;
        cout << "║   SYSTEM SHUTDOWN COMPLETE         ║" << endl;
        cout << "╚════════════════════════════════════╝" << endl;
    }
    catch (const exception &e)
    {
        cerr << "Fatal error: " << e.what() << endl;
        return -1;
    }

    return 0;
}