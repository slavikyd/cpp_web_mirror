#include <gst/gst.h>
#include <glib.h>
#include <signal.h>

static GMainLoop *loop;

static void int_handler(int signo) {
    if (loop != nullptr) {
        g_main_loop_quit(loop);
    }
}

int main(int argc, char *argv[]) {
    gst_init(&argc, &argv);

    GMainLoop *local_loop = g_main_loop_new(NULL, FALSE);
    loop = local_loop;

    // Create GStreamer elements
    GstElement *pipeline = gst_pipeline_new("webcam-pipeline");
    GstElement *source = gst_element_factory_make("autovideosrc", "source");
    GstElement *convert = gst_element_factory_make("videoconvert", "convert");
    GstElement *scale = gst_element_factory_make("videoscale", "scale");
    GstElement *capsfilter = gst_element_factory_make("capsfilter", "capsfilter");
    GstElement *encoder = gst_element_factory_make("x264enc", "encoder");
    GstElement *payloader = gst_element_factory_make("rtph264pay", "payloader");
    GstElement *sink = gst_element_factory_make("udpsink", "sink");

    if (!pipeline || !source || !convert || !scale || !capsfilter || !encoder || !payloader || !sink) {
        g_printerr("Failed to create GStreamer elements\n");
        return -1;
    }

    // Set properties
#ifdef _WIN32
    g_object_set(source, "device-index", 0, NULL); // Windows: first webcam
#else
    g_object_set(source, "device", "/dev/video0", NULL); // Linux device
#endif

    GstCaps *caps = gst_caps_new_simple(
        "video/x-raw",
        "width", G_TYPE_INT, 640,
        "height", G_TYPE_INT, 480,
        "framerate", GST_TYPE_FRACTION, 30, 1,
        NULL);
    g_object_set(capsfilter, "caps", caps, NULL);
    gst_caps_unref(caps);

    g_object_set(encoder, "tune", 0x00000004, NULL); // zerolatency
    g_object_set(sink, "host", "127.0.0.1", "port", 5000, NULL);

    // Build pipeline
    gst_bin_add_many(GST_BIN(pipeline), source, convert, scale, capsfilter, encoder, payloader, sink, NULL);

    if (!gst_element_link_many(source, convert, scale, capsfilter, encoder, payloader, sink, NULL)) {
        g_printerr("Failed to link elements\n");
        gst_object_unref(pipeline);
        return -1;
    }

    // Setup signal handlers to exit cleanly
    signal(SIGINT, int_handler);
    signal(SIGTERM, int_handler);

    gst_element_set_state(pipeline, GST_STATE_PLAYING);
    g_print("Streaming webcam UDP RTP to udp://127.0.0.1:5000\n");

    g_main_loop_run(loop);

    // Cleanup
    gst_element_set_state(pipeline, GST_STATE_NULL);
    gst_object_unref(pipeline);
    g_main_loop_unref(loop);
    g_print("Pipeline stopped and cleaned up\n");

    return 0;
}
