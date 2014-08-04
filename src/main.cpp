#include <algorithm>
#include <signal.h>
#include <cv.h>
#include <highgui.h>
#include <iostream>
#include <cstdlib>
#include <cstdio>
#include <list>
#include <unistd.h>

#include <gflags/gflags.h>

using namespace std;
using namespace cv;

// Comparison function
bool compare(const Vec3b &v, const Vec3b &w) {
    return v[0] < w[1];
}

DEFINE_bool(display, false, "Display video in window.");
DEFINE_bool(quiet, false, "Suppress terminal output.");
DEFINE_string(save, "", "Save output to file.");
DEFINE_string(axis, "y", "Axis of rotation.");
DEFINE_double(fps, 24, "Frame per second for output.");
DEFINE_double(threadcount, 4, "Thread count.");

enum axis_enum {
    AXIS_X,
    AXIS_Y,
    AXIS_T
};

typedef struct options_struct {
    enum axis_enum axis;
    Size size;
    VideoCapture *video_src;
    VideoWriter *video_dst;
} options_type;

typedef struct {
    unsigned int thread_id;
    unsigned int thread_count;
    Mat frame;
    options_type *o;
} thread_data;

int video_file_filter(options_type *o, char *window_name);

options_type * create_options(int *argc, char ***argv) {
    google::ParseCommandLineFlags(argc, argv, true);
    options_type *o = (options_type *) calloc(1, sizeof(options_type));
    
    if (*argc != 2) {
        cout << "usage: " << *argv[0] << " input-video" << endl;
        free(o);
        return NULL;
    }

    string input_file = (*argv)[(*argc) - 1];
    o->video_src = new VideoCapture(input_file);
    if(!o->video_src->isOpened()) return NULL;

    // get axis
    if (FLAGS_axis == "x") {
        o->axis = AXIS_X;
    } else if (FLAGS_axis == "y") {
        o->axis = AXIS_Y;
    } else {
        free(o);
        return NULL;
    }

    // get size of output video
    int width = o->video_src->get(CV_CAP_PROP_FRAME_WIDTH);
    int height = o->video_src->get(CV_CAP_PROP_FRAME_HEIGHT);
    o->size = Size(width, height);

    // get output video
    if (!FLAGS_save.empty()) {
        o->video_dst = new VideoWriter;
        int ex = CV_FOURCC('I', 'Y', 'U', 'V');
        o->video_dst->open(FLAGS_save, ex, FLAGS_fps, o->size);
    } else {
        cout << "Output file required." << endl;
        return NULL;
    }

    return o;

}

int main(int argc, char **argv) {
    setbuf(stdout, NULL);

    google::SetUsageMessage("Rotate video in time.");

    options_type *o = create_options(&argc, &argv);
    if (o == NULL)
        return -1;

    if (FLAGS_display) {
        namedWindow(argv[0],CV_WINDOW_NORMAL);
        setWindowProperty(argv[0], CV_WND_PROP_FULLSCREEN, CV_WINDOW_FULLSCREEN);
    }

    video_file_filter(o, argv[0]);

    delete o->video_src;
    delete o->video_dst;
    free(o);

    return 0;
}

void sort_row(options_type *o, Mat r) {
    int iMin;
    int n = o->size.width;
    for (int j = 0; j < n; j++) {
        iMin = j;
        for (int i = j + 1; i < n; i++) {
            if (compare(r.at<Vec3b>(i), r.at<Vec3b>(iMin)))
                iMin = i;
        }
        if (iMin != j) {
            swap(r.at<Vec3b>(j), r.at<Vec3b>(iMin));
        }
    }
}

void *sort_rows_(void *vdata) {
    thread_data *data = (thread_data*)vdata;
    Mat f = data->frame;
    for (int y = data->thread_id; y < data->o->size.height;  y += data->thread_count) {
        sort_row(data->o, f.row(y));
    }
}

void sort_rows(options_type *o, Mat f) {
    int thread_count = FLAGS_threadcount;
    pthread_t threads[thread_count];
    int irets[thread_count];
    thread_data data[thread_count];

    for (unsigned int i = 0; i < thread_count; i++) {
        data[i] = thread_data{i, thread_count, f, o};
        irets[i] = pthread_create(&threads[i], NULL, sort_rows_, &data[i]);
        if (irets[i]) {
            fprintf(stderr,"Error - pthread_create() return code: %d\n",irets[i]);
            exit(EXIT_FAILURE);
        }
    }

    for (unsigned int i = 0; i < thread_count; i++) {
        pthread_join(threads[i], NULL);
    }
}


int video_file_filter(options_type *o, char *window_name) {
    Mat frame;
    *(o->video_src) >> frame;
    Mat frame_out(o->size, frame.type());
    o->video_src->set(CV_CAP_PROP_POS_FRAMES, 0);

    int frame_count = o->video_src->get(CV_CAP_PROP_FRAME_COUNT);
    for (int j = 0; j < frame_count; j++) {
        *(o->video_src) >> frame;
        // Output status to console
        if (!FLAGS_quiet)
            printf("Frame %d of %d...",j + 1, frame_count);

        // Create a new frame.
        switch (o->axis) {
            case AXIS_X:
                sort_rows(o, frame);
                break;
            case AXIS_Y:
                break;
            default:
                cout << "UNIMPLEMENTED" << endl;
                return -1;
        }

        // Display window
        if (FLAGS_display) {
            imshow(window_name, frame);
            if (waitKey(1) == '')
                return 0;
        }

        // Save frame to file
        if (!FLAGS_save.empty()) {
            printf("writing...");
            o->video_dst->write(frame);
        }
        
        printf("done.\n");

    }
}
