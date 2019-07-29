#include "FFPlayer.h"

#include <thread>

#include "glog/logging.h"

namespace simple_player {
    FFPlayer::FFPlayer() {
        play_status_ = 0;
        render_ = new SDLRender();
        decoder_ = new FFDecoder();
        source_ = new FFSource();
    }

    FFPlayer::~FFPlayer() {
        if(render_ != nullptr) {
            delete render_;
            render_ = nullptr;
        }

        if(decoder_ != nullptr) {
            delete decoder_;
            decoder_ = nullptr;
        }

        if(source_ != nullptr) {
            delete source_;
            source_ = nullptr;
        }
    }

    bool FFPlayer::open(const std::string &url) {
        bool bRet = render_->open();
        if(!bRet) {
            LOG(ERROR) << "render_->init fail!";
            return false;
        }

        bRet = source_->open(url);
        if (!bRet) {
            LOG(ERROR) << "source_->open fail!";
            return false;
        }

        bRet = decoder_->open(source_->getAVCodecID(), source_->getAVCodecParameters());
        if (!bRet) {
            LOG(ERROR) << "decoder_->open fail!";
            return false;
        }

        frame_queue_.init(1000);
        pkt_queue_.init(1000);

        play_status_ = 1;
        std::thread th_render(&FFPlayer::image_render_thread, this);
        th_render.detach();
        std::thread th_decode(&FFPlayer::video_decode_thread, this);
        th_decode.detach();
        std::thread th_receive(&FFPlayer::receive_stream_thread, this);
        th_receive.detach();

        return true;
    }

    bool FFPlayer::close() {
        source_->close();
        decoder_->close();
        render_->close();
        return false;
    }

    void FFPlayer::receive_stream_thread() {
        while(play_status_ == 1) {
            AVPacket* pkt = av_packet_alloc();
            if (pkt == nullptr) {
                LOG(ERROR) << "av_packet_alloc fail!";
                break;
            }

            bool bRet = source_->read_frame(pkt);
            if(!bRet) {
                LOG(ERROR) << "source_->read_frame fail!";
                av_packet_free(&pkt);
                break;
            }

            pkt_queue_.push(pkt);
        }
    }

    void FFPlayer::video_decode_thread() {
        while(play_status_ == 1) {
            AVPacket* pkt = pkt_queue_.pop();
            AVFrame* frame = ::av_frame_alloc();
            if (frame == nullptr) {
                LOG(ERROR) << "av_frame_alloc fail!";
                break;
            }
            decoder_->decode(pkt, frame);
            ::av_packet_unref(pkt);
            frame_queue_.push(frame);
        }
    }

    void FFPlayer::image_render_thread() {
        while(play_status_ == 1) {
            AVFrame* frame = frame_queue_.pop();
            render_->render(frame);
            ::av_frame_free(&frame);
        }
    }
}