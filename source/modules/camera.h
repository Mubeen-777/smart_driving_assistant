
#pragma once

#include <opencv2/opencv.hpp>
#include <string>
#include <atomic>
#include <thread>
#include <memory>
#include <vector>
#include <mutex>

#ifdef __linux__
#include <linux/videodev2.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#include <poll.h>
#include <errno.h>
#include <string.h>
#endif

using namespace cv;
using namespace std;

class CameraManager {
public:
    enum CameraType {
        CAMERA_OPENCV,
        CAMERA_V4L2,
        CAMERA_GSTREAMER,
        CAMERA_ANDROID_USB,
        CAMERA_ANDROID_IP
    };

    enum PhoneCameraMode {
        PHONE_MODE_AUTO,
        PHONE_MODE_USB_V4L2,
        PHONE_MODE_USB_ADB,
        PHONE_MODE_WIFI_IP
    };

    struct CameraConfig {
        string source = "/dev/video4";
        int width = 1280;
        int height = 720;
        int fps = 30;
        int buffer_size = 20;
        CameraType type = CAMERA_V4L2;
        PhoneCameraMode phone_mode = PHONE_MODE_AUTO;
        

        bool use_mjpeg = true;
        string pixel_format = "MJPG";
        

        string phone_ip = "192.168.18.76";
        int phone_port = 4747;
        string phone_model = "";
        

        bool zero_copy = false;
        bool low_latency = true;
        int skip_frames = 0;
        

        float exposure = -1.0f;
        int brightness = -1;
        int contrast = -1;
    };

    CameraManager();
    ~CameraManager();

    bool initialize(const CameraConfig& config);
    bool initialize(const string& source = "/dev/video0", 
                   int width = 1280, 
                   int height = 720, 
                   int fps = 30,
                   CameraType type = CAMERA_V4L2);
    
    bool initializePhoneCamera(PhoneCameraMode mode = PHONE_MODE_USB_V4L2,
                              int width = 1280,
                              int height = 720,
                              int fps = 30);
    
    bool grabFrame(Mat& frame);
    bool getLatestFrame(Mat& frame);
    
    bool togglePhoneFlash(bool on);
    bool switchPhoneCamera(bool front = true);
    bool setPhoneZoom(float zoom);
    
    void startBackgroundCapture();
    void stopBackgroundCapture();
    
    bool isOpened() const { return camera_opened; }
    Size getFrameSize() const { return frame_size; }
    double getCurrentFPS() const;
    string getCameraInfo() const;
    
    void release();
    bool recover();
    
    static vector<string> listV4L2Devices();
    static vector<string> detectPhoneCameras();
    static string findDroidCamDevice();
    
private:
    CameraConfig config;
    atomic<bool> camera_opened;
    atomic<bool> capturing;
    atomic<bool> new_frame_available;
    cv::Size frame_size;
    
    VideoCapture cap;
    
#ifdef __linux__
    int v4l2_fd;
    bool is_streaming;
    uint32_t v4l2_pixel_format;
    
    struct V4L2Buffer {
        void* start;
        size_t length;
        bool queued;
    };
    vector<V4L2Buffer> v4l2_buffers;
    vector<struct pollfd> v4l2_pollfds;
#endif
    
    vector<Mat> frame_buffers;
    atomic<int> read_index;
    atomic<int> write_index;
    
    thread capture_thread;
    
    atomic<double> current_fps;
    int frame_counter;
    chrono::steady_clock::time_point fps_start_time;
    
    mutex frame_mutex;
    mutex v4l2_mutex;
    
    int phone_camera_fd;
    thread phone_monitor_thread;
    atomic<bool> phone_connected;
    
    bool initializeImpl();
    bool initializeV4L2();
    bool initializeOpenCV();
    bool initializeAndroidUSB();
    bool initializeAndroidIP();
    bool initializeGStreamer();
    
#ifdef __linux__
    bool setupV4L2Format();
    bool setupV4L2Buffers();
    bool startV4L2Streaming();
    bool stopV4L2Streaming();
    Mat readV4L2Frame(bool blocking = false);
    bool setV4L2Control(unsigned int id, int value);
    int getV4L2Control(unsigned int id);
    vector<string> getV4L2Formats();
    bool optimizeForLowLatency();
#endif
    
    bool connectToPhone();
    bool setupPhoneStream();
    void phoneMonitorThread();
    void captureThread();
    void setupZeroCopyBuffers();
    bool mapFrameToMat(void* buffer, size_t size, Mat& frame);
};