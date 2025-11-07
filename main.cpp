#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include <condition_variable>
#include <cstring>
#include <iostream>
#include <mutex>
#include <opencv2/opencv.hpp>
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

bool send_all_vec(int fd, const std::vector<char>& v) {
    return send_all(fd, v.data(), v.size());
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

        // === Overlay section ===
        int cx = frame.cols / 2;
        int cy = frame.rows / 2;

        // Draw crosshair lines (white, 2 px thick)
        cv::line(frame, cv::Point(cx - 20, cy), cv::Point(cx + 20, cy), cv::Scalar(255, 255, 255), 2);
        cv::line(frame, cv::Point(cx, cy - 20), cv::Point(cx, cy + 20), cv::Scalar(255, 255, 255), 2);

        // Add timestamp in top-left corner
        auto t = std::time(nullptr);
        std::tm tm = *std::localtime(&t);
        char buf[64];
        std::strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", &tm);
        std::string time_str(buf);

        cv::putText(frame, time_str, cv::Point(10, 30),
                    cv::FONT_HERSHEY_SIMPLEX, 0.8, cv::Scalar(0, 255, 255), 2);

        // === Encode to JPEG ===
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


std::string read_request_line(int client_fd) {
    std::string req;
    char c;
    // Read until CRLF
    while (true) {
        ssize_t r = ::recv(client_fd, &c, 1, 0);
        if (r <= 0) break;
        req.push_back(c);
        size_t n = req.size();
        if (n >= 2 && req[n-2] == '\r' && req[n-1] == '\n') break;
        if (req.size() > 4096) break;
    }
    return req;
}

void handle_client(int client_fd) {
    // Read the request header (simple parsing)
    std::string request;
    char buf[4096];
    ssize_t r = ::recv(client_fd, buf, sizeof(buf) - 1, 0);
    if (r <= 0) { close(client_fd); return; }
    buf[r] = '\0';
    request = std::string(buf);

    // Extract request line: e.g., "GET / HTTP/1.1"
    std::istringstream iss(request);
    std::string method, path, proto;
    iss >> method >> path >> proto;
    if (method.empty() || path.empty()) {
        close(client_fd);
        return;
    }

    if (path == "/" || path == "/index.html") {
        // Serve a simple HTML page (inlined)
        const std::string html =
            "HTTP/1.0 200 OK\r\n"
            "Content-Type: text/html; charset=UTF-8\r\n"
            "Cache-Control: no-cache, no-store, must-revalidate\r\n"
            "Pragma: no-cache\r\n"
            "Connection: close\r\n"
            "\r\n"
            "<!doctype html>\n"
            "<html>\n"
            "<head>\n"
            "  <meta charset=\"utf-8\">\n"
            "  <meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">\n"
            "  <title>Webcam MJPEG Stream</title>\n"
            "</head>\n"
            "<body>\n"
            "  <h1>Webcam MJPEG Stream</h1>\n"
            "  <p>If the image is blank, allow camera access on server or check device index.</p>\n"
            "  <img src=\"/stream\" style=\"max-width:100%;height:auto;\" />\n"
            "  <p>Open <code>http://localhost:8080/</code> in your browser.</p>\n"
            "</body>\n"
            "</html>\n";
        send_all(client_fd, html.c_str(), html.size());
        close(client_fd);
        return;
    } else if (path == "/stream") {
        // Send MJPEG headers
        std::string header = 
            "HTTP/1.0 200 OK\r\n"
            "Connection: close\r\n"
            "Cache-Control: no-cache\r\n"
            "Pragma: no-cache\r\n"
            "Content-Type: multipart/x-mixed-replace; boundary=" + BOUNDARY + "\r\n"
            "\r\n";
        if (!send_all(client_fd, header.c_str(), header.size())) {
            close(client_fd);
            return;
        }

        // Stream loop: send the latest_jpeg repeatedly
        while (running) {
            std::vector<uchar> frame_copy;
            {
                std::unique_lock<std::mutex> lk(frame_mutex);
                // Wait until first frame available or running becomes false
                frame_cv.wait_for(lk, std::chrono::milliseconds(500), [] { return !latest_jpeg.empty() || !running; });
                if (!running) break;
                frame_copy = latest_jpeg; // copy
            }
            if (frame_copy.empty()) continue;

            // Prepare part header
            std::ostringstream part;
            part << "--" << BOUNDARY << "\r\n"
                 << "Content-Type: image/jpeg\r\n"
                 << "Content-Length: " << frame_copy.size() << "\r\n"
                 << "\r\n";
            std::string partHeader = part.str();
            if (!send_all(client_fd, partHeader.c_str(), partHeader.size())) break;
            if (!send_all(client_fd, reinterpret_cast<const char*>(frame_copy.data()), frame_copy.size())) break;
            if (!send_all(client_fd, "\r\n", 2)) break;

            // Small sleep to pace sending; you can adjust to control bandwidth / fps
            std::this_thread::sleep_for(std::chrono::milliseconds(33)); // ~30fps
        }
        close(client_fd);
        return;
    } else {
        // Not found
        const std::string notfound =
            "HTTP/1.0 404 Not Found\r\n"
            "Content-Type: text/plain\r\n"
            "Connection: close\r\n"
            "\r\n"
            "404 Not Found\n";
        send_all(client_fd, notfound.c_str(), notfound.size());
        close(client_fd);
        return;
    }
}

int main(int argc, char** argv) {
    int device = 0;
    int port = PORT;
    if (argc >= 2) device = std::stoi(argv[1]);
    if (argc >= 3) port = std::stoi(argv[2]);

    std::cout << "Starting webcam streamer. Camera device: " << device << ", port: " << port << std::endl;

    std::thread cam_thread(camera_thread_func, device);

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

    std::cout << "Listening on port " << port << ". Open http://localhost:" << port << "/ in a browser.\n";

    // Accept loop
    while (running) {
        sockaddr_in client_addr;
        socklen_t client_len = sizeof(client_addr);
        int client_fd = accept(server_fd, (sockaddr*)&client_addr, &client_len);
        if (client_fd < 0) {
            if (errno == EINTR) continue;
            perror("accept");
            break;
        }
        std::thread t(handle_client, client_fd);
        t.detach();
    }

    // Shutdown
    running = false;
    frame_cv.notify_all();
    close(server_fd);
    cam_thread.join();
    std::cout << "Shutting down.\n";
    return 0;
}