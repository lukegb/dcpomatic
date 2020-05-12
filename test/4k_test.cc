/*
    Copyright (C) 2013-2015 Carl Hetherington <cth@carlh.net>

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

/** @defgroup completedcp Complete builds of DCPs with various characteristics, testing broad areas of code */

/** @file  test/4k_test.cc
 *  @brief Run a 4K encode from a simple input.
 *  @ingroup completedcp
 *
 *  The output is checked against test/data/4k_test.
 */

#include <boost/test/unit_test.hpp>
#include "lib/film.h"
#include "lib/ffmpeg_content.h"
#include "lib/dcp_content_type.h"
#include "lib/video_content.h"
#include "lib/ratio.h"
#include "lib/dcpomatic_log.h"
#include "test.h"

using boost::shared_ptr;

BOOST_AUTO_TEST_CASE (fourk_test)
{
	shared_ptr<Film> film = new_test_film ("4k_test");
	dcpomatic_log = film->log ();
	film->set_name ("4k_test");
	shared_ptr<FFmpegContent> c (new FFmpegContent("test/data/test.mp4"));
	film->set_resolution (RESOLUTION_4K);
	film->set_dcp_content_type (DCPContentType::from_isdcf_name ("FTR"));
	film->set_container (Ratio::from_id ("185"));
	film->examine_and_add_content (c);
	BOOST_REQUIRE (!wait_for_jobs());

	film->make_dcp ();
	BOOST_REQUIRE (!wait_for_jobs());

	boost::filesystem::path p (test_film_dir ("4k_test"));
	p /= film->dcp_name ();

	check_dcp ("test/data/4k_test", p.string ());
}
