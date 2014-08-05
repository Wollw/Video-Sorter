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
bool compare_LT(const Vec3b &v, const Vec3b &w) {
    return (v[0] + v[1] +  v[2]) / 3 < (w[0] + w[1] + w[2]) / 3;
}
bool compare_GT(const Vec3b &v, const Vec3b &w) {
    return (v[0] + v[1] +  v[2]) / 3 > (w[0] + w[1] + w[2]) / 3;
}

DEFINE_bool(quiet, false, "Suppress terminal output.");
DEFINE_string(save, "", "Save output to file.");
DEFINE_string(axis, "x", "Axis of rotation.");
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

    video_file_filter(o, argv[0]);

    delete o->video_src;
    delete o->video_dst;
    free(o);

    return 0;
}

// grabbed and changed from http://www.algolist.net/Algorithms/Sorting/Quicksort
void quickSort(Mat r, int left, int right) {
    int i = left, j = right;
    Vec3b pivot = r.at<Vec3b>((left + right) / 2);

    /* partition */
    while (i <= j) {
        while (compare_LT(r.at<Vec3b>(i), pivot))
            i++;
        while (compare_GT(r.at<Vec3b>(j), pivot))
            j--;
        if (i <= j) {
            swap(r.at<Vec3b>(i), r.at<Vec3b>(j));
            i++;
            j--;
        }
    }

    /* recursion */
    if (left < j) {
        quickSort(r, left, j);
    }
    if (i < right) {
        quickSort(r, i, right);
    }
}

void sort_row(options_type *o, Mat r) {
    if (FLAGS_axis == "x")
        quickSort(r, 0, o->size.width-1);
    else
        quickSort(r, 0, o->size.height-1);
}


void *sort_rows_(void *vdata) {
    thread_data *data = (thread_data*)vdata;
    Mat f = data->frame;
    if (FLAGS_axis == "x") {
        for (int y = data->thread_id; y < data->o->size.height; y += data->thread_count) {
            sort_row(data->o, f.row(y));
        }
    } else {
        for (int x = data->thread_id; x < data->o->size.width; x += data->thread_count) {
            sort_row(data->o, f.col(x));
        }
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
        if (o->axis == AXIS_X || o->axis == AXIS_Y) {
            sort_rows(o, frame);
        } else {
            cout << "UNIMPLEMENTED" << endl;
            return -1;
        }

        o->video_dst->write(frame);
        printf("done.\n");

    }
}
