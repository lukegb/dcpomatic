/*
    Copyright (C) 2014-2016 Carl Hetherington <cth@carlh.net>

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

#include "dcp_subtitle.h"
#include "content.h"

class DCPSubtitleContent : public DCPSubtitle, public Content
{
public:
	DCPSubtitleContent (boost::filesystem::path);
	DCPSubtitleContent (cxml::ConstNodePtr, int);

	void examine (std::shared_ptr<const Film> film, std::shared_ptr<Job>);
	std::string summary () const;
	std::string technical_summary () const;
	void as_xml (xmlpp::Node *, bool with_paths) const;
	dcpomatic::DCPTime full_length (std::shared_ptr<const Film> film) const;
	dcpomatic::DCPTime approximate_length () const;

private:
	dcpomatic::ContentTime _length;
};
