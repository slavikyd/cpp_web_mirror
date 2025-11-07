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

    // Build pipeline description:
    // Capture webcam → Video conversion → Caps filter (resolution fps) → VP8 encoder → WebM mux → HTTP server sink
    const gchar *pipeline_desc =
        "v4l2src device=/dev/video0 ! "
        "videoconvert ! "
        "video/x-raw,width=640,height=480,framerate=30/1 ! "
        "vp8enc deadline=1 ! "
        "webmmux streamable=true name=mux ! "
        "tcpserversink host=0.0.0.0 port=8080";

    GError *error = NULL;
    GstElement *pipeline = gst_parse_launch(pipeline_desc, &error);

    if (!pipeline) {
        g_print("Failed to create pipeline: %s\n", error->message);
        g_error_free(error);
        return -1;
    }

    g_print("Streaming webcam on http://localhost:8080\n");

    gst_element_set_state(pipeline, GST_STATE_PLAYING);
    g_main_loop_run(loop);

    gst_element_set_state(pipeline, GST_STATE_NULL);
    gst_object_unref(pipeline);
    g_main_loop_unref(loop);

    return 0;
}
