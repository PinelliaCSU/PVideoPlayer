#include "audioextractionservice.h"

#include <QFileInfo>
#include <cstdio>
#include <cstring>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswresample/swresample.h>
}

#define av_log_info(fmt, ...) av_log(NULL, AV_LOG_INFO, fmt, ##__VA_ARGS__)
#define av_log_error(fmt, ...) av_log(NULL, AV_LOG_ERROR, fmt, ##__VA_ARGS__)
bool AudioExtractionService::extract(const QString &inputFile, const QString &outputFile)
{
    AVFormatContext *inFmtCtx = nullptr;
    AVFormatContext *outFmtCtx = nullptr;
    int audioIdx = -1;
    int ret = 0;
    bool result = false;

    // ── 1. 打开输入文件 ──
    ret = avformat_open_input(&inFmtCtx, inputFile.toUtf8().constData(), nullptr, nullptr);
    if (ret < 0) {
        av_log_error("Extract: cannot open input: %s\n", inputFile.toUtf8().constData());
        return false;
    }
    ret = avformat_find_stream_info(inFmtCtx, nullptr);
    if (ret < 0) {
        av_log_error("Extract: cannot find stream info\n");
        avformat_close_input(&inFmtCtx);
        return false;
    }

    // ── 2. 查找音频流 ──
    audioIdx = av_find_best_stream(inFmtCtx, AVMEDIA_TYPE_AUDIO, -1, -1, nullptr, 0);
    if (audioIdx < 0) {
        av_log_error("Extract: no audio stream found\n");
        avformat_close_input(&inFmtCtx);
        return false;
    }
    AVStream *inStream = inFmtCtx->streams[audioIdx];
    AVCodecParameters *inCodecPar = inStream->codecpar;

    av_log_info("Extract: audio codec=%s, sample_rate=%d, channels=%d\n",
                avcodec_get_name(inCodecPar->codec_id),
                inCodecPar->sample_rate, inCodecPar->channels);

    // ── 3. 确定输出格式 ──
    QString ext = QFileInfo(outputFile).suffix().toLower();
    const AVOutputFormat *outFmt = av_guess_format(nullptr,
                                                   outputFile.toUtf8().constData(), nullptr);
    if (!outFmt) {
        av_log_error("Extract: cannot guess output format for %s\n", ext.toUtf8().constData());
        avformat_close_input(&inFmtCtx);
        return false;
    }

    ret = avformat_alloc_output_context2(&outFmtCtx, nullptr, nullptr,
                                         outputFile.toUtf8().constData());
    if (ret < 0 || !outFmtCtx) {
        av_log_error("Extract: cannot create output context\n");
        avformat_close_input(&inFmtCtx);
        return false;
    }

    // ── 4. 判断能否流拷贝（source codec == output format default codec）──
    bool canStreamCopy = false;
    if (outFmt->audio_codec != AV_CODEC_ID_NONE) {
        canStreamCopy = (inCodecPar->codec_id == outFmt->audio_codec);
    }
    // 双重检查：用 avformat_query_codec 确认
    if (!canStreamCopy) {
        canStreamCopy = (avformat_query_codec(outFmt, inCodecPar->codec_id,
                                              FF_COMPLIANCE_NORMAL) == 1);
    }

    // WAV 一律走转码（生成标准 PCM_S16LE）
    bool isWav = (ext == "wav");

    if (isWav || !canStreamCopy) {
        // ═══════════════════════════════════════════
        // 路径 A：转码（解码 → 重采样 → 编码输出）
        // ═══════════════════════════════════════════

        // 4a. 创建输入解码器
        const AVCodec *decoder = avcodec_find_decoder(inCodecPar->codec_id);
        if (!decoder) {
            av_log_error("Extract: unsupported audio codec\n");
            goto cleanup;
        }
        AVCodecContext *decCtx = avcodec_alloc_context3(decoder);
        avcodec_parameters_to_context(decCtx, inCodecPar);
        ret = avcodec_open2(decCtx, decoder, nullptr);
        if (ret < 0) {
            av_log_error("Extract: cannot open decoder\n");
            avcodec_free_context(&decCtx);
            goto cleanup;
        }

        // 4b. 创建输出流
        AVStream *outStream = avformat_new_stream(outFmtCtx, nullptr);
        if (!outStream) {
            avcodec_free_context(&decCtx);
            goto cleanup;
        }
        outStream->time_base = (AVRational){1, decCtx->sample_rate};

        // 4c. 确定输出编码器
        AVCodecContext *encCtx = nullptr;
        const AVCodec *encoder = nullptr;
        bool needEncoder = !isWav;  // WAV 用裸 PCM，不需要编码器

        if (needEncoder) {
            // 非 WAV 格式：需要用编码器
            AVCodecID encId = outFmt->audio_codec;
            if (encId == AV_CODEC_ID_NONE) {
                // 格式没有默认音频编码器，无法转码
                av_log_error("Extract: output format %s has no default audio codec\n", ext.toUtf8().constData());
                avcodec_free_context(&decCtx);
                goto cleanup;
            }
            encoder = avcodec_find_encoder(encId);
            if (!encoder) {
                av_log_error("Extract: encoder for %s not available\n", avcodec_get_name(encId));
                avcodec_free_context(&decCtx);
                goto cleanup;
            }
            encCtx = avcodec_alloc_context3(encoder);
            encCtx->sample_rate = decCtx->sample_rate;
            encCtx->channel_layout = decCtx->channel_layout;
            encCtx->channels = decCtx->channels;
            encCtx->sample_fmt = encoder->sample_fmts ? encoder->sample_fmts[0]
                                                      : AV_SAMPLE_FMT_FLTP;
            encCtx->time_base = (AVRational){1, encCtx->sample_rate};
            // 允许编码器选择最佳比特率
            encCtx->bit_rate = 0;
            if (outFmtCtx->oformat->flags & AVFMT_GLOBALHEADER)
                encCtx->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;

            ret = avcodec_open2(encCtx, encoder, nullptr);
            if (ret < 0) {
                av_log_error("Extract: cannot open encoder\n");
                avcodec_free_context(&decCtx);
                avcodec_free_context(&encCtx);
                goto cleanup;
            }
            avcodec_parameters_from_context(outStream->codecpar, encCtx);
        } else {
            // WAV: 直接输出 PCM S16LE
            outStream->codecpar->codec_type = AVMEDIA_TYPE_AUDIO;
            outStream->codecpar->codec_id = AV_CODEC_ID_PCM_S16LE;
            outStream->codecpar->sample_rate = decCtx->sample_rate;
            outStream->codecpar->channel_layout = decCtx->channel_layout;
            outStream->codecpar->channels = decCtx->channels;
            outStream->codecpar->format = AV_SAMPLE_FMT_S16;
            outStream->codecpar->block_align = 2 * decCtx->channels;
            outStream->codecpar->bits_per_coded_sample = 16;
        }

        // 4d. 创建重采样器：输入格式 → 输出格式
        SwrContext *swrCtx = nullptr;
        {
            AVSampleFormat outSampleFmt = isWav ? AV_SAMPLE_FMT_S16
                                                : encCtx->sample_fmt;
            int outSampleRate = decCtx->sample_rate;
            int64_t outChLayout = decCtx->channel_layout;
            int outChannels = decCtx->channels;

            swrCtx = swr_alloc_set_opts(nullptr,
                                        outChLayout, outSampleFmt, outSampleRate,
                                        decCtx->channel_layout, decCtx->sample_fmt, decCtx->sample_rate,
                                        0, nullptr);
            if (!swrCtx || swr_init(swrCtx) < 0) {
                av_log_error("Extract: cannot create resampler\n");
                swr_free(&swrCtx);
                avcodec_free_context(&decCtx);
                if (encCtx) avcodec_free_context(&encCtx);
                goto cleanup;
            }
        }

        // 4e. 打开输出文件 + 写文件头
        if (!(outFmtCtx->oformat->flags & AVFMT_NOFILE)) {
            ret = avio_open(&outFmtCtx->pb, outputFile.toUtf8().constData(),
                            AVIO_FLAG_WRITE);
            if (ret < 0) {
                av_log_error("Extract: cannot open output file\n");
                swr_free(&swrCtx);
                avcodec_free_context(&decCtx);
                if (encCtx) avcodec_free_context(&encCtx);
                goto cleanup;
            }
        }
        ret = avformat_write_header(outFmtCtx, nullptr);
        if (ret < 0) {
            av_log_error("Extract: cannot write header\n");
            swr_free(&swrCtx);
            avcodec_free_context(&decCtx);
            if (encCtx) avcodec_free_context(&encCtx);
            goto cleanup;
        }

        // 4f. 主循环：读包 → 解码 → 重采样 → 编码 → 写输出
        AVPacket *inPkt = av_packet_alloc();
        AVFrame *decFrame = av_frame_alloc();
        int64_t outPts = 0;

        while (av_read_frame(inFmtCtx, inPkt) >= 0) {
            if (inPkt->stream_index != audioIdx) {
                av_packet_unref(inPkt);
                continue;
            }

            ret = avcodec_send_packet(decCtx, inPkt);
            if (ret < 0) {
                av_packet_unref(inPkt);
                continue;
            }

            while (ret >= 0) {
                ret = avcodec_receive_frame(decCtx, decFrame);
                if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) break;
                if (ret < 0) break;

                // 计算重采样输出样本数
                int dstNbSamples = av_rescale_rnd(
                    swr_get_delay(swrCtx, decCtx->sample_rate) + decFrame->nb_samples,
                    decCtx->sample_rate, decCtx->sample_rate, AV_ROUND_UP);

                // 分配输出缓冲区
                AVFrame *outFrame = av_frame_alloc();
                outFrame->nb_samples = dstNbSamples;
                outFrame->channel_layout = isWav ? decCtx->channel_layout
                                                 : encCtx->channel_layout;
                outFrame->sample_rate = decCtx->sample_rate;
                outFrame->format = isWav ? AV_SAMPLE_FMT_S16 : encCtx->sample_fmt;
                av_frame_get_buffer(outFrame, 0);

                // 重采样
                int actualSamples = swr_convert(
                    swrCtx,
                    outFrame->data, dstNbSamples,
                    (const uint8_t **)decFrame->data, decFrame->nb_samples);
                if (actualSamples < 0) {
                    av_frame_free(&outFrame);
                    continue;
                }
                outFrame->nb_samples = actualSamples;

                if (isWav) {
                    // ── WAV：直接写 PCM 包 ──
                    AVPacket *outPkt = av_packet_alloc();
                    outPkt->data = nullptr;   // 让 av_packet_from_data 不接管内存
                    outPkt->size = 0;

                    int bufSize = av_samples_get_buffer_size(
                        nullptr, outFrame->channels,
                        outFrame->nb_samples, AV_SAMPLE_FMT_S16, 1);

                    // 复制数据到包（避免 double-free）
                    uint8_t *pktData = (uint8_t *)av_malloc(bufSize);
                    memcpy(pktData, outFrame->data[0], bufSize);

                    av_packet_from_data(outPkt, pktData, bufSize);
                    outPkt->stream_index = 0;
                    outPkt->pts = outPts;
                    outPkt->dts = outPts;
                    outPts += outFrame->nb_samples;

                    av_interleaved_write_frame(outFmtCtx, outPkt);
                    av_packet_free(&outPkt);  // 内部会 av_free pktData
                } else {
                    // ── 编码路径 ──
                    outFrame->pts = outPts;
                    outPts += outFrame->nb_samples;

                    ret = avcodec_send_frame(encCtx, outFrame);
                    if (ret < 0) {
                        av_frame_free(&outFrame);
                        continue;
                    }
                    while (ret >= 0) {
                        AVPacket *outPkt = av_packet_alloc();
                        ret = avcodec_receive_packet(encCtx, outPkt);
                        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
                            av_packet_free(&outPkt);
                            break;
                        }
                        if (ret < 0) {
                            av_packet_free(&outPkt);
                            break;
                        }
                        outPkt->stream_index = 0;
                        av_packet_rescale_ts(outPkt, encCtx->time_base,
                                             outStream->time_base);
                        av_interleaved_write_frame(outFmtCtx, outPkt);
                        av_packet_free(&outPkt);
                    }
                }
                av_frame_free(&outFrame);
            }
            av_packet_unref(inPkt);
        }

        // 4g. 冲刷解码器 → 编码器
        avcodec_send_packet(decCtx, nullptr);
        while (true) {
            ret = avcodec_receive_frame(decCtx, decFrame);
            if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) break;
            if (ret < 0) break;
            // 重采样并编码最后的帧
            int dstNbSamples = av_rescale_rnd(
                swr_get_delay(swrCtx, decCtx->sample_rate) + decFrame->nb_samples,
                decCtx->sample_rate, decCtx->sample_rate, AV_ROUND_UP);
            AVFrame *outFrame = av_frame_alloc();
            outFrame->nb_samples = dstNbSamples;
            outFrame->channel_layout = isWav ? decCtx->channel_layout
                                             : encCtx->channel_layout;
            outFrame->sample_rate = decCtx->sample_rate;
            outFrame->format = isWav ? AV_SAMPLE_FMT_S16 : encCtx->sample_fmt;
            av_frame_get_buffer(outFrame, 0);
            int actualSamples = swr_convert(swrCtx, outFrame->data, dstNbSamples,
                                            (const uint8_t **)decFrame->data,
                                            decFrame->nb_samples);
            if (actualSamples > 0) {
                outFrame->nb_samples = actualSamples;
                if (isWav) {
                    int bufSize = av_samples_get_buffer_size(
                        nullptr, outFrame->channels,
                        outFrame->nb_samples, AV_SAMPLE_FMT_S16, 1);
                    AVPacket *outPkt = av_packet_alloc();
                    uint8_t *pktData = (uint8_t *)av_malloc(bufSize);
                    memcpy(pktData, outFrame->data[0], bufSize);
                    av_packet_from_data(outPkt, pktData, bufSize);
                    outPkt->stream_index = 0;
                    outPkt->pts = outPts;
                    outPkt->dts = outPts;
                    outPts += outFrame->nb_samples;
                    av_interleaved_write_frame(outFmtCtx, outPkt);
                    av_packet_free(&outPkt);
                } else {
                    outFrame->pts = outPts;
                    outPts += outFrame->nb_samples;
                    avcodec_send_frame(encCtx, outFrame);
                }
            }
            av_frame_free(&outFrame);
        }

        // 冲刷编码器（如果不是 WAV）
        if (!isWav && encCtx) {
            avcodec_send_frame(encCtx, nullptr);
            while (true) {
                AVPacket *outPkt = av_packet_alloc();
                ret = avcodec_receive_packet(encCtx, outPkt);
                if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
                    av_packet_free(&outPkt);
                    break;
                }
                if (ret < 0) {
                    av_packet_free(&outPkt);
                    break;
                }
                outPkt->stream_index = 0;
                av_packet_rescale_ts(outPkt, encCtx->time_base,
                                     outStream->time_base);
                av_interleaved_write_frame(outFmtCtx, outPkt);
                av_packet_free(&outPkt);
            }
        }

        // 写文件尾
        av_write_trailer(outFmtCtx);

        // 清理
        av_packet_free(&inPkt);
        av_frame_free(&decFrame);
        swr_free(&swrCtx);
        avcodec_free_context(&decCtx);
        if (encCtx) avcodec_free_context(&encCtx);

        result = true;
        av_log_info("Extract (transcode): %s -> %s done\n",
                    inputFile.toUtf8().constData(),
                    outputFile.toUtf8().constData());

    } else {
        // ═══════════════════════════════════════════
        // 路径 B：流拷贝（快速，无损，源格式 = 目标格式）
        // ═══════════════════════════════════════════

        AVStream *outStream = avformat_new_stream(outFmtCtx, nullptr);
        if (!outStream) goto cleanup;

        ret = avcodec_parameters_copy(outStream->codecpar, inCodecPar);
        if (ret < 0) {
            av_log_error("Extract: cannot copy codec params\n");
            goto cleanup;
        }
        outStream->codecpar->codec_tag = 0;
        outStream->time_base = inStream->time_base;

        // 打开输出文件
        if (!(outFmtCtx->oformat->flags & AVFMT_NOFILE)) {
            ret = avio_open(&outFmtCtx->pb, outputFile.toUtf8().constData(),
                            AVIO_FLAG_WRITE);
            if (ret < 0) {
                av_log_error("Extract: cannot open output file\n");
                goto cleanup;
            }
        }

        // 写文件头
        ret = avformat_write_header(outFmtCtx, nullptr);
        if (ret < 0) {
            av_log_error("Extract: cannot write header\n");
            goto cleanup;
        }

        // 复制音频包
        AVPacket pkt;
        while (av_read_frame(inFmtCtx, &pkt) >= 0) {
            if (pkt.stream_index == audioIdx) {
                av_packet_rescale_ts(&pkt, inStream->time_base,
                                     outStream->time_base);
                pkt.stream_index = outStream->index;
                av_interleaved_write_frame(outFmtCtx, &pkt);
            }
            av_packet_unref(&pkt);
        }

        // 写文件尾
        av_write_trailer(outFmtCtx);

        result = true;
        av_log_info("Extract (stream copy): %s -> %s done\n",
                    inputFile.toUtf8().constData(),
                    outputFile.toUtf8().constData());
    }

cleanup:
    // ── 6. 统一清理 ──
    if (outFmtCtx) {
        if (!(outFmtCtx->oformat->flags & AVFMT_NOFILE) && outFmtCtx->pb)
            avio_closep(&outFmtCtx->pb);
        avformat_free_context(outFmtCtx);
    }
    avformat_close_input(&inFmtCtx);

    return result;
}
