/*
    Copyright (C) 2021 Carl Hetherington <cth@carlh.net>

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


#include "ffmpeg_wrapper.h"
#include "warnings.h"
DCPOMATIC_DISABLE_WARNINGS
extern "C" {
#include <libavformat/avformat.h>
}
DCPOMATIC_ENABLE_WARNINGS
#include <new>


using namespace ffmpeg;


Packet::Packet ()
{
	_packet = av_packet_alloc ();
	if (!_packet) {
		throw std::bad_alloc ();
	}
}


Packet::~Packet ()
{
	av_packet_free (&_packet);
}

