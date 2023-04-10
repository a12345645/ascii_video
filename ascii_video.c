#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <inttypes.h>
#include <math.h>
#include <ncurses.h>
#include <unistd.h>

#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/avutil.h>
#include <libswscale/swscale.h>
#include <libswresample/swresample.h>
#include <libavutil/imgutils.h>

#define NUM_COLORS 216

int main(int argc, char *argv[])
{
    AVFormatContext *fmt_ctx = NULL;
    AVCodecContext *codec_ctx = NULL;
    AVCodec *codec = NULL;
    AVPacket pkt;
    AVFrame *frame = NULL;
	AVDictionary *options = NULL;
    struct SwsContext *sws_ctx = NULL;
    int video_stream_idx = -1;
    int ret, got_frame, fps;

    if (argc < 2)
    {
        fprintf(stderr, "Usage: %s <input_file>\n", argv[0]);
        exit(1);
    }

    av_register_all();
    if ((ret = avformat_open_input(&fmt_ctx, argv[1], NULL, &options)) < 0)
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

	fps = av_q2d(fmt_ctx->streams[video_stream_idx]->r_frame_rate);///
	
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

    int width , height, cont = 100;

    if (codec_ctx->height < codec_ctx->width)
    {
        width = cont;
        height = width * codec_ctx->height / codec_ctx->width;
    } else {
        height = cont;
        width = height * codec_ctx->width / codec_ctx->height;
    }

    printf("%d %d\n", width, height);
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

    // Initialize ncurses
    initscr();
    start_color();
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
                    int r_val = r * 72;
                    int g_val = g * 72;
                    int b_val = b * 72;
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
			usleep(1000*1000/fps);
            ret = avcodec_send_packet(codec_ctx, &pkt);
            if (ret < 0)
            {
                fprintf(stderr, "Error sending packet for decoding: %s\n", av_err2str(ret));
                exit(1);
            }

            while (ret >= 0)
            {
                ret = avcodec_receive_frame(codec_ctx, frame);
                if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
                {
                    break;
                }
                else if (ret < 0)
                {
                    fprintf(stderr, "Error during decoding: %s\n", av_err2str(ret));
                    exit(1);
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
                            int r_val = round(r * 5.0 / 255.0) * 72;
                            int g_val = round(g * 5.0 / 255.0) * 72;
                            int b_val = round(b * 5.0 / 255.0) * 72;
                            color_num = r_val / 72 * 36 + g_val / 72 * 6 + b_val / 72 + 1;
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
                        for (int k = 0; k < 1; k++)
                        {
                            for (int l = 0; l < 2; l++)
                            {
                                mvaddch(k + y * 1, l + x * 2, ' ');
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
    av_frame_free(&frame);
    av_frame_free(&out_frame);
    avcodec_free_context(&codec_ctx);
    avformat_close_input(&fmt_ctx);
	av_dict_free(&options);
	
    refresh();
    endwin();
}
