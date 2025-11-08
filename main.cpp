#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include <condition_variable>
#include <ctime>
#include <cstring>
#include <iostream>
#include <mutex>
#include <opencv2/opencv.hpp>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

static const int PORT = 8080;
static const std::string BOUNDARY = "--frame";

std::mutex frame_mutex;
std::condition_variable frame_cv;
std::vector<uchar> latest_jpeg;
bool running = true;

bool send_all(int fd, const char* data, size_t len) {
    size_t sent = 0;
    while (sent < len) {
        ssize_t r = ::send(fd, data + sent, len - sent, 0);
        if (r <= 0) return false;
        sent += (size_t)r;
    }
    return true;
}

void camera_thread_func(int device_index, int width = 640, int height = 480, int fps = 30) {
    cv::VideoCapture cap(device_index);
    if (!cap.isOpened()) {
        std::cerr << "ERROR: Could not open camera device " << device_index << std::endl;
        running = false;
        frame_cv.notify_all();
        return;
    }

    cap.set(cv::CAP_PROP_FRAME_WIDTH, width);
    cap.set(cv::CAP_PROP_FRAME_HEIGHT, height);
    cap.set(cv::CAP_PROP_FPS, fps);

    cv::Mat frame;
    std::vector<int> params = {cv::IMWRITE_JPEG_QUALITY, 80};

    while (running) {
        if (!cap.read(frame)) {
            std::cerr << "WARNING: Camera read failed, retrying..." << std::endl;
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            continue;
        }

        //overlay

        //crosshair
        int cx = frame.cols / 2;
        int cy = frame.rows / 2;
        cv::line(frame, cv::Point(cx - 20, cy), cv::Point(cx + 20, cy), cv::Scalar(255, 255, 255), 2);
        cv::line(frame, cv::Point(cx, cy - 20), cv::Point(cx, cy + 20), cv::Scalar(255, 255, 255), 2);

        //time
        auto t = std::time(nullptr);
        std::tm tm = *std::localtime(&t);
        char buf[64];
        std::strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", &tm);
        cv::putText(frame, buf, cv::Point(10, 30),
                    cv::FONT_HERSHEY_SIMPLEX, 0.8, cv::Scalar(0, 255, 255), 2);

        std::vector<uchar> buf_jpeg;
        cv::imencode(".jpg", frame, buf_jpeg, params);

        {
            std::lock_guard<std::mutex> lk(frame_mutex);
            latest_jpeg = std::move(buf_jpeg);
        }
        frame_cv.notify_all();

        std::this_thread::sleep_for(std::chrono::milliseconds(1000 / fps));
    }

    cap.release();
}

void handle_client(int client_fd) {
    char buf[4096];
    ssize_t r = ::recv(client_fd, buf, sizeof(buf) - 1, 0);
    if (r <= 0) { close(client_fd); return; }
    buf[r] = '\0';
    std::string request(buf);

    std::istringstream iss(request);
    std::string method, path, proto;
    iss >> method >> path >> proto;
    if (method.empty() || path.empty()) {
        close(client_fd);
        return;
    }

    if (path == "/" || path == "/index.html") {
        std::string html =
            "HTTP/1.0 200 OK\r\n"
            "Content-Type: text/html\r\n"
            "Cache-Control: no-cache\r\n"
            "Pragma: no-cache\r\n"
            "Connection: close\r\n\r\n"
            "<!doctype html><html><head><title>Webcam Stream</title></head>"
            "<body><h1>Webcam Stream</h1>"
            "<img src=\"/stream\" style=\"max-width:100%;height:auto;\" />"
            "</body></html>";
        send_all(client_fd, html.c_str(), html.size());
        close(client_fd);
        return;
    }

    if (path == "/stream") {
        std::string header =
            "HTTP/1.0 200 OK\r\n"
            "Cache-Control: no-cache\r\n"
            "Pragma: no-cache\r\n"
            "Connection: close\r\n"
            "Content-Type: multipart/x-mixed-replace; boundary=" + BOUNDARY + "\r\n\r\n";
        if (!send_all(client_fd, header.c_str(), header.size())) {
            close(client_fd);
            return;
        }

        while (running) {
            std::vector<uchar> frame_copy;
            {
                std::unique_lock<std::mutex> lk(frame_mutex);
                frame_cv.wait_for(lk, std::chrono::milliseconds(500),
                                  [] { return !latest_jpeg.empty() || !running; });
                if (!running) break;
                frame_copy = latest_jpeg;
            }
            if (frame_copy.empty()) continue;

            std::ostringstream part;
            part << "--" << BOUNDARY << "\r\n"
                 << "Content-Type: image/jpeg\r\n"
                 << "Content-Length: " << frame_copy.size() << "\r\n\r\n";
            std::string part_header = part.str();

            if (!send_all(client_fd, part_header.c_str(), part_header.size())) break;
            if (!send_all(client_fd, reinterpret_cast<const char*>(frame_copy.data()), frame_copy.size())) break;
            if (!send_all(client_fd, "\r\n", 2)) break;
            
            // FPS setup here - delay correlates with the frames directly
            std::this_thread::sleep_for(std::chrono::milliseconds(33)); 
        }

        close(client_fd);
        return;
    }

    std::string notfound =
        "HTTP/1.0 404 Not Found\r\nContent-Type: text/plain\r\n\r\n404 Not Found";
    send_all(client_fd, notfound.c_str(), notfound.size());
    close(client_fd);
}

int main(int argc, char** argv) {
    int device = 0;
    int port = PORT;
    if (argc >= 2) device = std::stoi(argv[1]);
    if (argc >= 3) port = std::stoi(argv[2]);

    std::cout << "Starting webcam streamer (device=" << device
              << ", port=" << port << ")\n";

    std::thread cam_thread(camera_thread_func, device, 640, 480, 30);

    int server_fd = ::socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) {
        perror("socket");
        running = false;
        cam_thread.join();
        return 1;
    }

    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(port);

    if (bind(server_fd, (sockaddr*)&addr, sizeof(addr)) < 0) {
        perror("bind");
        close(server_fd);
        running = false;
        cam_thread.join();
        return 1;
    }

    if (listen(server_fd, 10) < 0) {
        perror("listen");
        close(server_fd);
        running = false;
        cam_thread.join();
        return 1;
    }

    std::cout << "Listening on port " << port
              << ". Open http://localhost:" << port << "/\n";

    while (running) {
        sockaddr_in client_addr{};
        socklen_t client_len = sizeof(client_addr);
        int client_fd = accept(server_fd, (sockaddr*)&client_addr, &client_len);
        if (client_fd < 0) {
            if (errno == EINTR) continue;
            perror("accept");
            break;
        }
        std::thread(handle_client, client_fd).detach();
    }

    running = false;
    frame_cv.notify_all();
    close(server_fd);
    cam_thread.join();
    std::cout << "Server shut down.\n";
    return 0;
}