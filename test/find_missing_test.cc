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


#include "lib/content.h"
#include "lib/content_factory.h"
#include "lib/dcp_content.h"
#include "lib/film.h"
#include "lib/find_missing.h"
#include "test.h"
#include <boost/filesystem.hpp>
#include <boost/test/unit_test.hpp>


using std::make_shared;
using std::string;


BOOST_AUTO_TEST_CASE (find_missing_test_with_single_files)
{
	using namespace boost::filesystem;

	auto name = string{"find_missing_test_with_single_files"};

	/* Make a directory with some content */
	auto content_dir = path("build/test") / path(name + "_content");
	remove_all (content_dir);
	create_directories (content_dir);
	copy_file ("test/data/flat_red.png", content_dir / "A.png");
	copy_file ("test/data/flat_red.png", content_dir / "B.png");
	copy_file ("test/data/flat_red.png", content_dir / "C.png");

	/* Make a film with that content */
	auto film = new_test_film2 (name + "_film", {
		content_factory(content_dir / "A.png").front(),
		content_factory(content_dir / "B.png").front(),
		content_factory(content_dir / "C.png").front()
		});
	film->write_metadata ();

	/* Move the content somewhere eles */
	auto moved = path("build/test") / path(name + "_moved");
	remove_all (moved);
	rename (content_dir, moved);

	/* That should make the content paths invalid */
	for (auto content: film->content()) {
		BOOST_CHECK (!content->paths_valid());
	}

	/* Fix the missing files and check the result */
	dcpomatic::find_missing (film->content(), moved / "A.png");

	for (auto content: film->content()) {
		BOOST_CHECK (content->paths_valid());
	}
}


BOOST_AUTO_TEST_CASE (find_missing_test_with_multiple_files)
{
	using namespace boost::filesystem;

	auto name = string{"find_missing_test_with_multiple_files"};

	/* Copy an arbitrary DCP into a test directory */
	auto content_dir = path("build/test") / path(name + "_content");
	remove_all (content_dir);
	create_directories (content_dir);
	for (auto ref: directory_iterator("test/data/scaling_test_133_185")) {
		copy (ref, content_dir / ref.path().filename());
	}

	/* Make a film containing that DCP */
	auto film = new_test_film2 (name + "_film", { make_shared<DCPContent>(content_dir) });
	film->write_metadata ();

	/* Move the DCP's content elsewhere */
	auto moved = path("build/test") / path(name + "_moved");
	remove_all (moved);
	rename (content_dir, moved);

	/* That should make the content paths invalid */
	for (auto content: film->content()) {
		BOOST_CHECK (!content->paths_valid());
	}

	/* Fix the missing files and check the result */
	dcpomatic::find_missing (film->content(), moved / "foo");

	for (auto content: film->content()) {
		BOOST_CHECK (content->paths_valid());
	}
}


BOOST_AUTO_TEST_CASE (find_missing_test_with_multiple_files_one_incorrect)
{
	using namespace boost::filesystem;

	auto name = string{"find_missing_test_with_multiple_files_one_incorrect"};

	/* Copy an arbitrary DCP into a test directory */
	auto content_dir = path("build/test") / path(name + "_content");
	remove_all (content_dir);
	create_directories (content_dir);
	for (auto ref: directory_iterator("test/data/scaling_test_133_185")) {
		copy (ref, content_dir / ref.path().filename());
	}

	/* Make a film containing that DCP */
	auto film = new_test_film2 (name + "_film", { make_shared<DCPContent>(content_dir) });
	film->write_metadata ();

	/* Move the DCP's content elsewhere */
	auto moved = path("build/test") / path(name + "_moved");
	remove_all (moved);
	rename (content_dir, moved);

	/* Corrupt one of the files in the moved content, so that it should not be found in the find_missing
	 * step
	 */
	remove (moved / "cpl_80daeb7a-57d8-4a70-abeb-cd92ddac1527.xml");
	copy ("test/data/scaling_test_133_185/ASSETMAP.xml", moved / "cpl_80daeb7a-57d8-4a70-abeb-cd92ddac1527.xml");

	/* The film's contents should be invalid */
	for (auto content: film->content()) {
		BOOST_CHECK (!content->paths_valid());
	}

	dcpomatic::find_missing (film->content(), moved / "foo");

	/* And even after find_missing there should still be missing content */
	for (auto content: film->content()) {
		BOOST_CHECK (!content->paths_valid());
	}
}

