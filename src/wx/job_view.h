/*
    Copyright (C) 2012-2017 Carl Hetherington <cth@carlh.net>

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

#ifndef DCPOMATIC_JOB_VIEW_H
#define DCPOMATIC_JOB_VIEW_H

#include <boost/noncopyable.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/signals2.hpp>

class Job;
class wxScrolledWindow;
class wxWindow;
class wxFlexGridSizer;
class wxCommandEvent;
class wxBoxSizer;
class wxGauge;
class wxStaticText;
class wxButton;
class wxSizer;

class JobView : public boost::noncopyable
{
public:
	JobView (boost::shared_ptr<Job> job, wxWindow* parent, wxWindow* container, wxFlexGridSizer* table);
	virtual ~JobView () {}

	virtual int insert_position () const = 0;
	virtual void job_list_changed () {}

	void setup ();
	void maybe_pulse ();
	void insert (int pos);
	void detach ();

	boost::shared_ptr<Job> job () const {
		return _job;
	}

protected:
	virtual void finished ();

	boost::shared_ptr<Job> _job;
	wxFlexGridSizer* _table;
	wxBoxSizer* _buttons;
	wxBoxSizer* _gauge_message;

private:

	virtual void finish_setup (wxWindow *, wxSizer *) {}

	void progress ();
	void details_clicked (wxCommandEvent &);
	void cancel_clicked (wxCommandEvent &);

	wxWindow* _parent;
	wxWindow* _container;
	wxGauge* _gauge;
	wxStaticText* _message;
	wxButton* _cancel;
	wxButton* _details;
	std::string _last_message;

	boost::signals2::scoped_connection _progress_connection;
	boost::signals2::scoped_connection _finished_connection;
};

#endif
