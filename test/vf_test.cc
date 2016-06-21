/*
    Copyright (C) 2015 Carl Hetherington <cth@carlh.net>

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

#include "lib/film.h"
#include "lib/dcp_content.h"
#include "lib/ffmpeg_content.h"
#include "test.h"
#include <boost/test/unit_test.hpp>
#include <boost/foreach.hpp>
#include <boost/make_shared.hpp>

using std::list;
using std::string;
using boost::shared_ptr;
using boost::make_shared;

/** Test the logic which decides whether a DCP can be referenced or not */
BOOST_AUTO_TEST_CASE (vf_test1)
{
	shared_ptr<Film> film = new_test_film ("vf_test1");
	shared_ptr<DCPContent> dcp = make_shared<DCPContent> (film, "test/data/reels_test2");
	film->examine_and_add_content (dcp);
	wait_for_jobs ();

	/* Multi-reel DCP can't be referenced if we are using a single reel for the project */
	film->set_reel_type (REELTYPE_SINGLE);
	list<string> why_not;
	BOOST_CHECK (!dcp->can_reference_video(why_not));
	BOOST_CHECK (!dcp->can_reference_audio(why_not));
	BOOST_CHECK (!dcp->can_reference_subtitle(why_not));

	/* Multi-reel DCP can be referenced if we are using by-video-content */
	film->set_reel_type (REELTYPE_BY_VIDEO_CONTENT);
	BOOST_CHECK (dcp->can_reference_video(why_not));
	BOOST_CHECK (dcp->can_reference_audio(why_not));
	/* (but reels_test2 has no subtitles to reference) */
	BOOST_CHECK (!dcp->can_reference_subtitle(why_not));

	shared_ptr<FFmpegContent> other = make_shared<FFmpegContent> (film, "test/data/test.mp4");
	film->examine_and_add_content (other);
	wait_for_jobs ();

	/* Not possible if there is overlap */
	other->set_position (DCPTime (0));
	BOOST_CHECK (!dcp->can_reference_video(why_not));
	BOOST_CHECK (!dcp->can_reference_audio(why_not));
	BOOST_CHECK (!dcp->can_reference_subtitle(why_not));

	/* This should not be considered an overlap */
	other->set_position (dcp->end ());
	BOOST_CHECK (dcp->can_reference_video(why_not));
	BOOST_CHECK (dcp->can_reference_audio(why_not));
	/* (reels_test2 has no subtitles to reference) */
	BOOST_CHECK (!dcp->can_reference_subtitle(why_not));
}
