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


#include "screens_panel.h"
#include "wx_util.h"
#include "cinema_dialog.h"
#include "screen_dialog.h"
#include "dcpomatic_button.h"
#include "lib/config.h"
#include "lib/cinema.h"
#include "lib/screen.h"


using std::list;
using std::pair;
using std::cout;
using std::map;
using std::string;
using std::make_pair;
using std::make_shared;
using std::shared_ptr;
using boost::optional;
using namespace dcpomatic;


ScreensPanel::ScreensPanel (wxWindow* parent)
	: wxPanel (parent, wxID_ANY)
	, _ignore_selection_change (false)
{
	wxBoxSizer* sizer = new wxBoxSizer (wxVERTICAL);

#ifdef __WXGTK3__
	int const height = 30;
#else
	int const height = -1;
#endif

	_search = new wxSearchCtrl (this, wxID_ANY, wxEmptyString, wxDefaultPosition, wxSize(200, height));
#ifndef __WXGTK3__
	/* The cancel button seems to be strangely broken in GTK3; clicking on it twice sometimes works */
	_search->ShowCancelButton (true);
#endif
	sizer->Add (_search, 0, wxBOTTOM, DCPOMATIC_SIZER_GAP);

	auto targets = new wxBoxSizer (wxHORIZONTAL);
	_targets = new TreeCtrl (this);
	targets->Add (_targets, 1, wxEXPAND | wxRIGHT, DCPOMATIC_SIZER_GAP);

	add_cinemas ();

	auto target_buttons = new wxBoxSizer (wxVERTICAL);

	_add_cinema = new Button (this, _("Add Cinema..."));
	target_buttons->Add (_add_cinema, 1, wxEXPAND | wxBOTTOM, DCPOMATIC_BUTTON_STACK_GAP);
	_edit_cinema = new Button (this, _("Edit Cinema..."));
	target_buttons->Add (_edit_cinema, 1, wxEXPAND | wxBOTTOM, DCPOMATIC_BUTTON_STACK_GAP);
	_remove_cinema = new Button (this, _("Remove Cinema"));
	target_buttons->Add (_remove_cinema, 1, wxEXPAND | wxBOTTOM, DCPOMATIC_BUTTON_STACK_GAP);
	_add_screen = new Button (this, _("Add Screen..."));
	target_buttons->Add (_add_screen, 1, wxEXPAND | wxBOTTOM, DCPOMATIC_BUTTON_STACK_GAP);
	_edit_screen = new Button (this, _("Edit Screen..."));
	target_buttons->Add (_edit_screen, 1, wxEXPAND | wxBOTTOM, DCPOMATIC_BUTTON_STACK_GAP);
	_remove_screen = new Button (this, _("Remove Screen"));
	target_buttons->Add (_remove_screen, 1, wxEXPAND | wxBOTTOM, DCPOMATIC_BUTTON_STACK_GAP);

	targets->Add (target_buttons, 0, 0);

	sizer->Add (targets, 1, wxEXPAND);

	_search->Bind        (wxEVT_TEXT, boost::bind (&ScreensPanel::search_changed, this));
	_targets->Bind       (wxEVT_TREE_SEL_CHANGED, &ScreensPanel::selection_changed_shim, this);

	_add_cinema->Bind    (wxEVT_BUTTON, boost::bind (&ScreensPanel::add_cinema_clicked, this));
	_edit_cinema->Bind   (wxEVT_BUTTON, boost::bind (&ScreensPanel::edit_cinema_clicked, this));
	_remove_cinema->Bind (wxEVT_BUTTON, boost::bind (&ScreensPanel::remove_cinema_clicked, this));

	_add_screen->Bind    (wxEVT_BUTTON, boost::bind (&ScreensPanel::add_screen_clicked, this));
	_edit_screen->Bind   (wxEVT_BUTTON, boost::bind (&ScreensPanel::edit_screen_clicked, this));
	_remove_screen->Bind (wxEVT_BUTTON, boost::bind (&ScreensPanel::remove_screen_clicked, this));

	SetSizer (sizer);
}


ScreensPanel::~ScreensPanel ()
{
	_targets->Unbind (wxEVT_TREE_SEL_CHANGED, &ScreensPanel::selection_changed_shim, this);
}


void
ScreensPanel::setup_sensitivity ()
{
	bool const sc = _selected_cinemas.size() == 1;
	bool const ss = _selected_screens.size() == 1;

	_edit_cinema->Enable (sc);
	_remove_cinema->Enable (_selected_cinemas.size() >= 1);

	_add_screen->Enable (sc);
	_edit_screen->Enable (ss);
	_remove_screen->Enable (_selected_screens.size() >= 1);
}


optional<wxTreeItemId>
ScreensPanel::add_cinema (shared_ptr<Cinema> c)
{
	auto search = wx_to_std (_search->GetValue ());
	transform (search.begin(), search.end(), search.begin(), ::tolower);

	if (!search.empty ()) {
		auto name = c->name;
		transform (name.begin(), name.end(), name.begin(), ::tolower);
		if (name.find (search) == string::npos) {
			return optional<wxTreeItemId>();
		}
	}

	auto id = _targets->AppendItem(_root, std_to_wx(c->name));

	_cinemas[id] = c;

	for (auto i: c->screens()) {
		add_screen (c, i);
	}

	_targets->SortChildren (_root);

	return id;
}


optional<wxTreeItemId>
ScreensPanel::add_screen (shared_ptr<Cinema> c, shared_ptr<Screen> s)
{
	auto i = _cinemas.begin();
	while (i != _cinemas.end() && i->second != c) {
		++i;
	}

	if (i == _cinemas.end()) {
		return {};
	}

	_screens[_targets->AppendItem (i->first, std_to_wx (s->name))] = s;
	return i->first;
}


void
ScreensPanel::add_cinema_clicked ()
{
	auto d = new CinemaDialog (GetParent(), _("Add Cinema"));
	if (d->ShowModal () == wxID_OK) {
		auto c = make_shared<Cinema>(d->name(), d->emails(), d->notes(), d->utc_offset_hour(), d->utc_offset_minute());
		Config::instance()->add_cinema (c);
		auto id = add_cinema (c);
		if (id) {
			_targets->Unselect ();
			_targets->SelectItem (*id);
		}
	}

	d->Destroy ();
}


void
ScreensPanel::edit_cinema_clicked ()
{
	if (_selected_cinemas.size() != 1) {
		return;
	}

	auto c = *_selected_cinemas.begin();

	auto d = new CinemaDialog (
		GetParent(), _("Edit cinema"), c.second->name, c.second->emails, c.second->notes, c.second->utc_offset_hour(), c.second->utc_offset_minute()
		);

	if (d->ShowModal() == wxID_OK) {
		c.second->name = d->name ();
		c.second->emails = d->emails ();
		c.second->notes = d->notes ();
		c.second->set_utc_offset_hour (d->utc_offset_hour ());
		c.second->set_utc_offset_minute (d->utc_offset_minute ());
		_targets->SetItemText (c.first, std_to_wx (d->name()));
		Config::instance()->changed (Config::CINEMAS);
	}

	d->Destroy ();
}


void
ScreensPanel::remove_cinema_clicked ()
{
	if (_selected_cinemas.size() == 1) {
		if (!confirm_dialog(this, wxString::Format(_("Are you sure you want to remove the cinema '%s'?"), std_to_wx(_selected_cinemas.begin()->second->name)))) {
			return;
		}
	} else {
		if (!confirm_dialog(this, wxString::Format(_("Are you sure you want to remove %d cinemas?"), int(_selected_cinemas.size())))) {
			return;
		}
	}

	for (auto const& i: _selected_cinemas) {
		Config::instance()->remove_cinema (i.second);
		_targets->Delete (i.first);
	}

	selection_changed ();
}


void
ScreensPanel::add_screen_clicked ()
{
	if (_selected_cinemas.size() != 1) {
		return;
	}

	auto c = _selected_cinemas.begin()->second;

	auto d = new ScreenDialog (GetParent(), _("Add Screen"));
	if (d->ShowModal () != wxID_OK) {
		d->Destroy ();
		return;
	}

	for (auto i: c->screens ()) {
		if (i->name == d->name()) {
			error_dialog (
				GetParent(),
				wxString::Format (
					_("You cannot add a screen called '%s' as the cinema already has a screen with this name."),
					std_to_wx(d->name()).data()
					)
				);
			return;
		}
	}

	auto s = std::make_shared<Screen>(d->name(), d->notes(), d->recipient(), d->trusted_devices());
	c->add_screen (s);
	auto id = add_screen (c, s);
	if (id) {
		_targets->Expand (id.get ());
	}

	Config::instance()->changed (Config::CINEMAS);

	d->Destroy ();
}


void
ScreensPanel::edit_screen_clicked ()
{
	if (_selected_screens.size() != 1) {
		return;
	}

	auto s = *_selected_screens.begin();

	auto d = new ScreenDialog (GetParent(), _("Edit screen"), s.second->name, s.second->notes, s.second->recipient, s.second->trusted_devices);
	if (d->ShowModal() != wxID_OK) {
		d->Destroy ();
		return;
	}

	auto c = s.second->cinema;
	for (auto i: c->screens ()) {
		if (i != s.second && i->name == d->name()) {
			error_dialog (
				GetParent(),
				wxString::Format (
					_("You cannot change this screen's name to '%s' as the cinema already has a screen with this name."),
					std_to_wx(d->name()).data()
					)
				);
			return;
		}
	}

	s.second->name = d->name ();
	s.second->notes = d->notes ();
	s.second->recipient = d->recipient ();
	s.second->trusted_devices = d->trusted_devices ();
	_targets->SetItemText (s.first, std_to_wx (d->name()));
	Config::instance()->changed (Config::CINEMAS);

	d->Destroy ();
}


void
ScreensPanel::remove_screen_clicked ()
{
	if (_selected_screens.size() == 1) {
		if (!confirm_dialog(this, wxString::Format(_("Are you sure you want to remove the screen '%s'?"), std_to_wx(_selected_screens.begin()->second->name)))) {
			return;
		}
	} else {
		if (!confirm_dialog(this, wxString::Format(_("Are you sure you want to remove %d screens?"), int(_selected_screens.size())))) {
			return;
		}
	}

	for (auto const& i: _selected_screens) {
		auto j = _cinemas.begin ();
		while (j != _cinemas.end ()) {
			auto sc = j->second->screens ();
			if (find (sc.begin(), sc.end(), i.second) != sc.end ()) {
				break;
			}

			++j;
		}

		if (j == _cinemas.end()) {
			continue;
		}

		j->second->remove_screen (i.second);
		_targets->Delete (i.first);
	}

	Config::instance()->changed (Config::CINEMAS);
}


list<shared_ptr<Screen>>
ScreensPanel::screens () const
{
	list<shared_ptr<Screen>> s;

	for (auto const& i: _selected_cinemas) {
		for (auto j: i.second->screens()) {
			s.push_back (j);
		}
	}

	for (auto const& i: _selected_screens) {
		s.push_back (i.second);
	}

	s.sort ();
	s.unique ();

	return s;
}


void
ScreensPanel::selection_changed_shim (wxTreeEvent &)
{
	selection_changed ();
}


void
ScreensPanel::selection_changed ()
{
	if (_ignore_selection_change) {
		return;
	}

	wxArrayTreeItemIds s;
	_targets->GetSelections (s);

	_selected_cinemas.clear ();
	_selected_screens.clear ();

	for (size_t i = 0; i < s.GetCount(); ++i) {
		auto j = _cinemas.find (s[i]);
		if (j != _cinemas.end ()) {
			_selected_cinemas[j->first] = j->second;
		}
		auto k = _screens.find (s[i]);
		if (k != _screens.end ()) {
			_selected_screens[k->first] = k->second;
		}
	}

	setup_sensitivity ();
	ScreensChanged ();
}


void
ScreensPanel::add_cinemas ()
{
	_root = _targets->AddRoot ("Foo");

	for (auto i: Config::instance()->cinemas()) {
		add_cinema (i);
	}
}


void
ScreensPanel::search_changed ()
{
	_targets->DeleteAllItems ();
	_cinemas.clear ();
	_screens.clear ();

	add_cinemas ();

	_ignore_selection_change = true;

	for (auto const& i: _selected_cinemas) {
		/* The wxTreeItemIds will now be different, so we must search by cinema */
		auto j = _cinemas.begin ();
		while (j != _cinemas.end() && j->second != i.second) {
			++j;
		}

		if (j != _cinemas.end()) {
			_targets->SelectItem (j->first);
		}
	}

	for (auto const& i: _selected_screens) {
		auto j = _screens.begin ();
		while (j != _screens.end() && j->second != i.second) {
			++j;
		}

		if (j != _screens.end()) {
			_targets->SelectItem (j->first);
		}
	}

	_ignore_selection_change = false;
}


wxIMPLEMENT_DYNAMIC_CLASS (TreeCtrl, wxTreeCtrl);


int
TreeCtrl::OnCompareItems (wxTreeItemId const& a, wxTreeItemId const& b)
{
	return strcoll (wx_to_std(GetItemText(a)).c_str(), wx_to_std(GetItemText(b)).c_str());
}

