/*
    Copyright (C) 2012-2021 Carl Hetherington <cth@carlh.net>

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


#include "wx_util.h"
#include <wx/wx.h>
#include <map>


class Film;
class ScreensPanel;
class RecipientsPanel;
class KDMTimingPanel;
class DKDMOutputPanel;
class KDMCPLPanel;
struct CPLSummary;


class DKDMDialog : public wxDialog
{
public:
	DKDMDialog (wxWindow *, std::shared_ptr<const Film> film);

private:
	void setup_sensitivity ();
	void make_clicked ();
	bool confirm_overwrite (boost::filesystem::path path);

	std::weak_ptr<const Film> _film;
	RecipientsPanel* _recipients;
	KDMTimingPanel* _timing;
	KDMCPLPanel* _cpl;
	DKDMOutputPanel* _output;
	wxButton* _make;
};
