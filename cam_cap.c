
/*******************************************************************************
#             uvccapture: USB UVC Video Class Snapshot Software                #
#This package work with the Logitech UVC based webcams with the mjpeg feature  #
#.                                                                             #
# 	Orginally Copyright (C) 2005 2006 Laurent Pinchart &&  Michel Xhaard       #
#       Modifications Copyright (C) 2006  Gabriel A. Devenyi                   #
#                               (C) 2010  Alexandru Csete                      #
#                                                                              #
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

void sigcatch (int32_t sig)
{
    fprintf(stderr, "Exiting...\n");
    run = 0;
}

void usage (void)
{
    fprintf(stderr, "uvccapture version %s\n", version);
    fprintf(stderr, "Usage is: uvccapture [options]\n");
    fprintf(stderr, "Options:\n");
    fprintf(stderr, "-v\t\tVerbose (add more v's to be more verbose)\n");
    fprintf(stderr, "-o<filename>\tOutput filename (default: cam_cap_snap.jpg). Use img%%05d.jpg for sequential files.\n");
    fprintf(stderr, "-d<device>\tV4L2 Device (default: /dev/video1)\n");
    fprintf(stderr,
             "-x<width>\tImage Width (must be supported by device), default 1920x1080\n");
    fprintf(stderr,
             "-y<height>\tImage Height (must be supported by device), default 1920x1080\n");
    fprintf(stderr,
             "-c<command>\tCommand to run after each image capture(executed as <command> <output_filename>)\n");
    fprintf(stderr,
             "-j<integer>\tSkip <integer> frames before first capture\n");
    fprintf(stderr,
             "-t<integer>\tTake continuous shots with <integer> microseconds between them (0 for single shot), default is single shot\n");
    fprintf(stderr,
             "-n<integer>\tTake <integer> shots then exit. Only applicable when delay is non-zero\n");
    fprintf(stderr,
             "-q<percentage>\tJPEG Quality Compression Level (activates YUYV capture), default 90\n");
    fprintf(stderr, "-r\t\tUse read instead of mmap for image capture\n");
    fprintf(stderr,
             "-w\t\tWait for capture command to finish before starting next capture\n");
    fprintf(stderr, "-m\t\tToggles capture mode to YUYV capture\n");
    fprintf(stderr, "-f<format value>\t\tChange output format, 0-MJPEG, 1-YUYV, 2-BMP, default is BMP\n");
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

int32_t main (int32_t argc, char *argv[])
{
    char *videodevice = "/dev/video1";
    char *outputfile_prefix = "cam_cap_snap";
    char  thisfile[200]; /* used as filename buffer in multi-file seq. */
    int32_t formatIn = V4L2_PIX_FMT_MJPEG;
    int32_t formatOut = CAM_CAP_PIX_OUT_FMT_BMP;
    int32_t grabmethod = 1;
    int32_t width = 1920;
    int32_t height = 1080;
    int32_t brightness = 0, contrast = 0, saturation = 0, gain = 0;
    int32_t num = -1; /* number of images to capture */
    int32_t verbose = 0;
    int32_t delay = 0;
    int32_t skip = 0;
    int32_t quality = 95;
    int32_t i = 0;
    struct timeval ref_time, end_time;
    int32_t time_dur = 0;

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

        case 'f':
            {
                int32_t fmtOut = atoi(&argv[1][2]);
                switch (fmtOut) {
                case CAM_CAP_PIX_OUT_FMT_YUYV:
                case CAM_CAP_PIX_OUT_FMT_MJPEG:
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
    if (quality != 95)
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

    //Reset all camera controls
    if (verbose >= 1)
        fprintf(stderr, "Resetting camera settings\n");
    v4l2ResetControl (videoIn, V4L2_CID_BRIGHTNESS);
    v4l2ResetControl (videoIn, V4L2_CID_CONTRAST);
    v4l2ResetControl (videoIn, V4L2_CID_SATURATION);
    v4l2ResetControl (videoIn, V4L2_CID_GAIN);

    //Setup Camera Parameters
    if (brightness != 0) {
        if (verbose >= 1)
            fprintf(stderr, "Setting camera brightness to %d\n", brightness);
        v4l2SetControl (videoIn, V4L2_CID_BRIGHTNESS, brightness);
    } else if (verbose >= 1) {
        fprintf(stderr, "Camera brightness level is %d\n",
                 v4l2GetControl (videoIn, V4L2_CID_BRIGHTNESS));
    }
    if (contrast != 0) {
        if (verbose >= 1)
            fprintf(stderr, "Setting camera contrast to %d\n", contrast);
        v4l2SetControl (videoIn, V4L2_CID_CONTRAST, contrast);
    } else if (verbose >= 1) {
        fprintf(stderr, "Camera contrast level is %d\n",
                 v4l2GetControl (videoIn, V4L2_CID_CONTRAST));
    }
    if (saturation != 0) {
        if (verbose >= 1)
            fprintf(stderr, "Setting camera saturation to %d\n", saturation);
        v4l2SetControl (videoIn, V4L2_CID_SATURATION, saturation);
    } else if (verbose >= 1) {
        fprintf(stderr, "Camera saturation level is %d\n",
                 v4l2GetControl (videoIn, V4L2_CID_SATURATION));
    }
    if (gain != 0) {
        if (verbose >= 1)
            fprintf(stderr, "Setting camera gain to %d\n", gain);
        v4l2SetControl (videoIn, V4L2_CID_GAIN, gain);
    } else if (verbose >= 1) {
        fprintf(stderr, "Camera gain level is %d\n",
                 v4l2GetControl (videoIn, V4L2_CID_GAIN));
    }

    initLut();

    gettimeofday(&ref_time, NULL);
    while (run) {
        if (verbose >= 2)
            fprintf(stderr, "Grabbing frame\n");
        if (uvcGrab (videoIn) < 0) {
            fprintf(stderr, "Error grabbing\n");
            close_v4l2(videoIn);
            free(videoIn);
            freeLut();
            exit (1);
        }

        if (skip > 0) { skip--; continue; }

        gettimeofday(&end_time, NULL);
        time_dur = (end_time.tv_sec - ref_time.tv_sec) * 1000000 + (end_time.tv_usec - ref_time.tv_usec);
        if (time_dur > delay * 1000) {
            switch (formatOut) {
            case CAM_CAP_PIX_OUT_FMT_MJPEG:
            {
                if (delay > 0) {
                    sprintf(thisfile, "%s_%d", outputfile_prefix, i);
                    i++;
                    if (verbose >= 1)
                        fprintf(stderr, "Saving image to: %s\n", thisfile);
                        file = fopen (thisfile, "wb");
                } else {
                    sprintf(thisfile, "%s_0", outputfile_prefix);
                    if (verbose >= 1)
                        fprintf(stderr, "Saving image to: %s\n", thisfile);
                        file = fopen (thisfile, "wb");
                }

                if (NULL != file) {
                    switch (videoIn->formatIn) {
                    case V4L2_PIX_FMT_YUYV:
                        compress_yuyv_to_jpeg(videoIn, file, quality);
                        printf("Not supported yet!\n");
                        return 0;
                        break;
                    case V4L2_PIX_FMT_MJPEG:
                        fwrite(videoIn->tmpbuffer, videoIn->buf.bytesused + DHT_SIZE, 1,
                               file);
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
                case V4L2_PIX_FMT_MJPEG:
                    utils_get_picture_bmp(outputfile_prefix, videoIn->framebuffer,
                               videoIn->width, videoIn->height);
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

            gettimeofday(&ref_time, NULL);
        }
        if ((delay == 0) || (num == i))
            break;
    }
    close_v4l2 (videoIn);
    free (videoIn);
    freeLut();

    return 0;
}
