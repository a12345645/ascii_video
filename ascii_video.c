#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <stdint.h>
#include <inttypes.h>
#include <math.h>
#include <ncurses.h>

#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/avutil.h>
#include <libswscale/swscale.h>
#include <libswresample/swresample.h>
#include <libavutil/imgutils.h>
#include <sys/timerfd.h>
#include <sys/select.h>
#include <time.h>
#include <signal.h>

#define NUM_COLORS 216
#define BRIGHTNESS 2.0
#define COLOR_RANGE (unsigned int)(51 * BRIGHTNESS)

#define BLOCK_ROWS 1
#define BLOCK_COLS 2

void sigint_handler(int sig)
{
    // 将颜色模式设置为默认模式
    use_default_colors();

    // 将前景色和背景色都设置为默认颜色
    assume_default_colors(-1, -1);
    endwin();
    // 可以在這裡進行相應的處理
}

#define SPEED_UP 4.0

int main(int argc, char *argv[])
{
    AVFormatContext *fmt_ctx = NULL;
    AVCodecContext *codec_ctx = NULL;
    AVCodec *codec = NULL;
    AVPacket pkt;
    AVFrame *frame = NULL;
    struct SwsContext *sws_ctx = NULL;
    int video_stream_idx = -1;
    int ret, got_frame;

    if (argc < 2)
    {
        fprintf(stderr, "Usage: %s <input_file>\n", argv[0]);
        exit(1);
    }

    av_register_all();

    if ((ret = avformat_open_input(&fmt_ctx, argv[1], NULL, NULL)) < 0)
    {
        fprintf(stderr, "Could not open input file '%s': %s\n", argv[1], av_err2str(ret));
        exit(1);
    }

    if ((ret = avformat_find_stream_info(fmt_ctx, NULL)) < 0)
    {
        fprintf(stderr, "Could not find stream information: %s\n", av_err2str(ret));
        exit(1);
    }

    av_dump_format(fmt_ctx, 0, argv[1], 0);

    for (int i = 0; i < fmt_ctx->nb_streams; i++)
    {
        if (fmt_ctx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO)
        {
            video_stream_idx = i;
            break;
        }
    }

    if (video_stream_idx == -1)
    {
        fprintf(stderr, "Could not find video stream\n");
        exit(1);
    }

    codec = avcodec_find_decoder(fmt_ctx->streams[video_stream_idx]->codecpar->codec_id);
    if (!codec)
    {
        fprintf(stderr, "Failed to find codec\n");
        exit(1);
    }

    codec_ctx = avcodec_alloc_context3(codec);
    if (!codec_ctx)
    {
        fprintf(stderr, "Failed to allocate codec context\n");
        exit(1);
    }

    if ((ret = avcodec_parameters_to_context(codec_ctx, fmt_ctx->streams[video_stream_idx]->codecpar)) < 0)
    {
        fprintf(stderr, "Failed to copy codec parameters to codec context: %s\n", av_err2str(ret));
        exit(1);
    }

    if ((ret = avcodec_open2(codec_ctx, codec, NULL)) < 0)
    {
        fprintf(stderr, "Failed to open codec: %s\n", av_err2str(ret));
        exit(1);
    }

    frame = av_frame_alloc();

    // Initialize ncurses
    initscr();
    start_color();

    signal(SIGINT, sigint_handler);

    int terminal_row, terminal_col;
    getmaxyx(stdscr, terminal_row, terminal_col);
    terminal_row /= BLOCK_ROWS;
    terminal_col /= BLOCK_COLS;

    double row_aspect_ratio = (double)terminal_row / codec_ctx->height, col_aspect_ratio = (double)terminal_col / codec_ctx->width;
    int width, height;

    if (row_aspect_ratio < col_aspect_ratio)
    {
        width = row_aspect_ratio * codec_ctx->width;
        height = row_aspect_ratio * codec_ctx->height;
    }
    else
    {
        width = col_aspect_ratio * codec_ctx->width;
        height = col_aspect_ratio * codec_ctx->height;
    }

    int row_offset = (terminal_row - height) * BLOCK_ROWS / 2, col_offset = (terminal_col - width) * BLOCK_COLS / 2;

    AVFrame *out_frame = NULL;

    out_frame = av_frame_alloc();
    if (!out_frame)
    {
        fprintf(stderr, "Failed to allocate gray frame\n");
        exit(1);
    }
    out_frame->format = AV_PIX_FMT_RGB24;
    out_frame->width = width;
    out_frame->height = height;
    av_frame_get_buffer(out_frame, 0);

    sws_ctx = sws_getContext(codec_ctx->width, codec_ctx->height, codec_ctx->pix_fmt,
                             width, height, AV_PIX_FMT_RGB24, SWS_BILINEAR, NULL, NULL, NULL);

    int timer_fd = timerfd_create(CLOCK_MONOTONIC, TFD_NONBLOCK);
    if (timer_fd == -1)
    {
        perror("Failed to create timerfd");
        exit(EXIT_FAILURE);
    }
    double fps = av_q2d(fmt_ctx->streams[video_stream_idx]->r_frame_rate) * SPEED_UP;
    struct itimerspec timer_spec = {
        .it_interval = {0, (1000000 / fps) * 1000},
        .it_value = {0, (1000000 / fps) * 1000}};
    if (timerfd_settime(timer_fd, 0, &timer_spec, NULL) == -1)
    {
        perror("Failed to set timerfd time");
        exit(EXIT_FAILURE);
    }

    // 监视 timerfd 文件描述符并等待计时器超时事件的发生
    fd_set fds;
    FD_ZERO(&fds);
    FD_SET(timer_fd, &fds);

    int index = 0;

    if (COLORS >= NUM_COLORS)
    {
        // 使用 216 种颜色
        int color_num = 0;
        for (int r = 0; r <= 5; r++)
        {
            for (int g = 0; g <= 5; g++)
            {
                for (int b = 0; b <= 5; b++)
                {
                    int r_val = r * COLOR_RANGE;
                    int g_val = g * COLOR_RANGE;
                    int b_val = b * COLOR_RANGE;
                    init_color(color_num, r_val, g_val, b_val);
                    init_pair(color_num + 1, COLOR_BLACK, color_num);
                    color_num++;
                }
            }
        }
    }
    else
    {
        // 使用 16 种颜色
        for (int i = 0; i < 16; i++)
        {
            init_pair(i + 1, i, COLOR_BLACK);
        }
    }

    while (av_read_frame(fmt_ctx, &pkt) >= 0)
    {
        if (pkt.stream_index == video_stream_idx)
        {
            ret = avcodec_send_packet(codec_ctx, &pkt);
            if (ret < 0)
            {
                fprintf(stderr, "Error sending packet for decoding: %s\n", av_err2str(ret));
                exit(1);
            }

            while (ret >= 0)
            {
                int nfds = select(timer_fd + 1, &fds, NULL, NULL, NULL);
                if (nfds == -1)
                {
                    perror("Failed to select on timerfd");
                    goto closed;
                }
                uint64_t data;
                if (read(timer_fd, &data, sizeof(uint64_t)) != sizeof(uint64_t))
                {
                    perror("Failed to read timerfd");
                    goto closed;
                }
                ret = avcodec_receive_frame(codec_ctx, frame);
                if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
                {
                    break;
                }
                else if (ret < 0)
                {
                    fprintf(stderr, "Error during decoding: %s\n", av_err2str(ret));
                    goto closed;
                }

                // Convert the frame to grayscale and scale it
                sws_scale(sws_ctx, (const uint8_t *const *)frame->data, frame->linesize,
                          0, codec_ctx->height, out_frame->data, out_frame->linesize);
                // Display the pixels of the frame
                // mvprintw(30, 30, "%d", ret);
                for (int y = 0; y < height; y++)
                {
                    for (int x = 0; x < width; x++)
                    {
                        uint8_t *rgb = &out_frame->data[0][y * out_frame->linesize[0] + x * 3];
                        uint8_t r = rgb[0];
                        uint8_t g = rgb[1];
                        uint8_t b = rgb[2];

                        int color_num;
                        if (COLORS >= NUM_COLORS)
                        {
                            // 使用 216 种颜色
                            int r_val = round(r * 5.0 / 255.0) * COLOR_RANGE;
                            int g_val = round(g * 5.0 / 255.0) * COLOR_RANGE;
                            int b_val = round(b * 5.0 / 255.0) * COLOR_RANGE;
                            color_num = r_val / COLOR_RANGE * 36 + g_val / COLOR_RANGE * 6 + b_val / COLOR_RANGE + 1;
                        }
                        else
                        {
                            // 使用 16 种颜色
                            if (r > 128 && g < 128 && b < 128)
                            {
                                color_num = 1; // 红色
                            }
                            else if (r < 128 && g > 128 && b < 128)
                            {
                                color_num = 2; // 绿色
                            }
                            else if (r < 128 && g < 128 && b > 128)
                            {
                                color_num = 3; // 蓝色
                            }
                            else if (r > 128 && g > 128 && b < 128)
                            {
                                color_num = 4; // 黄色
                            }
                            else if (r > 128 && g < 128 && b > 128)
                            {
                                color_num = 5; // 洋红色
                            }
                            else if (r < 128 && g > 128 && b > 128)
                            {
                                color_num = 6; // 青色
                            }
                            else
                            {
                                color_num = 7; // 白色
                            }
                        }

                        attron(COLOR_PAIR(color_num));
                        for (int k = 0; k < BLOCK_ROWS; k++)
                        {
                            for (int l = 0; l < BLOCK_COLS; l++)
                            {
                                mvaddch(row_offset + k + y * BLOCK_ROWS, col_offset + l + x * BLOCK_COLS, ' ');
                            }
                        }
                        // mvaddch(y, x, ' ');
                        attroff(COLOR_PAIR(color_num));
                        // mvprintw(30, 30, "%d", color_pair_num);
                    }
                }
                refresh();
                // getch();
                av_frame_unref(frame);
            }
        }
        av_packet_unref(&pkt);
    }
closed:
    av_frame_free(&frame);
    av_frame_free(&out_frame);
    avcodec_free_context(&codec_ctx);
    avformat_close_input(&fmt_ctx);
    start_color();
    endwin();
}