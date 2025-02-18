/*
    Copyright (C) 2013-2021 Carl Hetherington <cth@carlh.net>

    This file is part of DCP-o-matic.

    DCP-o-matic is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    DCP-o-matic is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with DCP-o-matic.  If not, see <http://www.gnu.org/licenses/>.

*/


#include "dcpomatic_log.h"
#include "ffmpeg_examiner.h"
#include "ffmpeg_content.h"
#include "job.h"
#include "ffmpeg_audio_stream.h"
#include "ffmpeg_subtitle_stream.h"
#include "util.h"
#include "warnings.h"
DCPOMATIC_DISABLE_WARNINGS
extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/pixfmt.h>
#include <libavutil/pixdesc.h>
#include <libavutil/channel_layout.h>
#include <libavutil/display.h>
#include <libavutil/eval.h>
}
DCPOMATIC_ENABLE_WARNINGS
#include <iostream>

#include "i18n.h"


using std::cout;
using std::make_shared;
using std::max;
using std::shared_ptr;
using std::string;
using std::vector;
using boost::optional;
using namespace dcpomatic;


/* This is how many frames from the start of any video that we will examine to see if we
 * can spot soft 2:3 pull-down ("telecine").
 */
static const int PULLDOWN_CHECK_FRAMES = 16;


/** @param job job that the examiner is operating in, or 0 */
FFmpegExaminer::FFmpegExaminer (shared_ptr<const FFmpegContent> c, shared_ptr<Job> job)
	: FFmpeg (c)
	, _video_length (0)
	, _need_video_length (false)
	, _pulldown (false)
{
	/* Find audio and subtitle streams */

	for (uint32_t i = 0; i < _format_context->nb_streams; ++i) {
		auto s = _format_context->streams[i];
		auto codec = _codec_context[i] ? _codec_context[i]->codec : nullptr;
		if (s->codecpar->codec_type == AVMEDIA_TYPE_AUDIO && codec) {

			/* This is a hack; sometimes it seems that _audio_codec_context->channel_layout isn't set up,
			   so bodge it here.  No idea why we should have to do this.
			*/

			if (s->codecpar->channel_layout == 0) {
				s->codecpar->channel_layout = av_get_default_channel_layout (s->codecpar->channels);
			}

			DCPOMATIC_ASSERT (_format_context->duration != AV_NOPTS_VALUE);
			DCPOMATIC_ASSERT (codec->name);

			_audio_streams.push_back (
				make_shared<FFmpegAudioStream>(
					stream_name (s),
					codec->name,
					s->id,
					s->codecpar->sample_rate,
					llrint ((double(_format_context->duration) / AV_TIME_BASE) * s->codecpar->sample_rate),
					s->codecpar->channels
					)
				);

		} else if (s->codecpar->codec_type == AVMEDIA_TYPE_SUBTITLE) {
			_subtitle_streams.push_back (make_shared<FFmpegSubtitleStream>(subtitle_stream_name (s), s->id));
		}
	}

	if (has_video ()) {
		/* See if the header has duration information in it */
		_need_video_length = _format_context->duration == AV_NOPTS_VALUE;
		if (!_need_video_length) {
			_video_length = llrint ((double (_format_context->duration) / AV_TIME_BASE) * video_frame_rate().get());
		}
	}

	if (job && _need_video_length) {
		job->sub (_("Finding length"));
	}

	/* Run through until we find:
	 *   - the first video.
	 *   - the first audio for each stream.
	 *   - the top-field-first and repeat-first-frame values ("temporal_reference") for the first PULLDOWN_CHECK_FRAMES video frames.
	 */

	int64_t const len = _file_group.length ();
	/* A string which we build up to describe the top-field-first and repeat-first-frame values for the first few frames.
	 * It would be nicer to use something like vector<bool> here but we want to search the array for a pattern later,
	 * and a string seems a reasonably neat way to do that.
	 */
	string temporal_reference;
	while (true) {
		auto packet = av_packet_alloc ();
		DCPOMATIC_ASSERT (packet);
		int r = av_read_frame (_format_context, packet);
		if (r < 0) {
			av_packet_free (&packet);
			break;
		}

		if (job) {
			if (len > 0) {
				job->set_progress (float (_format_context->pb->pos) / len);
			} else {
				job->set_progress_unknown ();
			}
		}

		auto context = _codec_context[packet->stream_index];

		if (_video_stream && packet->stream_index == _video_stream.get()) {
			video_packet (context, temporal_reference, packet);
		}

		bool got_all_audio = true;

		for (size_t i = 0; i < _audio_streams.size(); ++i) {
			if (_audio_streams[i]->uses_index(_format_context, packet->stream_index)) {
				audio_packet (context, _audio_streams[i], packet);
			}
			if (!_audio_streams[i]->first_audio) {
				got_all_audio = false;
			}
		}

		av_packet_free (&packet);

		if (_first_video && got_all_audio && temporal_reference.size() >= (PULLDOWN_CHECK_FRAMES * 2)) {
			/* All done */
			break;
		}
	}

	if (_video_stream) {
		auto context = _codec_context[_video_stream.get()];
		while (video_packet(context, temporal_reference, nullptr)) {}
	}

	for (auto i: _audio_streams) {
		auto context = _codec_context[i->index(_format_context)];
		audio_packet(context, i, nullptr);
	}

	if (_video_stream) {
		/* This code taken from get_rotation() in ffmpeg:cmdutils.c */
		auto stream = _format_context->streams[*_video_stream];
		auto rotate_tag = av_dict_get (stream->metadata, "rotate", 0, 0);
		uint8_t* displaymatrix = av_stream_get_side_data (stream, AV_PKT_DATA_DISPLAYMATRIX, 0);
		_rotation = 0;

		if (rotate_tag && *rotate_tag->value && strcmp(rotate_tag->value, "0")) {
			char *tail;
			_rotation = av_strtod (rotate_tag->value, &tail);
			if (*tail) {
				_rotation = 0;
			}
		}

		if (displaymatrix && !_rotation) {
			_rotation = - av_display_rotation_get ((int32_t*) displaymatrix);
		}

		_rotation = *_rotation - 360 * floor (*_rotation / 360 + 0.9 / 360);
	}

	LOG_GENERAL("Temporal reference was %1", temporal_reference);
	if (temporal_reference.find("T2T3B2B3T2T3B2B3") != string::npos || temporal_reference.find("B2B3T2T3B2B3T2T3") != string::npos) {
		/* The magical sequence (taken from mediainfo) suggests that 2:3 pull-down is in use */
		_pulldown = true;
		LOG_GENERAL_NC("Suggest that this may be 2:3 pull-down (soft telecine)");
	}
}


/** @param temporal_reference A string to which we should add two characters per frame;
 *  the first   is T or B depending on whether it's top- or bottom-field first,
 *  the second  is 3 or 2 depending on whether "repeat_pict" is true or not.
 *  @return true if some video was decoded, otherwise false.
 */
bool
FFmpegExaminer::video_packet (AVCodecContext* context, string& temporal_reference, AVPacket* packet)
{
	DCPOMATIC_ASSERT (_video_stream);

	if (_first_video && !_need_video_length && temporal_reference.size() >= (PULLDOWN_CHECK_FRAMES * 2)) {
		return false;
	}

	int r = avcodec_send_packet (context, packet);
	if (r < 0 && !(r == AVERROR_EOF && !packet)) {
		/* We could cope with AVERROR(EAGAIN) and re-send the packet but I think it should never happen.
		 * AVERROR_EOF can happen during flush if we've already sent a flush packet.
		 */
		throw DecodeError (N_("avcodec_send_packet"), N_("FFmpegExaminer::video_packet"), r);
	}

	r = avcodec_receive_frame (context, _video_frame);
	if (r == AVERROR(EAGAIN)) {
		/* More input is required */
		return true;
	} else if (r == AVERROR_EOF) {
		/* No more output is coming */
		return false;
	}

	if (!_first_video) {
		_first_video = frame_time (_video_frame, _format_context->streams[_video_stream.get()]);
	}
	if (_need_video_length) {
		_video_length = frame_time (
			_video_frame,
			_format_context->streams[_video_stream.get()]
			).get_value_or (ContentTime ()).frames_round (video_frame_rate().get ());
	}
	if (temporal_reference.size() < (PULLDOWN_CHECK_FRAMES * 2)) {
		temporal_reference += (_video_frame->top_field_first ? "T" : "B");
		temporal_reference += (_video_frame->repeat_pict ? "3" : "2");
	}

	return true;
}


void
FFmpegExaminer::audio_packet (AVCodecContext* context, shared_ptr<FFmpegAudioStream> stream, AVPacket* packet)
{
	if (stream->first_audio) {
		return;
	}

	int r = avcodec_send_packet (context, packet);
	if (r < 0 && !(r == AVERROR_EOF && !packet) && r != AVERROR(EAGAIN)) {
		/* We could cope with AVERROR(EAGAIN) and re-send the packet but I think it should never happen.
		 * AVERROR_EOF can happen during flush if we've already sent a flush packet.
		 * EAGAIN means we need to do avcodec_receive_frame, so just carry on and do that.
		 */
		throw DecodeError (N_("avcodec_send_packet"), N_("FFmpegExaminer::audio_packet"), r);
	}

	auto frame = audio_frame (stream);

	if (avcodec_receive_frame (context, frame) < 0) {
		return;
	}

	stream->first_audio = frame_time (frame, stream->stream(_format_context));
}


optional<ContentTime>
FFmpegExaminer::frame_time (AVFrame* frame, AVStream* stream) const
{
	optional<ContentTime> t;

	int64_t const bet = frame->best_effort_timestamp;
	if (bet != AV_NOPTS_VALUE) {
		t = ContentTime::from_seconds (bet * av_q2d(stream->time_base));
	}

	return t;
}


optional<double>
FFmpegExaminer::video_frame_rate () const
{
	DCPOMATIC_ASSERT (_video_stream);
	return av_q2d(av_guess_frame_rate(_format_context, _format_context->streams[_video_stream.get()], 0));
}


dcp::Size
FFmpegExaminer::video_size () const
{
	return dcp::Size (video_codec_context()->width, video_codec_context()->height);
}


/** @return Length according to our content's header */
Frame
FFmpegExaminer::video_length () const
{
	return max (Frame (1), _video_length);
}


optional<double>
FFmpegExaminer::sample_aspect_ratio () const
{
	DCPOMATIC_ASSERT (_video_stream);
	auto sar = av_guess_sample_aspect_ratio (_format_context, _format_context->streams[_video_stream.get()], 0);
	if (sar.num == 0) {
		/* I assume this means that we don't know */
		return {};
	}
	return double (sar.num) / sar.den;
}


string
FFmpegExaminer::subtitle_stream_name (AVStream* s) const
{
	auto n = stream_name (s);

	if (n.empty()) {
		n = _("unknown");
	}

	return n;
}


string
FFmpegExaminer::stream_name (AVStream* s) const
{
	string n;

	if (s->metadata) {
		auto const lang = av_dict_get (s->metadata, "language", 0, 0);
		if (lang) {
			n = lang->value;
		}

		auto const title = av_dict_get (s->metadata, "title", 0, 0);
		if (title) {
			if (!n.empty()) {
				n += " ";
			}
			n += title->value;
		}
	}

	return n;
}


optional<int>
FFmpegExaminer::bits_per_pixel () const
{
	if (video_codec_context()->pix_fmt == -1) {
		return {};
	}

	auto const d = av_pix_fmt_desc_get (video_codec_context()->pix_fmt);
	DCPOMATIC_ASSERT (d);
	return av_get_bits_per_pixel (d);
}


bool
FFmpegExaminer::yuv () const
{
	switch (video_codec_context()->pix_fmt) {
	case AV_PIX_FMT_YUV420P:
	case AV_PIX_FMT_YUYV422:
	case AV_PIX_FMT_YUV422P:
	case AV_PIX_FMT_YUV444P:
	case AV_PIX_FMT_YUV410P:
	case AV_PIX_FMT_YUV411P:
	case AV_PIX_FMT_YUVJ420P:
	case AV_PIX_FMT_YUVJ422P:
	case AV_PIX_FMT_YUVJ444P:
	case AV_PIX_FMT_UYVY422:
	case AV_PIX_FMT_UYYVYY411:
	case AV_PIX_FMT_NV12:
	case AV_PIX_FMT_NV21:
	case AV_PIX_FMT_YUV440P:
	case AV_PIX_FMT_YUVJ440P:
	case AV_PIX_FMT_YUVA420P:
	case AV_PIX_FMT_YUV420P16LE:
	case AV_PIX_FMT_YUV420P16BE:
	case AV_PIX_FMT_YUV422P16LE:
	case AV_PIX_FMT_YUV422P16BE:
	case AV_PIX_FMT_YUV444P16LE:
	case AV_PIX_FMT_YUV444P16BE:
	case AV_PIX_FMT_YUV420P9BE:
	case AV_PIX_FMT_YUV420P9LE:
	case AV_PIX_FMT_YUV420P10BE:
	case AV_PIX_FMT_YUV420P10LE:
	case AV_PIX_FMT_YUV422P10BE:
	case AV_PIX_FMT_YUV422P10LE:
	case AV_PIX_FMT_YUV444P9BE:
	case AV_PIX_FMT_YUV444P9LE:
	case AV_PIX_FMT_YUV444P10BE:
	case AV_PIX_FMT_YUV444P10LE:
	case AV_PIX_FMT_YUV422P9BE:
	case AV_PIX_FMT_YUV422P9LE:
	case AV_PIX_FMT_YUVA420P9BE:
	case AV_PIX_FMT_YUVA420P9LE:
	case AV_PIX_FMT_YUVA422P9BE:
	case AV_PIX_FMT_YUVA422P9LE:
	case AV_PIX_FMT_YUVA444P9BE:
	case AV_PIX_FMT_YUVA444P9LE:
	case AV_PIX_FMT_YUVA420P10BE:
	case AV_PIX_FMT_YUVA420P10LE:
	case AV_PIX_FMT_YUVA422P10BE:
	case AV_PIX_FMT_YUVA422P10LE:
	case AV_PIX_FMT_YUVA444P10BE:
	case AV_PIX_FMT_YUVA444P10LE:
	case AV_PIX_FMT_YUVA420P16BE:
	case AV_PIX_FMT_YUVA420P16LE:
	case AV_PIX_FMT_YUVA422P16BE:
	case AV_PIX_FMT_YUVA422P16LE:
	case AV_PIX_FMT_YUVA444P16BE:
	case AV_PIX_FMT_YUVA444P16LE:
	case AV_PIX_FMT_NV16:
	case AV_PIX_FMT_NV20LE:
	case AV_PIX_FMT_NV20BE:
	case AV_PIX_FMT_YVYU422:
	case AV_PIX_FMT_YUVA444P:
	case AV_PIX_FMT_YUVA422P:
	case AV_PIX_FMT_YUV420P12BE:
	case AV_PIX_FMT_YUV420P12LE:
	case AV_PIX_FMT_YUV420P14BE:
	case AV_PIX_FMT_YUV420P14LE:
	case AV_PIX_FMT_YUV422P12BE:
	case AV_PIX_FMT_YUV422P12LE:
	case AV_PIX_FMT_YUV422P14BE:
	case AV_PIX_FMT_YUV422P14LE:
	case AV_PIX_FMT_YUV444P12BE:
	case AV_PIX_FMT_YUV444P12LE:
	case AV_PIX_FMT_YUV444P14BE:
	case AV_PIX_FMT_YUV444P14LE:
	case AV_PIX_FMT_YUVJ411P:
		return true;
	default:
		return false;
	}
}


bool
FFmpegExaminer::has_video () const
{
	return static_cast<bool>(_video_stream);
}


VideoRange
FFmpegExaminer::range () const
{
	switch (color_range()) {
	case AVCOL_RANGE_MPEG:
	case AVCOL_RANGE_UNSPECIFIED:
		return VideoRange::VIDEO;
	case AVCOL_RANGE_JPEG:
	default:
		return VideoRange::FULL;
	}
}


PixelQuanta
FFmpegExaminer::pixel_quanta () const
{
	auto const desc = av_pix_fmt_desc_get(video_codec_context()->pix_fmt);
	return { 1 << desc->log2_chroma_w, 1 << desc->log2_chroma_h };
}

