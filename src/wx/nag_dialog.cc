/*
    Copyright (C) 2017-2021 Carl Hetherington <cth@carlh.net>

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


#include "nag_dialog.h"
#include "wx_util.h"
#include "static_text.h"
#include "check_box.h"


using std::shared_ptr;
#if BOOST_VERSION >= 106100
using namespace boost::placeholders;
#endif


static constexpr int width = 400;


NagDialog::NagDialog (wxWindow* parent, Config::Nag nag, wxString message, bool can_cancel)
	: wxDialog (parent, wxID_ANY, _("Important notice"))
	, _nag (nag)
{
	auto sizer = new wxBoxSizer (wxVERTICAL);
	_text = new StaticText (this, wxEmptyString, wxDefaultPosition, wxSize(width, 300));
	sizer->Add (_text, 1, wxEXPAND | wxALL, DCPOMATIC_DIALOG_BORDER);

	auto b = new CheckBox (this, _("Don't show this message again"));
	sizer->Add (b, 0, wxALL, 6);
	b->Bind (wxEVT_CHECKBOX, bind (&NagDialog::shut_up, this, _1));

	int flags = wxOK;
	if (can_cancel) {
		flags |= wxCANCEL;
	}
	auto buttons = CreateSeparatedButtonSizer (flags);
	if (buttons) {
		sizer->Add(buttons, wxSizerFlags().Expand().DoubleBorder());
	}

	_text->SetLabelMarkup (message);
	_text->Wrap (width);

	SetSizer (sizer);
	sizer->Layout ();
	sizer->SetSizeHints (this);
}


void
NagDialog::shut_up (wxCommandEvent& ev)
{
	Config::instance()->set_nagged (_nag, ev.IsChecked());
}


/** @return true if the user clicked Cancel */
bool
NagDialog::maybe_nag (wxWindow* parent, Config::Nag nag, wxString message, bool can_cancel)
{
	if (Config::instance()->nagged(nag)) {
		return false;
	}

	auto d = new NagDialog (parent, nag, message, can_cancel);
	int const r = d->ShowModal();
	d->Destroy ();

	return r == wxID_CANCEL;
}
