#include <opencv2/opencv.hpp>
#include <boost/asio.hpp>
#include <thread>
#include <vector>

using boost::asio::ip::tcp;

const std::string boundary = "--frame";
const int PORT = 8080;

void streamWebcam(tcp::socket socket) {
    try {
        cv::VideoCapture cap(0);  // Open USB webcam
        if (!cap.isOpened()) return;

        // HTTP headers for MJPEG stream
        std::string header = "HTTP/1.0 200 OK\r\n"
                             "Content-Type: multipart/x-mixed-replace; boundary=" + boundary + "\r\n\r\n";
        boost::asio::write(socket, boost::asio::buffer(header));

        cv::Mat frame;
        std::vector<uchar> buf;
        while (true) {
            cap >> frame;
            if (frame.empty()) break;
            cv::imencode(".jpg", frame, buf);

            std::string part_header = boundary + "\r\n"
                                      "Content-Type: image/jpeg\r\n"
                                      "Content-Length: " + std::to_string(buf.size()) + "\r\n\r\n";

            boost::asio::write(socket, boost::asio::buffer(part_header));
            boost::asio::write(socket, boost::asio::buffer(buf));
            boost::asio::write(socket, boost::asio::buffer("\r\n"));

            // Small delay for frame rate control
            std::this_thread::sleep_for(std::chrono::milliseconds(33)); // ~30 FPS
        }
    } catch (...) {
        // Handle exceptions or socket errors
    }
    socket.close();
}

int main() {
    try {
        boost::asio::io_context io_context;
        tcp::acceptor acceptor(io_context, tcp::endpoint(tcp::v4(), PORT));

        while (true) {
            tcp::socket socket(io_context);
            acceptor.accept(socket);

            // For each client, respond with MJPEG stream on a new thread
            std::thread(streamWebcam, std::move(socket)).detach();
        }
    } catch (std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
    }
    return 0;
}
