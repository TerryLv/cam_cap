
/*******************************************************************************
#             cam_cap: USB UVC Video Class Snapshot Software                #
#                                                                             #
# This program is free software; you can redistribute it and/or modify         #
# it under the terms of the GNU General Public License as published by         #
# the Free Software Foundation; either version 2 of the License, or            #
# (at your option) any later version.                                          #
#                                                                              #
# This program is distributed in the hope that it will be useful,              #
# but WITHOUT ANY WARRANTY; without even the implied warranty of               #
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the                #
# GNU General Public License for more details.                                 #
#                                                                              #
# You should have received a copy of the GNU General Public License            #
# along with this program; if not, write to the Free Software                  #
# Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA    #
#                                                                              #
*******************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <jpeglib.h>
#include <time.h>
#include <sys/time.h>
#include <linux/videodev2.h>

#include "v4l2uvc.h"
#include "cam_cap.h"
#include "utils.h"
#include "color.h"

static const char version[] = VERSION;
int32_t run = 1;

#define CAM_V4L2_PARAMS_NUM     (13)

typedef struct {
    char *param_name;
    int  param_cid;
} cam_v4l2_param;

static cam_v4l2_param cam_v4l2_param_group[CAM_V4L2_PARAMS_NUM] = {
    { "Brightness", V4L2_CID_BRIGHTNESS },
    { "Contrast",   V4L2_CID_CONTRAST },
    { "Saturation", V4L2_CID_SATURATION },
    { "Hue",        V4L2_CID_HUE },
    { "Sharpness",  V4L2_CID_SHARPNESS },
    { "Gain",       V4L2_CID_GAIN },
    { "Gamma",      V4L2_CID_GAMMA },
    { "Exposure Auto", V4L2_CID_EXPOSURE_AUTO },
    { "Exposure Absolute", V4L2_CID_EXPOSURE_ABSOLUTE },
    { "White Balance", V4L2_CID_AUTO_WHITE_BALANCE },
    { "White Balance Balance", V4L2_CID_WHITE_BALANCE_TEMPERATURE },
    { "Focus Auto", V4L2_CID_FOCUS_AUTO },
    { "Focus Absolute", V4L2_CID_FOCUS_ABSOLUTE }
};

void sigcatch (int32_t sig)
{
    fprintf(stderr, "Exiting...\n");
    run = 0;
}

void usage (void)
{
    fprintf(stderr, "cam_cap version %s\n", version);
    fprintf(stderr, "Usage is: cam_cap [options]\n");
    fprintf(stderr, "Options:\n");
    fprintf(stderr, "-v\t\tVerbose (add more v's to be more verbose)\n");
    fprintf(stderr, "-o<filename>\tOutput filename prefix(default: cam_cap_snap_xxx.jpg).\n");
    fprintf(stderr, "-d<device>\tV4L2 Device (default: /dev/video1)\n");
    fprintf(stderr,
             "-x<width>\tImage Width (must be supported by device), default 640x480\n");
    fprintf(stderr,
             "-y<height>\tImage Height (must be supported by device), default 640x480\n");
    fprintf(stderr,
             "-j<integer>\tSkip <integer> frames before first capture\n");
    fprintf(stderr,
             "-t<integer>\tTake continuous shots with <integer> microseconds between them (0 for single shot), default is single shot\n");
    fprintf(stderr,
             "-T\t\tTest capture speed, -n must be set with this option\n");
    fprintf(stderr,
             "-n<integer>\tTake <integer> shots then exit. If delay is defined, it will do capture with delay interval, Or, it will do capture continuously\n");
    fprintf(stderr,
             "-q<percentage>\tJPEG Quality Compression Level (activates YUYV capture), default 95\n");
    fprintf(stderr, "-r\t\tUse read instead of mmap for image capture\n");
    fprintf(stderr,
             "-w\t\tWait for capture command to finish before starting next capture\n");
    fprintf(stderr, "-m\t\tToggles capture mode to YUYV capture\n");
    fprintf(stderr, "-f<format>\tChange output format, 0-JPEG, 1-YUYV, 2-BMP, default is JPEG\n");
    fprintf(stderr, "Camera Settings:\n");
    fprintf(stderr, "-B<integer>\tBrightness\n");
    fprintf(stderr, "-C<integer>\tContrast\n");
    fprintf(stderr, "-S<integer>\tSaturation\n");
    fprintf(stderr, "-G<integer>\tGain\n");
    exit (8);
}

int32_t compress_yuyv_to_jpeg (struct vdIn *vd, FILE * file, int32_t quality)
{
    struct jpeg_compress_struct cinfo;
    struct jpeg_error_mgr jerr;
    JSAMPROW row_pointer[1];
    unsigned char *line_buffer, *yuyv;
    int32_t z;

    fprintf(stderr, "Compressing YUYV frame to JPEG image.\n");

    line_buffer = calloc (vd->width * 3, 1);
    yuyv = vd->framebuffer;

    cinfo.err = jpeg_std_error (&jerr);
    jpeg_create_compress (&cinfo);
    jpeg_stdio_dest (&cinfo, file);

    cinfo.image_width = vd->width;
    cinfo.image_height = vd->height;
    cinfo.input_components = 3;
    cinfo.in_color_space = JCS_RGB;

    jpeg_set_defaults (&cinfo);
    jpeg_set_quality (&cinfo, quality, TRUE);

    jpeg_start_compress (&cinfo, TRUE);

    z = 0;
    while (cinfo.next_scanline < cinfo.image_height) {
        int32_t x;
        unsigned char *ptr = line_buffer;

        for (x = 0; x < vd->width; x++) {
            int32_t r, g, b;
            int32_t y, u, v;

            if (!z)
                y = yuyv[0] << 8;
            else
                y = yuyv[2] << 8;
            u = yuyv[1] - 128;
            v = yuyv[3] - 128;

            r = (y + (359 * v)) >> 8;
            g = (y - (88 * u) - (183 * v)) >> 8;
            b = (y + (454 * u)) >> 8;

            *(ptr++) = (r > 255) ? 255 : ((r < 0) ? 0 : r);
            *(ptr++) = (g > 255) ? 255 : ((g < 0) ? 0 : g);
            *(ptr++) = (b > 255) ? 255 : ((b < 0) ? 0 : b);

            if (z++) {
                z = 0;
                yuyv += 4;
            }
        }

        row_pointer[0] = line_buffer;
        jpeg_write_scanlines (&cinfo, row_pointer, 1);
    }

    jpeg_finish_compress (&cinfo);
    jpeg_destroy_compress (&cinfo);

    free (line_buffer);

    return (0);
}

static int32_t cam_cap_print_cam_parameters(struct vdIn *vd)
{
    int tmp = 0;

    fprintf(stderr, "Camera settings: \n");
    for (tmp = 0; tmp < CAM_V4L2_PARAMS_NUM; ++tmp) {
        int param_cid = cam_v4l2_param_group[tmp].param_cid;
        int value_g = 0;

        if (!v4l2GetControl(vd, param_cid, &value_g))
            fprintf(stderr, "%s: %d\r\n", cam_v4l2_param_group[tmp].param_name, value_g);
        else
            fprintf(stderr, "%s: Failed!\r\n", cam_v4l2_param_group[tmp].param_name);
    }
    return 0;
}

int32_t main (int32_t argc, char *argv[])
{
    char *videodevice = "/dev/video1";
    char *outputfile_prefix = "cam_cap_snap";
    char  thisfile[200] = { 0 }; /* used as filename buffer in multi-file seq. */
    int32_t formatIn = V4L2_PIX_FMT_MJPEG;
    int32_t formatOut = CAM_CAP_PIX_OUT_FMT_JPEG;
    int32_t grabmethod = 1;
    int32_t width = 640;
    int32_t height = 480;
    int32_t brightness = 0, contrast = 0, saturation = 0, gain = 0;
    int32_t num = -1; /* number of images to capture */
    int32_t verbose = 0;
    int32_t delay = 0;
    int32_t skip = 0;
    int32_t quality = 95;
    int32_t frame_num = 0;
    struct timeval delay_ref_time, delay_end_time;
    struct timeval spd_tst_start_time, spd_tst_end_time;
    int32_t time_dur = 0;
    int32_t query = 0;
    int32_t speed_tst= 0;

    struct vdIn *videoIn;
    FILE *file;

    (void)signal (SIGINT, sigcatch);
    (void)signal (SIGQUIT, sigcatch);
    (void)signal (SIGKILL, sigcatch);
    (void)signal (SIGTERM, sigcatch);
    (void)signal (SIGABRT, sigcatch);
    (void)signal (SIGTRAP, sigcatch);

    //Options Parsing (FIXME)
    while ((argc > 1) && (argv[1][0] == '-')) {
        switch (argv[1][1]) {
        case 'v':
            verbose++;
            break;

        case 'o':
            outputfile_prefix = &argv[1][2];
            break;

        case 'd':
            videodevice = &argv[1][2];
            break;

        case 'x':
            width = atoi(&argv[1][2]);
            break;

        case 'y':
            height = atoi(&argv[1][2]);
            break;

        case 'r':
            grabmethod = 0;
            break;

        case 'm':
            formatIn = V4L2_PIX_FMT_YUYV;
            break;

        case 'Q':
            query = 1;
            break;

        case 'T':
            speed_tst = 1;
            break;

        case 'f':
            {
                int32_t fmtOut = atoi(&argv[1][2]);
                switch (fmtOut) {
                case CAM_CAP_PIX_OUT_FMT_YUYV:
                case CAM_CAP_PIX_OUT_FMT_JPEG:
                case CAM_CAP_PIX_OUT_FMT_BMP:
                    formatOut = fmtOut;
                    break;
                default:
                    printf("Unrecognized output format!\n");
                    return 1;
                }
            }
            break;

        case 'n':
            num = atoi(&argv[1][2]);

            break;

        case 'j':
            skip = atoi(&argv[1][2]);
            if (skip < 0) {
                printf("Unsupported skip value: %d\n", skip);
                return -1;
            }

            break;

        case 't':
            delay = atoi(&argv[1][2]);
            if (delay < 0) {
                printf("Unsupported delay value: %d\n", delay);
                return -1;
            }
            break;

        case 'B':
            brightness = atoi(&argv[1][2]);
            break;

        case 'C':
            contrast = atoi(&argv[1][2]);
            break;

        case 'S':
            saturation = atoi(&argv[1][2]);
            break;

        case 'G':
            gain = atoi(&argv[1][2]);
            break;

        case 'q':
            quality = atoi(&argv[1][2]);
            break;

        case 'h':
            usage();
            break;

        default:
            fprintf(stderr, "Unknown option %s \n", argv[1]);
            usage();
        }
        ++argv;
        --argc;
    }


    /* user requrested quality activates YUYV mode */
    if (quality > 95)
        formatIn = V4L2_PIX_FMT_YUYV;

    if (verbose >= 1) {
        fprintf(stderr, "Using videodevice: %s\n", videodevice);
        fprintf(stderr, "Saving images with prefix: %s\n", outputfile_prefix);
        fprintf(stderr, "Image size: %dx%d\n", width, height);
        if (delay > 0)
            fprintf(stderr, "Taking snapshot every %d microsecond\n", delay);
        else if (0 == delay)
            fprintf(stderr, "Taking single snapshot\n");
        else
            fprintf(stderr, "Invalid delay value: %d\n", delay); 
        if (grabmethod == 1)
            fprintf(stderr, "Taking images using mmap\n");
        else
            fprintf(stderr, "Taking images using read\n");
    }
    videoIn = (struct vdIn *) calloc(1, sizeof (struct vdIn));
    if (init_videoIn
        (videoIn, (char *) videodevice, width, height, formatIn, formatOut, grabmethod) < 0)
        exit (1);

    if (1 == query) {
        struct v4l2_queryctrl query_ctrl;

        fprintf(stderr, "Using videodevice: %s\n", videodevice);
        fprintf(stderr, "Saving images with prefix: %s\n", outputfile_prefix);
        fprintf(stderr, "Image size: %dx%d\n", width, height);
        if (delay > 0)
            fprintf(stderr, "Taking snapshot every %d microsecond\n", delay);
        else if (0 == delay)
            fprintf(stderr, "Taking single snapshot\n");
        else
            fprintf(stderr, "Invalid delay value: %d\n", delay); 
        if (grabmethod == 1)
            fprintf(stderr, "Taking images using mmap\n");
        else
            fprintf(stderr, "Taking images using read\n");

        v4l2QueryControl(videoIn, V4L2_CID_BRIGHTNESS, &query_ctrl);
        fprintf(stderr, "Brightness:\n\tDefault: %d\n\tMax: %d\n\tMin:%d\n",
                query_ctrl.default_value,
                query_ctrl.maximum,
                query_ctrl.minimum);
        v4l2QueryControl(videoIn, V4L2_CID_CONTRAST, &query_ctrl);
        fprintf(stderr, "Contrast:\n\tDefault: %d\n\tMax: %d\n\tMin:%d\n",
                query_ctrl.default_value,
                query_ctrl.maximum,
                query_ctrl.minimum);
        v4l2QueryControl(videoIn, V4L2_CID_SATURATION, &query_ctrl);
        fprintf(stderr, "Saturation:\n\tDefault: %d\n\tMax: %d\n\tMin:%d\n",
                query_ctrl.default_value,
                query_ctrl.maximum,
                query_ctrl.minimum);
        v4l2QueryControl(videoIn, V4L2_CID_GAIN, &query_ctrl);
        fprintf(stderr, "Gain:\n\tDefault: %d\n\tMax: %d\n\tMin:%d\n",
                query_ctrl.default_value,
                query_ctrl.maximum,
                query_ctrl.minimum);

        return 0;
    }

#if 0
    v4l2ResetControl (videoIn, V4L2_CID_BRIGHTNESS);
    v4l2ResetControl (videoIn, V4L2_CID_CONTRAST);
    v4l2ResetControl (videoIn, V4L2_CID_SATURATION);
    v4l2ResetControl (videoIn, V4L2_CID_GAIN);
#endif

    //Setup Camera Parameters
    if (brightness != 0) {
        v4l2ResetControl (videoIn, V4L2_CID_BRIGHTNESS);
        if (verbose >= 1)
            fprintf(stderr, "Setting camera brightness to %d\n", brightness);
        v4l2SetControl (videoIn, V4L2_CID_BRIGHTNESS, brightness);
    } else if (verbose >= 1) {
        int tmp = 0;
        v4l2GetControl (videoIn, V4L2_CID_BRIGHTNESS, &tmp);
        fprintf(stderr, "Camera brightness level is %d\n", tmp);
    }

    if (contrast != 0) {
        v4l2ResetControl (videoIn, V4L2_CID_CONTRAST);
        if (verbose >= 1)
            fprintf(stderr, "Setting camera contrast to %d\n", contrast);
        v4l2SetControl (videoIn, V4L2_CID_CONTRAST, contrast);
    } else if (verbose >= 1) {
        int tmp = 0;
        v4l2GetControl (videoIn, V4L2_CID_CONTRAST, &tmp);
        fprintf(stderr, "Camera contrast level is %d\n", tmp);
    }

    if (saturation != 0) {
        v4l2ResetControl (videoIn, V4L2_CID_SATURATION);
        if (verbose >= 1)
            fprintf(stderr, "Setting camera saturation to %d\n", saturation);
        v4l2SetControl (videoIn, V4L2_CID_SATURATION, saturation);
    } else if (verbose >= 1) {
        int tmp = 0;
         v4l2GetControl (videoIn, V4L2_CID_SATURATION, &tmp);
        fprintf(stderr, "Camera saturation level is %d\n", tmp);
    }

    if (gain != 0) {
        v4l2ResetControl (videoIn, V4L2_CID_GAIN);
        if (verbose >= 1)
            fprintf(stderr, "Setting camera gain to %d\n", gain);
        v4l2SetControl (videoIn, V4L2_CID_GAIN, gain);
    } else if (verbose >= 1) {
        int tmp = 0;
        v4l2GetControl (videoIn, V4L2_CID_GAIN, &tmp);
        fprintf(stderr, "Camera gain level is %d\n", tmp);
    }

    initLut();

    gettimeofday(&delay_ref_time, NULL);
    while (run) {
        if (verbose >= 2)
            fprintf(stderr, "Grabbing frame\n");

        if (verbose >= 3)
        {
            /* print camera parameters */ 
            cam_cap_print_cam_parameters(videoIn);
        }

        if (1 == speed_tst)
            gettimeofday(&spd_tst_start_time, NULL);
        if (uvcGrab (videoIn) < 0) {
            fprintf(stderr, "Error grabbing\n");
            close_v4l2(videoIn);
            free(videoIn);
            freeLut();
            exit (1);
        }

        if (skip > 0) { skip--; continue; }

        gettimeofday(&delay_end_time, NULL);
        time_dur = (delay_end_time.tv_sec - delay_ref_time.tv_sec) * 1000000 + (delay_end_time.tv_usec - delay_ref_time.tv_usec);
        if ((time_dur > delay * 1000) || (frame_num < num)) {
            switch (formatOut) {
            case CAM_CAP_PIX_OUT_FMT_JPEG:
            {
                if (delay > 0) {
                    sprintf(thisfile, "%s_%d", outputfile_prefix, frame_num);
                    if (verbose >= 1)
                        fprintf(stderr, "Saving image to: %s\n", thisfile);
                } else {
                    utils_get_picture_name(thisfile, outputfile_prefix, 1);
                    if (verbose >= 1)
                        fprintf(stderr, "Saving image to: %s\n", thisfile);
                }
                file = fopen (thisfile, "wb");

                if (NULL != file) {
                    switch (videoIn->formatIn) {
                    case V4L2_PIX_FMT_YUYV:
                        compress_yuyv_to_jpeg(videoIn, file, quality);
                        return 0;
                        break;
                    case V4L2_PIX_FMT_MJPEG:
#if 0
                        fwrite(videoIn->tmpbuffer, videoIn->tmpbuf_byteused + DHT_SIZE, 1,
                               file);
#endif
                        utils_get_picture_jpg(file, videoIn->tmpbuffer, videoIn->tmpbuf_byteused);
                        break;
                    default:
                        fprintf(stderr, "Unrecgnized input format!\n");
                        break;
                    }
                }
            }
            fclose(file);
            break;
            case CAM_CAP_PIX_OUT_FMT_YUYV:
            {
                switch (videoIn->formatIn) {
                case V4L2_PIX_FMT_YUYV:
                    utils_get_picture_yv2(outputfile_prefix, videoIn->framebuffer,
                             videoIn->width, videoIn->height);
                    break;
                case V4L2_PIX_FMT_MJPEG:
                    /* Compress to mjpg */
                    utils_get_picture_mjpg(outputfile_prefix, videoIn->tmpbuffer,
                             videoIn->tmpbuf_byteused);
                    break;
                default:
                    fprintf(stderr, "Unrecgnized input format!\n");
                    break;
                }                      
            }
            break;
            case CAM_CAP_PIX_OUT_FMT_BMP:
            {
                switch (videoIn->formatIn) {
                case V4L2_PIX_FMT_YUYV:
                {
#if 0
                    if (delay > 0) {
                        sprintf(thisfile, "%s_%d", outputfile_prefix, frame_num);
                        if (verbose >= 1)
                            fprintf(stderr, "Saving image to: %s\n", thisfile);
                    } else {
                        utils_get_picture_name(thisfile, outputfile_prefix, 2);
                        if (verbose >= 1)
                            fprintf(stderr, "Saving image to: %s\n", thisfile);
                    }
#endif
                    fprintf(stderr, "not supported yet!\n");
                }
                break;
                case V4L2_PIX_FMT_MJPEG:
                    fprintf(stderr, "not supported yet!\n");
                    break;
                default:
                    fprintf(stderr, "Unrecgnized input format!\n");
                    break;
                }
            }
            break;
            default:
                fprintf(stderr, "Unrecgnized output format!\n");
                break;
            }

            gettimeofday(&delay_ref_time, NULL);
        }
        if (1 == speed_tst) {
            gettimeofday(&spd_tst_end_time, NULL);
            time_dur = (spd_tst_end_time.tv_sec - spd_tst_start_time.tv_sec) * 1000000 + (spd_tst_end_time.tv_usec - spd_tst_start_time.tv_usec);
            fprintf(stderr, "Frame %d time consume: %dus\n", frame_num, time_dur);
        }
        if ((delay == 0) && (num <= 0))
            break;
        if (num == frame_num)
            break;

        frame_num++;

    }
    close_v4l2 (videoIn);
    free (videoIn);
    freeLut();

    return 0;
}
