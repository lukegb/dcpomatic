/*
    Copyright (C) 2020-2021 Carl Hetherington <cth@carlh.net>

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


#ifndef DCPOMATIC_FULL_LANGUAGE_TAG_DIALOG_H
#define DCPOMATIC_FULL_LANGUAGE_TAG_DIALOG_H


#include "lib/warnings.h"
#include <dcp/language_tag.h>
DCPOMATIC_DISABLE_WARNINGS
#include <wx/wx.h>
DCPOMATIC_ENABLE_WARNINGS


class wxListCtrl;
class LanguageSubtagPanel;


class FullLanguageTagDialog : public wxDialog
{
public:
	class Subtag
	{
	public:
		Subtag (dcp::LanguageTag::SubtagType type_, boost::optional<dcp::LanguageTag::SubtagData> subtag_)
			: type (type_)
			, subtag (subtag_)
		{}

		dcp::LanguageTag::SubtagType type;
		boost::optional<dcp::LanguageTag::SubtagData> subtag;
		std::string last_search;
	};

	FullLanguageTagDialog (wxWindow* parent, dcp::LanguageTag tag = dcp::LanguageTag("en"));

	dcp::LanguageTag get () const;
	void set (dcp::LanguageTag tag);

private:

	std::string subtag_type_name (dcp::LanguageTag::SubtagType type);
	void search_changed (std::string search);
	void add_to_current_tag (dcp::LanguageTag::SubtagType type, boost::optional<dcp::LanguageTag::SubtagData> subtag);
	void remove_from_current_tag ();
	void current_tag_selection_changed ();
	void chosen_subtag_changed (boost::optional<dcp::LanguageTag::SubtagData> selection);
	void setup_sensitivity ();

	std::vector<Subtag> _current_tag_subtags;
	wxListCtrl* _current_tag_list;
	LanguageSubtagPanel* _choose_subtag_panel;
	wxButton* _add_script;
	wxButton* _add_region;
	wxButton* _add_variant;
	wxButton* _add_external;
	wxButton* _remove;
};



class RegionSubtagDialog : public wxDialog
{
public:
	RegionSubtagDialog (wxWindow* parent, dcp::LanguageTag::RegionSubtag region);

	boost::optional<dcp::LanguageTag::RegionSubtag> get () const;

private:
	LanguageSubtagPanel* _panel;
};


#endif
