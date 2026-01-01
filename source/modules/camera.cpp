#include "camera.h"
#include <iostream>
#include <chrono>
#include <sstream>
#include <fstream>
#include <dirent.h>
#include <sys/stat.h>
#include <algorithm>

using namespace std::chrono;

CameraManager::CameraManager()
    : camera_opened(false),
      capturing(false),
      new_frame_available(false),
      read_index(0),
      write_index(1),
      current_fps(0.0),
      frame_counter(0),
      frame_size(1280, 720),
      phone_camera_fd(-1),
      phone_connected(false)
{
#ifdef __linux__
    v4l2_fd = -1;
    is_streaming = false;
    v4l2_pixel_format = 0;
#endif

    frame_buffers.resize(3);
    fps_start_time = steady_clock::now();
}

CameraManager::~CameraManager()
{
    stopBackgroundCapture();
    release();
}

bool CameraManager::initialize(const CameraConfig &cfg)
{
    config = cfg;
    return initializeImpl();
}

bool CameraManager::initialize(const string &source, int width, int height, int fps, CameraType type)
{
    config.source = source;
    config.width = width;
    config.height = height;
    config.fps = fps;
    config.type = type;
    return initializeImpl();
}

bool CameraManager::initializePhoneCamera(PhoneCameraMode mode, int width, int height, int fps)
{
    config.phone_mode = mode;
    config.width = width;
    config.height = height;
    config.fps = fps;
    config.type = CAMERA_ANDROID_USB;

    config.source = findDroidCamDevice();

    return initializeImpl();
}
string CameraManager::findDroidCamDevice()
{
    cout << "Searching for DroidCam device..." << endl;

    string preferred_device = "/dev/video0";
    int fd = open(preferred_device.c_str(), O_RDWR | O_NONBLOCK);
    if (fd >= 0)
    {
        struct v4l2_capability cap;
        if (ioctl(fd, VIDIOC_QUERYCAP, &cap) == 0)
        {
            string card((char *)cap.card);
            cout << "  Found " << preferred_device << ": " << card << endl;
            close(fd);

            cout << "  ✓ Using: " << preferred_device << " (DroidCam expected location)" << endl;
            return preferred_device;
        }
        close(fd);
    }

    for (int i = 1; i < 5; i++)
    {
        string device = "/dev/video" + to_string(i);
        fd = open(device.c_str(), O_RDWR | O_NONBLOCK);
        if (fd < 0)
            continue;

        struct v4l2_capability cap;
        if (ioctl(fd, VIDIOC_QUERYCAP, &cap) == 0)
        {
            string card((char *)cap.card);
            if (card.find("Droidcam") != string::npos ||
                card.find("droidcam") != string::npos)
            {
                cout << "  ✓ Found DroidCam at " << device << endl;
                close(fd);
                return device;
            }
        }
        close(fd);
    }

    cout << "  ⚠ No DroidCam found, defaulting to /dev/video0" << endl;
    return "/dev/video0";
}
bool CameraManager::initializeImpl()
{
    
    if (config.source == "/dev/video4" || config.source.find("phone") != string::npos)
    {
        
        config.source = findDroidCamDevice();
    }

    if (config.source.find("http:
    {
        config.type = CAMERA_GSTREAMER;
    }
    else if (config.source.find("/dev/video") == 0)
    {
#ifdef __linux__
        config.type = CAMERA_V4L2;
#else
        config.type = CAMERA_OPENCV;
#endif
    }
    else
    {
        config.type = CAMERA_OPENCV;
    }

    bool success = false;

    switch (config.type)
    {
#ifdef __linux__
    case CAMERA_V4L2:
        success = initializeV4L2();
        break;
#endif
    case CAMERA_GSTREAMER:
        success = initializeGStreamer();
        break;
    case CAMERA_OPENCV:
    default:
        success = initializeOpenCV();
        break;
    }

    if (success)
    {
        camera_opened = true;
        frame_size = Size(config.width, config.height);
        cout << "Camera initialized: " << config.source
             << " [" << config.width << "x" << config.height << "@" << config.fps << "fps]"
             << " Type: " << (config.type == CAMERA_V4L2 ? "V4L2" : "OpenCV")
             << endl;
    }

    return success;
}

#ifdef __linux__
bool CameraManager::initializeV4L2()
{
    cout << "Initializing V4L2 device: " << config.source << endl;

    v4l2_fd = open(config.source.c_str(), O_RDWR | O_NONBLOCK, 0);
    if (v4l2_fd < 0)
    {
        cerr << "  ✗ Cannot open V4L2 device: " << config.source << " (" << strerror(errno) << ")" << endl;
        return false;
    }

    struct v4l2_capability cap;
    if (ioctl(v4l2_fd, VIDIOC_QUERYCAP, &cap) < 0)
    {
        cerr << "  ✗ Not a V4L2 device" << endl;
        close(v4l2_fd);
        v4l2_fd = -1;
        return false;
    }

    if (!(cap.capabilities & V4L2_CAP_VIDEO_CAPTURE))
    {
        cerr << "  ✗ Device does not support capture" << endl;
        close(v4l2_fd);
        v4l2_fd = -1;
        return false;
    }

    cout << "  Device: " << cap.card << " (" << cap.driver << ")" << endl;

    if (!setupV4L2Format())
    {
        cerr << "  ✗ Failed to setup format" << endl;
        close(v4l2_fd);
        v4l2_fd = -1;
        return false;
    }

    config.buffer_size = 2; 

    if (!setupV4L2Buffers())
    {
        cerr << "  ✗ Failed to setup buffers" << endl;
        close(v4l2_fd);
        v4l2_fd = -1;
        return false;
    }

    if (!startV4L2Streaming())
    {
        cerr << "  ✗ Failed to start streaming" << endl;
        close(v4l2_fd);
        v4l2_fd = -1;
        return false;
    }

    if (config.low_latency)
    {
        optimizeForLowLatency();
    }

    frame_size = Size(config.width, config.height);
    cout << "  ✓ V4L2 camera initialized successfully" << endl;
    camera_opened = true;
    return true;
}

bool CameraManager::optimizeForLowLatency()
{
    cout << "  Applying low-latency optimizations..." << endl;

    setV4L2Control(V4L2_CID_EXPOSURE_AUTO, V4L2_EXPOSURE_MANUAL);
    setV4L2Control(V4L2_CID_EXPOSURE_ABSOLUTE, 150);

    setV4L2Control(V4L2_CID_AUTO_WHITE_BALANCE, 0);

    setV4L2Control(V4L2_CID_POWER_LINE_FREQUENCY, V4L2_CID_POWER_LINE_FREQUENCY_50HZ);

    cout << "  ✓ Low-latency optimizations applied" << endl;
    return true;
}

bool CameraManager::setupV4L2Format()
{
    struct v4l2_format format = {};
    format.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

    if (ioctl(v4l2_fd, VIDIOC_G_FMT, &format) < 0)
    {
        cerr << "  ✗ Failed to get format" << endl;
        return false;
    }

    cout << "  Current: " << format.fmt.pix.width << "x" << format.fmt.pix.height << endl;

    v4l2_pixel_format = format.fmt.pix.pixelformat;

    format.fmt.pix.width = config.width;
    format.fmt.pix.height = config.height;
    format.fmt.pix.field = V4L2_FIELD_NONE;

    format.fmt.pix.pixelformat = V4L2_PIX_FMT_MJPEG;
    if (ioctl(v4l2_fd, VIDIOC_S_FMT, &format) >= 0)
    {
        config.use_mjpeg = true;
        v4l2_pixel_format = V4L2_PIX_FMT_MJPEG;
        cout << "  Format: MJPEG [COMPRESSED - Best for USB]" << endl;
        goto format_success;
    }

    cout << "  ! MJPEG not supported, trying YUYV..." << endl;
    format.fmt.pix.pixelformat = V4L2_PIX_FMT_YUYV;
    if (ioctl(v4l2_fd, VIDIOC_S_FMT, &format) >= 0)
    {
        config.use_mjpeg = false;
        v4l2_pixel_format = V4L2_PIX_FMT_YUYV;
        cout << "  Format: YUYV [UNCOMPRESSED]" << endl;
        goto format_success;
    }

    cout << "  ! YUYV not supported, trying YUV420..." << endl;
    format.fmt.pix.pixelformat = V4L2_PIX_FMT_YUV420;
    if (ioctl(v4l2_fd, VIDIOC_S_FMT, &format) >= 0)
    {
        config.use_mjpeg = false;
        v4l2_pixel_format = V4L2_PIX_FMT_YUV420;
        cout << "  Format: YUV420 [UNCOMPRESSED - High bandwidth]" << endl;
        goto format_success;
    }

    cerr << "  ✗ No suitable format found" << endl;
    return false;

format_success:
    
    config.width = format.fmt.pix.width;
    config.height = format.fmt.pix.height;

    char fourcc[5] = {0};
    memcpy(fourcc, &v4l2_pixel_format, 4);
    cout << "  Resolution: " << config.width << "x" << config.height << " (" << fourcc << ")" << endl;

    struct v4l2_streamparm parm = {};
    parm.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

    if (ioctl(v4l2_fd, VIDIOC_G_PARM, &parm) == 0)
    {
        parm.parm.capture.timeperframe.numerator = 1;
        parm.parm.capture.timeperframe.denominator = config.fps;

        if (ioctl(v4l2_fd, VIDIOC_S_PARM, &parm) >= 0)
        {
            cout << "  FPS: " << config.fps << endl;
        }
    }

    return true;
}

bool CameraManager::setupV4L2Buffers()
{
    
    struct v4l2_requestbuffers req = {};
    req.count = config.buffer_size;
    req.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    req.memory = V4L2_MEMORY_MMAP;

    if (ioctl(v4l2_fd, VIDIOC_REQBUFS, &req) < 0)
    {
        cerr << "  ✗ Failed to request buffers: " << strerror(errno) << endl;
        return false;
    }

    if (req.count < 2)
    {
        cerr << "  ✗ Insufficient buffer memory" << endl;
        return false;
    }

    v4l2_buffers.resize(req.count);

    for (unsigned int i = 0; i < req.count; ++i)
    {
        struct v4l2_buffer buf = {};
        buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buf.memory = V4L2_MEMORY_MMAP;
        buf.index = i;

        if (ioctl(v4l2_fd, VIDIOC_QUERYBUF, &buf) < 0)
        {
            cerr << "  ✗ Failed to query buffer " << i << endl;
            return false;
        }

        v4l2_buffers[i].length = buf.length;
        v4l2_buffers[i].start = mmap(NULL, buf.length,
                                     PROT_READ | PROT_WRITE, MAP_SHARED,
                                     v4l2_fd, buf.m.offset);

        if (v4l2_buffers[i].start == MAP_FAILED)
        {
            cerr << "  ✗ Failed to map buffer " << i << endl;
            return false;
        }

        v4l2_buffers[i].queued = true;

        if (ioctl(v4l2_fd, VIDIOC_QBUF, &buf) < 0)
        {
            cerr << "  ✗ Failed to queue buffer " << i << endl;
            return false;
        }
    }

    cout << "  ✓ Allocated " << req.count << " buffers" << endl;
    return true;
}

bool CameraManager::startV4L2Streaming()
{
    enum v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    if (ioctl(v4l2_fd, VIDIOC_STREAMON, &type) < 0)
    {
        cerr << "  ✗ Failed to start streaming: " << strerror(errno) << endl;
        return false;
    }

    is_streaming = true;
    cout << "  ✓ Streaming started" << endl;
    return true;
}

bool CameraManager::stopV4L2Streaming()
{
    if (!is_streaming || v4l2_fd < 0)
        return true;

    enum v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    if (ioctl(v4l2_fd, VIDIOC_STREAMOFF, &type) < 0)
    {
        cerr << "  ✗ Failed to stop streaming" << endl;
        return false;
    }

    is_streaming = false;
    return true;
}

Mat CameraManager::readV4L2Frame(bool blocking)
{
    if (v4l2_fd < 0 || !is_streaming)
    {
        return Mat();
    }

    struct pollfd pfd;
    pfd.fd = v4l2_fd;
    pfd.events = POLLIN;

    int timeout_ms = blocking ? 1000 : 100; 
    int ret = poll(&pfd, 1, timeout_ms);

    if (ret < 0)
    {
        if (errno != EINTR)
        {
            cerr << "V4L2 poll error: " << strerror(errno) << endl;
        }
        return Mat();
    }

    if (ret == 0)
    {
        
        return Mat();
    }

    struct v4l2_buffer buf = {};
    buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    buf.memory = V4L2_MEMORY_MMAP;

    if (ioctl(v4l2_fd, VIDIOC_DQBUF, &buf) < 0)
    {
        if (errno != EAGAIN)
        {
            cerr << "V4L2 dequeue error: " << strerror(errno) << endl;
        }
        return Mat();
    }

    Mat frame;

    if (v4l2_pixel_format == V4L2_PIX_FMT_MJPEG)
    {
        
        vector<uchar> data((uchar *)v4l2_buffers[buf.index].start,
                           (uchar *)v4l2_buffers[buf.index].start + buf.bytesused);
        frame = imdecode(data, IMREAD_COLOR);
    }
    else if (v4l2_pixel_format == V4L2_PIX_FMT_YUYV)
    {
        
        Mat yuyv(config.height, config.width, CV_8UC2, v4l2_buffers[buf.index].start);
        cvtColor(yuyv, frame, COLOR_YUV2BGR_YUYV);
    }
    else if (v4l2_pixel_format == V4L2_PIX_FMT_YUV420)
    {
        
        int y_size = config.width * config.height;
        Mat yuv_frame(config.height + config.height / 2, config.width, CV_8UC1,
                      v4l2_buffers[buf.index].start);

        try
        {
            cvtColor(yuv_frame, frame, COLOR_YUV2BGR_I420);
        }
        catch (const cv::Exception &e)
        {
            cerr << "YUV420 conversion failed: " << e.what() << endl;
            frame = Mat();
        }
    }

    if (ioctl(v4l2_fd, VIDIOC_QBUF, &buf) < 0)
    {
        cerr << "V4L2 requeue error: " << strerror(errno) << endl;
    }

    return frame;
}

bool CameraManager::setV4L2Control(unsigned int id, int value)
{
    struct v4l2_control ctrl;
    ctrl.id = id;
    ctrl.value = value;

    if (ioctl(v4l2_fd, VIDIOC_S_CTRL, &ctrl) < 0)
    {
        return false;
    }

    return true;
}

int CameraManager::getV4L2Control(unsigned int id)
{
    struct v4l2_control ctrl;
    ctrl.id = id;

    if (ioctl(v4l2_fd, VIDIOC_G_CTRL, &ctrl) < 0)
    {
        return -1;
    }

    return ctrl.value;
}

vector<string> CameraManager::getV4L2Formats()
{
    vector<string> formats;

    if (v4l2_fd < 0)
        return formats;

    struct v4l2_fmtdesc fmtdesc;
    fmtdesc.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    fmtdesc.index = 0;

    while (ioctl(v4l2_fd, VIDIOC_ENUM_FMT, &fmtdesc) == 0)
    {
        string format_name((char *)fmtdesc.description);
        formats.push_back(format_name);
        fmtdesc.index++;
    }

    return formats;
}
#endif

bool CameraManager::initializeAndroidUSB()
{
    cout << "Initializing Android phone camera via USB..." << endl;
    config.type = CAMERA_V4L2;
    return initializeV4L2();
}

bool CameraManager::initializeAndroidIP()
{
    cout << "Initializing Android phone camera via IP..." << endl;

    string pipeline = "souphttpsrc location=http:
                      ":" + to_string(config.phone_port) + "/video is-live=true ! "
                                                           "jpegdec ! videoconvert ! appsink drop=true max-buffers=1";

    config.source = pipeline;
    config.type = CAMERA_GSTREAMER;

    return initializeGStreamer();
}

bool CameraManager::initializeGStreamer()
{
    cout << "Initializing GStreamer..." << endl;

    string pipeline;

    if (config.source.find("rtsp:
    {
        pipeline = "rtspsrc location=" + config.source + " latency=0 ! "
                                                         "rtph264depay ! h264parse ! avdec_h264 ! "
                                                         "videoconvert ! videoscale ! video/x-raw,width=" +
                   to_string(config.width) + ",height=" +
                   to_string(config.height) + " ! appsink drop=true max-buffers=1";
    }
    else if (config.source.find("http:
    {
        pipeline = "souphttpsrc location=" + config.source + " is-live=true ! "
                                                             "jpegdec ! videoconvert ! videoscale ! video/x-raw,width=" +
                   to_string(config.width) + ",height=" +
                   to_string(config.height) + " ! appsink drop=true max-buffers=1";
    }
    else
    {
        pipeline = config.source;
    }

    cout << "  Pipeline: " << pipeline << endl;

    cap.open(pipeline, CAP_GSTREAMER);
    if (!cap.isOpened())
    {
        cerr << "  ✗ Failed to open GStreamer pipeline" << endl;
        return false;
    }

    cout << "  ✓ GStreamer initialized" << endl;
    camera_opened = true;
    return true;
}

bool CameraManager::initializeOpenCV()
{
    cout << "Initializing OpenCV camera..." << endl;

    if (config.source.find("/dev/video") == 0)
    {
        cap.open(config.source, CAP_V4L2);
    }
    else
    {
        cap.open(config.source);
    }

    if (!cap.isOpened())
    {
        cerr << "  ✗ Failed to open camera with OpenCV" << endl;
        return false;
    }

    cap.set(CAP_PROP_FRAME_WIDTH, config.width);
    cap.set(CAP_PROP_FRAME_HEIGHT, config.height);
    cap.set(CAP_PROP_FPS, config.fps);
    cap.set(CAP_PROP_BUFFERSIZE, 1);

    cout << "  ✓ OpenCV camera initialized" << endl;
    camera_opened = true;
    return true;
}

bool CameraManager::grabFrame(Mat &frame)
{
    if (!camera_opened)
    {
        return false;
    }

    Mat captured_frame;
    static int consecutive_empty = 0;
    static auto last_success = steady_clock::now();

    switch (config.type)
    {
#ifdef __linux__
    case CAMERA_V4L2:
    case CAMERA_ANDROID_USB:
        captured_frame = readV4L2Frame(false);
        break;
#endif
    case CAMERA_ANDROID_IP:
    case CAMERA_GSTREAMER:
    case CAMERA_OPENCV:
    default:
        if (!cap.grab())
        {
            return false;
        }
        if (!cap.retrieve(captured_frame))
        {
            return false;
        }
        break;
    }

    if (captured_frame.empty())
    {
        consecutive_empty++;

        auto now = steady_clock::now();
        auto since_success = duration_cast<seconds>(now - last_success).count();

        if (consecutive_empty % 10 == 0)
        {
            cout << "Camera capture empty (" << consecutive_empty << " frames, "
                 << since_success << "s since last success)" << endl;
        }

        if (since_success > 5 && consecutive_empty > 20)
        {
            cerr << "Camera appears stuck, attempting recovery..." << endl;
            if (recover()) {
                cout << "✓ Camera recovery successful" << endl;
            } else {
                cerr << "✗ Camera recovery failed" << endl;
            }
            consecutive_empty = 0;
        }

        return false;
    }

    consecutive_empty = 0;
    last_success = steady_clock::now();

    frame = captured_frame;

    frame_counter++;
    auto current_time = steady_clock::now();
    auto elapsed = duration_cast<milliseconds>(current_time - fps_start_time).count();

    if (elapsed >= 1000)
    {
        current_fps = frame_counter * 1000.0 / elapsed;
        frame_counter = 0;
        fps_start_time = current_time;
    }

    return true;
}

bool CameraManager::getLatestFrame(Mat &frame)
{
    if (!camera_opened || !new_frame_available)
    {
        return false;
    }

    lock_guard<mutex> lock(frame_mutex);

    int current_read = read_index.load();
    if (!frame_buffers[current_read].empty())
    {
        frame = frame_buffers[current_read].clone();
        new_frame_available = false;
        return true;
    }

    return false;
}

void CameraManager::startBackgroundCapture()
{
    if (!camera_opened || capturing)
    {
        return;
    }

    capturing = true;
    capture_thread = thread(&CameraManager::captureThread, this);
    cout << "Background capture started" << endl;
}

void CameraManager::stopBackgroundCapture()
{
    capturing = false;
    if (capture_thread.joinable())
    {
        capture_thread.join();
    }
    cout << "Background capture stopped" << endl;
}

void CameraManager::captureThread()
{
    while (capturing && camera_opened)
    {
        Mat frame;

        if (grabFrame(frame) && !frame.empty())
        {
            lock_guard<mutex> lock(frame_mutex);

            int next_write = (write_index.load() + 1) % frame_buffers.size();

            frame.copyTo(frame_buffers[next_write]);
            write_index.store(next_write);
            read_index.store(next_write);
            new_frame_available = true;
        }
    }
}

void CameraManager::release()
{
    stopBackgroundCapture();

    if (camera_opened)
    {
        camera_opened = false;

#ifdef __linux__
        if (config.type == CAMERA_V4L2 || config.type == CAMERA_ANDROID_USB)
        {
            if (is_streaming)
            {
                stopV4L2Streaming();
            }

            for (auto &buffer : v4l2_buffers)
            {
                if (buffer.start)
                {
                    munmap(buffer.start, buffer.length);
                }
            }
            v4l2_buffers.clear();

            if (v4l2_fd >= 0)
            {
                close(v4l2_fd);
                v4l2_fd = -1;
            }
        }
#endif

        if (cap.isOpened())
        {
            cap.release();
        }

        for (auto &buffer : frame_buffers)
        {
            buffer.release();
        }

        cout << "Camera released" << endl;
    }
}

bool CameraManager::recover()
{
    cout << "Attempting camera recovery [Type: " << config.type << "]..." << endl;
    
    camera_opened = false;
    
#ifdef __linux__
    if (config.type == CAMERA_V4L2 || config.type == CAMERA_ANDROID_USB)
    {
        if (is_streaming) stopV4L2Streaming();
        for (auto &buffer : v4l2_buffers)
        {
            if (buffer.start) munmap(buffer.start, buffer.length);
        }
        v4l2_buffers.clear();
        if (v4l2_fd >= 0)
        {
            close(v4l2_fd);
            v4l2_fd = -1;
        }
    }
#endif

    if (cap.isOpened()) cap.release();
    
    this_thread::sleep_for(chrono::milliseconds(500));
    
    return initializeImpl();
}

double CameraManager::getCurrentFPS() const
{
    return current_fps.load();
}

string CameraManager::getCameraInfo() const
{
    stringstream info;
    info << "Source: " << config.source << "\n";
    info << "Resolution: " << config.width << "x" << config.height << "\n";
    info << "FPS: " << config.fps << "\n";
    info << "Type: ";

    switch (config.type)
    {
    case CAMERA_OPENCV:
        info << "OpenCV";
        break;
    case CAMERA_V4L2:
        info << "V4L2";
        break;
    case CAMERA_GSTREAMER:
        info << "GStreamer";
        break;
    case CAMERA_ANDROID_USB:
        info << "Android USB";
        break;
    case CAMERA_ANDROID_IP:
        info << "Android IP";
        break;
    default:
        info << "Unknown";
    }

    return info.str();
}

vector<string> CameraManager::listV4L2Devices()
{
    vector<string> devices;

#ifdef __linux__
    DIR *dir;
    struct dirent *ent;

    if ((dir = opendir("/sys/class/video4linux/")) != NULL)
    {
        while ((ent = readdir(dir)) != NULL)
        {
            string name = ent->d_name;
            if (name.find("video") == 0)
            {
                string path = "/sys/class/video4linux/" + name + "/name";
                ifstream file(path);
                if (file.is_open())
                {
                    string device_name;
                    getline(file, device_name);
                    devices.push_back(name + ": " + device_name);
                    file.close();
                }
            }
        }
        closedir(dir);
    }
#endif

    return devices;
}

vector<string> CameraManager::detectPhoneCameras()
{
    vector<string> phones;
    vector<string> devices = listV4L2Devices();

    for (const auto &dev : devices)
    {
        string lower_dev = dev;
        transform(lower_dev.begin(), lower_dev.end(), lower_dev.begin(), ::tolower);

        if (lower_dev.find("droidcam") != string::npos ||
            lower_dev.find("uvc") != string::npos ||
            lower_dev.find("android") != string::npos ||
            lower_dev.find("usb camera") != string::npos)
        {
            phones.push_back(dev);
        }
    }

    return phones;
}

bool CameraManager::togglePhoneFlash(bool on)
{
    cout << "Phone flash toggle not implemented" << endl;
    return false;
}

bool CameraManager::switchPhoneCamera(bool front)
{
    cout << "Camera switch not implemented" << endl;
    return false;
}

bool CameraManager::setPhoneZoom(float zoom)
{
    cout << "Zoom control not implemented" << endl;
    return false;
}

bool CameraManager::connectToPhone()
{
    return false;
}

bool CameraManager::setupPhoneStream()
{
    return false;
}

void CameraManager::phoneMonitorThread()
{
    while (phone_connected)
    {
        this_thread::sleep_for(chrono::seconds(1));
    }
}

void CameraManager::setupZeroCopyBuffers()
{
    cout << "Zero-copy buffers not implemented" << endl;
}

bool CameraManager::mapFrameToMat(void *buffer, size_t size, Mat &frame)
{
    return false;
}