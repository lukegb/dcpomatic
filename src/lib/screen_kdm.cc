/*
    Copyright (C) 2013-2016 Carl Hetherington <cth@carlh.net>

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

#include "screen_kdm.h"
#include "cinema.h"
#include "screen.h"
#include "util.h"
#include <boost/foreach.hpp>

using std::string;
using std::cout;
using std::list;
using boost::shared_ptr;

int
ScreenKDM::write_files (
	list<shared_ptr<ScreenKDM> > screen_kdms,
	boost::filesystem::path directory,
	dcp::NameFormat name_format,
	dcp::NameFormat::Map name_values,
	boost::function<bool (boost::filesystem::path)> confirm_overwrite
	)
{
	int written = 0;

	if (directory == "-") {
		/* Write KDMs to the stdout */
		BOOST_FOREACH (shared_ptr<ScreenKDM> i, screen_kdms) {
			cout << i->kdm_as_xml ();
			++written;
		}

		return written;
	}

	if (!boost::filesystem::exists (directory)) {
		boost::filesystem::create_directories (directory);
	}

	/* Write KDMs to the specified directory */
	BOOST_FOREACH (shared_ptr<ScreenKDM> i, screen_kdms) {
		name_values['c'] = i->screen->cinema ? i->screen->cinema->name : "";
		name_values['s'] = i->screen->name;
		name_values['i'] = i->kdm_id ();
		boost::filesystem::path out = directory / careful_string_filter(name_format.get(name_values, ".xml"));
		if (!boost::filesystem::exists (out) || confirm_overwrite (out)) {
			i->kdm_as_xml (out);
			++written;
		}
	}

	return written;
}
