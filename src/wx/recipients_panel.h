/*
    Copyright (C) 2015-2021 Carl Hetherington <cth@carlh.net>

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


#include "lib/dkdm_recipient.h"
#include "lib/warnings.h"
DCPOMATIC_DISABLE_WARNINGS
#include <wx/wx.h>
#include <wx/srchctrl.h>
#include <wx/treectrl.h>
DCPOMATIC_ENABLE_WARNINGS
#include <boost/signals2.hpp>
#include <list>
#include <map>


class DKDMRecipient;


class RecipientsPanel : public wxPanel
{
public:
	explicit RecipientsPanel (wxWindow* parent);
	~RecipientsPanel ();

	void setup_sensitivity ();

	std::list<std::shared_ptr<DKDMRecipient>> recipients () const;
	boost::signals2::signal<void ()> RecipientsChanged;

private:
	void add_recipients ();
	void add_recipient (std::shared_ptr<DKDMRecipient>);
	void add_recipient_clicked ();
	void edit_recipient_clicked ();
	void remove_recipient_clicked ();
	void selection_changed_shim (wxTreeEvent &);
	void selection_changed ();
	void search_changed ();

	wxSearchCtrl* _search;
	wxTreeCtrl* _targets;
	wxButton* _add_recipient;
	wxButton* _edit_recipient;
	wxButton* _remove_recipient;
	wxTreeItemId _root;

	typedef std::map<wxTreeItemId, std::shared_ptr<DKDMRecipient>> RecipientMap;
	RecipientMap _recipients;
	RecipientMap _selected;

	bool _ignore_selection_change;
};
