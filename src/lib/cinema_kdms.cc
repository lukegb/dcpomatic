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

#include "exceptions.h"
#include "cinema_kdms.h"
#include "cinema.h"
#include "screen.h"
#include "config.h"
#include "util.h"
#include "emailer.h"
#include "compose.hpp"
#include "log.h"
#include <zip.h>
#include <boost/foreach.hpp>
#include <boost/make_shared.hpp>

#include "i18n.h"

using std::list;
using std::cout;
using std::string;
using std::runtime_error;
using boost::shared_ptr;
using boost::make_shared;

void
CinemaKDMs::make_zip_file (string film_name, boost::filesystem::path zip_file) const
{
	int error;
	struct zip* zip = zip_open (zip_file.string().c_str(), ZIP_CREATE | ZIP_EXCL, &error);
	if (!zip) {
		if (error == ZIP_ER_EXISTS) {
			throw FileError ("ZIP file already exists", zip_file);
		}
		throw FileError ("could not create ZIP file", zip_file);
	}

	list<shared_ptr<string> > kdm_strings;

	BOOST_FOREACH (ScreenKDM const & i, screen_kdms) {
		shared_ptr<string> kdm = boost::make_shared<string> (i.kdm.as_xml ());
		kdm_strings.push_back (kdm);

		struct zip_source* source = zip_source_buffer (zip, kdm->c_str(), kdm->length(), 0);
		if (!source) {
			throw runtime_error ("could not create ZIP source");
		}

		if (zip_add (zip, i.filename(film_name).c_str(), source) == -1) {
			throw runtime_error ("failed to add KDM to ZIP archive");
		}
	}

	if (zip_close (zip) == -1) {
		throw runtime_error ("failed to close ZIP archive");
	}
}

list<CinemaKDMs>
CinemaKDMs::collect (list<ScreenKDM> screen_kdms)
{
	list<CinemaKDMs> cinema_kdms;

	while (!screen_kdms.empty ()) {

		/* Get all the screens from a single cinema */

		CinemaKDMs ck;

		list<ScreenKDM>::iterator i = screen_kdms.begin ();
		ck.cinema = i->screen->cinema;
		ck.screen_kdms.push_back (*i);
		list<ScreenKDM>::iterator j = i;
		++i;
		screen_kdms.remove (*j);

		while (i != screen_kdms.end ()) {
			if (i->screen->cinema == ck.cinema) {
				ck.screen_kdms.push_back (*i);
				list<ScreenKDM>::iterator j = i;
				++i;
				screen_kdms.remove (*j);
			} else {
				++i;
			}
		}

		cinema_kdms.push_back (ck);
	}

	return cinema_kdms;
}

void
CinemaKDMs::write_zip_files (string film_name, list<CinemaKDMs> cinema_kdms, boost::filesystem::path directory)
{
	BOOST_FOREACH (CinemaKDMs const & i, cinema_kdms) {
		boost::filesystem::path path = directory;
		path /= tidy_for_filename (i.cinema->name) + ".zip";
		i.make_zip_file (film_name, path);
	}
}

/** @param log Log to write email session transcript to, or 0 */
/* XXX: should probably get from/to from the KDMs themselves */
void
CinemaKDMs::email (
	string film_name, string cpl_name, list<CinemaKDMs> cinema_kdms, dcp::LocalTime from, dcp::LocalTime to, shared_ptr<Log> log
	)
{
	Config* config = Config::instance ();

	if (config->mail_server().empty()) {
		throw NetworkError (_("No mail server configured in preferences"));
	}

	BOOST_FOREACH (CinemaKDMs const & i, cinema_kdms) {

		boost::filesystem::path zip_file = boost::filesystem::temp_directory_path ();
		zip_file /= boost::filesystem::unique_path().string() + ".zip";
		i.make_zip_file (film_name, zip_file);

		string subject = config->kdm_subject();
		SafeStringStream start;
		start << from.date() << " " << from.time_of_day();
		SafeStringStream end;
		end << to.date() << " " << to.time_of_day();
		boost::algorithm::replace_all (subject, "$CPL_NAME", cpl_name);
		boost::algorithm::replace_all (subject, "$START_TIME", start.str ());
		boost::algorithm::replace_all (subject, "$END_TIME", end.str ());
		boost::algorithm::replace_all (subject, "$CINEMA_NAME", i.cinema->name);

		string body = config->kdm_email().c_str();
		boost::algorithm::replace_all (body, "$CPL_NAME", cpl_name);
		boost::algorithm::replace_all (body, "$START_TIME", start.str ());
		boost::algorithm::replace_all (body, "$END_TIME", end.str ());
		boost::algorithm::replace_all (body, "$CINEMA_NAME", i.cinema->name);

		SafeStringStream screens;
		BOOST_FOREACH (ScreenKDM const & j, i.screen_kdms) {
			screens << j.screen->name << ", ";
		}
		boost::algorithm::replace_all (body, "$SCREENS", screens.str().substr (0, screens.str().length() - 2));

		Emailer email (config->kdm_from(), i.cinema->emails, subject, body);

		BOOST_FOREACH (string i, config->kdm_cc()) {
			email.add_cc (i);
		}
		if (!config->kdm_bcc().empty ()) {
			email.add_bcc (config->kdm_bcc ());
		}

		string const name = tidy_for_filename(i.cinema->name) + "_" + tidy_for_filename(film_name) + ".zip";
		email.add_attachment (zip_file, name, "application/zip");

		Config* c = Config::instance ();

		try {
			email.send (c->mail_server(), c->mail_port(), c->mail_user(), c->mail_password());
		} catch (...) {
			if (log) {
				log->log ("Email content follows", LogEntry::TYPE_DEBUG_EMAIL);
				log->log (email.email(), LogEntry::TYPE_DEBUG_EMAIL);
				log->log ("Email session follows", LogEntry::TYPE_DEBUG_EMAIL);
				log->log (email.notes(), LogEntry::TYPE_DEBUG_EMAIL);
			}
			throw;
		}

		if (log) {
			log->log ("Email content follows", LogEntry::TYPE_DEBUG_EMAIL);
			log->log (email.email(), LogEntry::TYPE_DEBUG_EMAIL);
			log->log ("Email session follows", LogEntry::TYPE_DEBUG_EMAIL);
			log->log (email.notes(), LogEntry::TYPE_DEBUG_EMAIL);
		}
	}
}
