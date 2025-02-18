/*
    Copyright (C) 2013-2022 Carl Hetherington <cth@carlh.net>

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


/** @file  src/tools/dcpomatic_kdm_cli.cc
 *  @brief Command-line program to generate KDMs.
 */


#include "cinema.h"
#include "config.h"
#include "dkdm_wrapper.h"
#include "emailer.h"
#include "exceptions.h"
#include "film.h"
#include "kdm_with_metadata.h"
#include "screen.h"
#include <dcp/certificate.h>
#include <dcp/decrypted_kdm.h>
#include <dcp/encrypted_kdm.h>
#include <getopt.h>


using std::dynamic_pointer_cast;
using std::list;
using std::make_shared;
using std::runtime_error;
using std::shared_ptr;
using std::string;
using std::vector;
using boost::optional;
using boost::bind;
#if BOOST_VERSION >= 106100
using namespace boost::placeholders;
#endif
using namespace dcpomatic;


static void
help (std::function<void (string)> out)
{
	out (String::compose("Syntax: %1 [OPTION] <FILM|CPL-ID|DKDM>", program_name));
	out ("  -h, --help                               show this help");
	out ("  -o, --output                             output file or directory");
	out ("  -K, --filename-format                    filename format for KDMs");
	out ("  -Z, --container-name-format              filename format for ZIP containers");
	out ("  -f, --valid-from                         valid from time (in local time zone of the cinema) (e.g. \"2013-09-28 01:41:51\") or \"now\"");
	out ("  -t, --valid-to                           valid to time (in local time zone of the cinema) (e.g. \"2014-09-28 01:41:51\")");
	out ("  -d, --valid-duration                     valid duration (e.g. \"1 day\", \"4 hours\", \"2 weeks\")");
	out ("  -F, --formulation                        modified-transitional-1, multiple-modified-transitional-1, dci-any or dci-specific [default modified-transitional-1]");
	out ("  -p, --disable-forensic-marking-picture   disable forensic marking of pictures essences");
	out ("  -a, --disable-forensic-marking-audio     disable forensic marking of audio essences (optionally above a given channel, e.g 12)");
	out ("  -e, --email                              email KDMs to cinemas");
	out ("  -z, --zip                                ZIP each cinema's KDMs into its own file");
	out ("  -v, --verbose                            be verbose");
	out ("  -c, --cinema                             specify a cinema, either by name or email address");
	out ("  -S, --screen                             screen description");
	out ("  -C, --certificate                        file containing projector certificate");
	out ("  -T, --trusted-device                     file containing a trusted device's certificate");
	out ("      --list-cinemas                       list known cinemas from the DCP-o-matic settings");
	out ("      --list-dkdm-cpls                     list CPLs for which DCP-o-matic has DKDMs");
	out ("");
	out ("CPL-ID must be the ID of a CPL that is mentioned in DCP-o-matic's DKDM list.");
	out ("");
	out ("For example:");
	out ("");
	out ("Create KDMs for my_great_movie to play in all of Fred's Cinema's screens for the next two weeks and zip them up.");
	out ("(Fred's Cinema must have been set up in DCP-o-matic's KDM window)");
	out ("");
	out (String::compose("\t%1 -c \"Fred's Cinema\" -f now -d \"2 weeks\" -z my_great_movie", program_name));
}


class KDMCLIError : public std::runtime_error
{
public:
	KDMCLIError (std::string message)
		: std::runtime_error (String::compose("%1: %2", program_name, message).c_str())
	{}
};


static boost::posix_time::ptime
time_from_string (string t)
{
	if (t == "now") {
		return boost::posix_time::second_clock::local_time ();
	}

	return boost::posix_time::time_from_string (t);
}


static boost::posix_time::time_duration
duration_from_string (string d)
{
	int N;
	char unit_buf[64] = "\0";
	sscanf (d.c_str(), "%d %63s", &N, unit_buf);
	string const unit (unit_buf);

	if (N == 0) {
		throw KDMCLIError (String::compose("could not understand duration \"%1\"", d));
	}

	if (unit == "year" || unit == "years") {
		return boost::posix_time::time_duration (N * 24 * 365, 0, 0, 0);
	} else if (unit == "week" || unit == "weeks") {
		return boost::posix_time::time_duration (N * 24 * 7, 0, 0, 0);
	} else if (unit == "day" || unit == "days") {
		return boost::posix_time::time_duration (N * 24, 0, 0, 0);
	} else if (unit == "hour" || unit == "hours") {
		return boost::posix_time::time_duration (N, 0, 0, 0);
	}

	throw KDMCLIError (String::compose("could not understand duration \"%1\"", d));
}


static bool
always_overwrite ()
{
	return true;
}


static
void
write_files (
	list<KDMWithMetadataPtr> kdms,
	bool zip,
	boost::filesystem::path output,
	dcp::NameFormat container_name_format,
	dcp::NameFormat filename_format,
	bool verbose,
	std::function<void (string)> out
	)
{
	if (zip) {
		int const N = write_zip_files (
			collect (kdms),
			output,
			container_name_format,
			filename_format,
			bind (&always_overwrite)
			);

		if (verbose) {
			out (String::compose("Wrote %1 ZIP files to %2", N, output));
		}
	} else {
		int const N = write_files (
			kdms, output, filename_format,
			bind (&always_overwrite)
			);

		if (verbose) {
			out (String::compose("Wrote %1 KDM files to %2", N, output));
		}
	}
}


static
shared_ptr<Cinema>
find_cinema (string cinema_name)
{
	auto cinemas = Config::instance()->cinemas ();
	auto i = cinemas.begin();
	while (
		i != cinemas.end() &&
		(*i)->name != cinema_name &&
		find ((*i)->emails.begin(), (*i)->emails.end(), cinema_name) == (*i)->emails.end()) {

		++i;
	}

	if (i == cinemas.end ()) {
		throw KDMCLIError (String::compose("could not find cinema \"%1\"", cinema_name));
	}

	return *i;
}


static
void
from_film (
	list<shared_ptr<Screen>> screens,
	boost::filesystem::path film_dir,
	bool verbose,
	boost::filesystem::path output,
	dcp::NameFormat container_name_format,
	dcp::NameFormat filename_format,
	boost::posix_time::ptime valid_from,
	boost::posix_time::ptime valid_to,
	dcp::Formulation formulation,
	bool disable_forensic_marking_picture,
	optional<int> disable_forensic_marking_audio,
	bool email,
	bool zip,
	std::function<void (string)> out
	)
{
	shared_ptr<Film> film;
	try {
		film = make_shared<Film>(film_dir);
		film->read_metadata ();
		if (verbose) {
			out (String::compose("Read film %1", film->name()));
		}
	} catch (std::exception& e) {
		throw KDMCLIError (String::compose("error reading film \"%1\" (%2)", film_dir.string(), e.what()));
	}

	/* XXX: allow specification of this */
	vector<CPLSummary> cpls = film->cpls ();
	if (cpls.empty ()) {
		throw KDMCLIError ("no CPLs found in film");
	} else if (cpls.size() > 1) {
		throw KDMCLIError ("more than one CPL found in film");
	}

	auto cpl = cpls.front().cpl_file;

	try {
		list<KDMWithMetadataPtr> kdms;
		for (auto i: screens) {
			auto p = kdm_for_screen (film, cpl, i, valid_from, valid_to, formulation, disable_forensic_marking_picture, disable_forensic_marking_audio);
			if (p) {
				kdms.push_back (p);
			}
		}
		write_files (kdms, zip, output, container_name_format, filename_format, verbose, out);
		if (email) {
			send_emails ({kdms}, container_name_format, filename_format, film->dcp_name());
		}
	} catch (FileError& e) {
		throw KDMCLIError (String::compose("%1 (%2)", e.what(), e.file().string()));
	}
}


static
optional<dcp::EncryptedKDM>
sub_find_dkdm (shared_ptr<DKDMGroup> group, string cpl_id)
{
	for (auto i: group->children()) {
		auto g = dynamic_pointer_cast<DKDMGroup>(i);
		if (g) {
			auto dkdm = sub_find_dkdm (g, cpl_id);
			if (dkdm) {
				return dkdm;
			}
		} else {
			auto d = dynamic_pointer_cast<DKDM>(i);
			assert (d);
			if (d->dkdm().cpl_id() == cpl_id) {
				return d->dkdm();
			}
		}
	}

	return {};
}


static
optional<dcp::EncryptedKDM>
find_dkdm (string cpl_id)
{
	return sub_find_dkdm (Config::instance()->dkdms(), cpl_id);
}


static
dcp::EncryptedKDM
kdm_from_dkdm (
	dcp::DecryptedKDM dkdm,
	dcp::Certificate target,
	vector<string> trusted_devices,
	dcp::LocalTime valid_from,
	dcp::LocalTime valid_to,
	dcp::Formulation formulation,
	bool disable_forensic_marking_picture,
	optional<int> disable_forensic_marking_audio
	)
{
	/* Signer for new KDM */
	auto signer = Config::instance()->signer_chain ();
	if (!signer->valid ()) {
		throw KDMCLIError ("signing certificate chain is invalid.");
	}

	/* Make a new empty KDM and add the keys from the DKDM to it */
	dcp::DecryptedKDM kdm (
		valid_from,
		valid_to,
		dkdm.annotation_text().get_value_or(""),
		dkdm.content_title_text(),
		dcp::LocalTime().as_string()
		);

	for (auto const& j: dkdm.keys()) {
		kdm.add_key(j);
	}

	return kdm.encrypt (signer, target, trusted_devices, formulation, disable_forensic_marking_picture, disable_forensic_marking_audio);
}


static
void
from_dkdm (
	list<shared_ptr<Screen>> screens,
	dcp::DecryptedKDM dkdm,
	bool verbose,
 	boost::filesystem::path output,
	dcp::NameFormat container_name_format,
	dcp::NameFormat filename_format,
	boost::posix_time::ptime valid_from,
	boost::posix_time::ptime valid_to,
	dcp::Formulation formulation,
	bool disable_forensic_marking_picture,
	optional<int> disable_forensic_marking_audio,
	bool email,
	bool zip,
	std::function<void (string)> out
	)
{
	dcp::NameFormat::Map values;

	try {
		list<KDMWithMetadataPtr> kdms;
		for (auto i: screens) {
			if (!i->recipient) {
				continue;
			}

			int const offset_hour = i->cinema ? i->cinema->utc_offset_hour() : 0;
			int const offset_minute = i->cinema ? i->cinema->utc_offset_minute() : 0;

			dcp::LocalTime begin(valid_from, offset_hour, offset_minute);
			dcp::LocalTime end(valid_to, offset_hour, offset_minute);

			auto const kdm = kdm_from_dkdm(
							dkdm,
							i->recipient.get(),
							i->trusted_device_thumbprints(),
							begin,
							end,
							formulation,
							disable_forensic_marking_picture,
							disable_forensic_marking_audio
							);

			dcp::NameFormat::Map name_values;
			name_values['c'] = i->cinema ? i->cinema->name : "";
			name_values['s'] = i->name;
			name_values['f'] = dkdm.annotation_text().get_value_or("");
			name_values['b'] = begin.date() + " " + begin.time_of_day(true, false);
			name_values['e'] = end.date() + " " + end.time_of_day(true, false);
			name_values['i'] = kdm.cpl_id();

			kdms.push_back (make_shared<KDMWithMetadata>(name_values, i->cinema.get(), i->cinema ? i->cinema->emails : list<string>(), kdm));
		}
		write_files (kdms, zip, output, container_name_format, filename_format, verbose, out);
		if (email) {
			send_emails ({kdms}, container_name_format, filename_format, dkdm.annotation_text().get_value_or(""));
		}
	} catch (FileError& e) {
		throw KDMCLIError (String::compose("%1 (%2)", e.what(), e.file().string()));
	}
}


static
void
dump_dkdm_group (shared_ptr<DKDMGroup> group, int indent, std::function<void (string)> out)
{
	auto const indent_string = string(indent, ' ');

	if (indent > 0) {
		out (indent_string + group->name());
	}
	for (auto i: group->children()) {
		auto g = dynamic_pointer_cast<DKDMGroup>(i);
		if (g) {
			dump_dkdm_group (g, indent + 2, out);
		} else {
			auto d = dynamic_pointer_cast<DKDM>(i);
			assert(d);
			out (indent_string + d->dkdm().cpl_id());
		}
	}
}


optional<string>
kdm_cli (int argc, char* argv[], std::function<void (string)> out)
try
{
	boost::filesystem::path output = boost::filesystem::current_path();
	auto container_name_format = Config::instance()->kdm_container_name_format();
	auto filename_format = Config::instance()->kdm_filename_format();
	optional<string> cinema_name;
	shared_ptr<Cinema> cinema;
	string screen_description;
	list<shared_ptr<Screen>> screens;
	optional<dcp::EncryptedKDM> dkdm;
	optional<boost::posix_time::ptime> valid_from;
	optional<boost::posix_time::ptime> valid_to;
	bool zip = false;
	bool list_cinemas = false;
	bool list_dkdm_cpls = false;
	optional<string> duration_string;
	bool verbose = false;
	dcp::Formulation formulation = dcp::Formulation::MODIFIED_TRANSITIONAL_1;
	bool disable_forensic_marking_picture = false;
	optional<int> disable_forensic_marking_audio;
	bool email = false;

	program_name = argv[0];

	int option_index = 0;
	while (true) {
		static struct option long_options[] = {
			{ "help", no_argument, 0, 'h'},
			{ "output", required_argument, 0, 'o'},
			{ "filename-format", required_argument, 0, 'K'},
			{ "container-name-format", required_argument, 0, 'Z'},
			{ "valid-from", required_argument, 0, 'f'},
			{ "valid-to", required_argument, 0, 't'},
			{ "valid-duration", required_argument, 0, 'd'},
			{ "formulation", required_argument, 0, 'F' },
			{ "disable-forensic-marking-picture", no_argument, 0, 'p' },
			{ "disable-forensic-marking-audio", optional_argument, 0, 'a' },
			{ "email", no_argument, 0, 'e' },
			{ "zip", no_argument, 0, 'z' },
			{ "verbose", no_argument, 0, 'v' },
			{ "cinema", required_argument, 0, 'c' },
			{ "screen", required_argument, 0, 'S' },
			{ "certificate", required_argument, 0, 'C' },
			{ "trusted-device", required_argument, 0, 'T' },
			{ "list-cinemas", no_argument, 0, 'B' },
			{ "list-dkdm-cpls", no_argument, 0, 'D' },
			{ 0, 0, 0, 0 }
		};

		int c = getopt_long (argc, argv, "ho:K:Z:f:t:d:F:pae::zvc:S:C:T:BD", long_options, &option_index);

		if (c == -1) {
			break;
		}

		switch (c) {
		case 'h':
			help (out);
			exit (EXIT_SUCCESS);
		case 'o':
			output = optarg;
			break;
		case 'K':
			filename_format = dcp::NameFormat (optarg);
			break;
		case 'Z':
			container_name_format = dcp::NameFormat (optarg);
			break;
		case 'f':
			valid_from = time_from_string (optarg);
			break;
		case 't':
			valid_to = time_from_string (optarg);
			break;
		case 'd':
			duration_string = optarg;
			break;
		case 'F':
			if (string(optarg) == "modified-transitional-1") {
				formulation = dcp::Formulation::MODIFIED_TRANSITIONAL_1;
			} else if (string(optarg) == "multiple-modified-transitional-1") {
				formulation = dcp::Formulation::MULTIPLE_MODIFIED_TRANSITIONAL_1;
			} else if (string(optarg) == "dci-any") {
				formulation = dcp::Formulation::DCI_ANY;
			} else if (string(optarg) == "dci-specific") {
				formulation = dcp::Formulation::DCI_SPECIFIC;
			} else {
				throw KDMCLIError ("unrecognised KDM formulation " + string (optarg));
			}
			break;
		case 'p':
			disable_forensic_marking_picture = true;
			break;
		case 'a':
			disable_forensic_marking_audio = 0;
			if (optarg == 0 && argv[optind] != 0 && argv[optind][0] != '-') {
				disable_forensic_marking_audio = atoi (argv[optind++]);
			} else if (optarg) {
				disable_forensic_marking_audio = atoi (optarg);
			}
			break;
		case 'e':
			email = true;
			break;
		case 'z':
			zip = true;
			break;
		case 'v':
			verbose = true;
			break;
		case 'c':
			/* This could be a cinema to search for in the configured list or the name of a cinema being
			   built up on-the-fly in the option.  Cater for both possilibities here by storing the name
			   (for lookup) and by creating a Cinema which the next Screen will be added to.
			*/
			cinema_name = optarg;
			cinema = make_shared<Cinema>(optarg, list<string>(), "", 0, 0);
			break;
		case 'S':
			screen_description = optarg;
			break;
		case 'C':
		{
			/* Make a new screen and add it to the current cinema */
			dcp::CertificateChain chain (dcp::file_to_string(optarg));
			auto screen = make_shared<Screen>(screen_description, "", chain.leaf(), vector<TrustedDevice>());
			if (cinema) {
				cinema->add_screen (screen);
			}
			screens.push_back (screen);
			break;
		}
		case 'T':
			/* A trusted device ends up in the last screen we made */
			if (!screens.empty ()) {
				screens.back()->trusted_devices.push_back(TrustedDevice(dcp::Certificate(dcp::file_to_string(optarg))));
			}
			break;
		case 'B':
			list_cinemas = true;
			break;
		case 'D':
			list_dkdm_cpls = true;
			break;
		}
	}

	if (list_cinemas) {
		auto cinemas = Config::instance()->cinemas ();
		for (auto i: cinemas) {
			out (String::compose("%1 (%2)", i->name, Emailer::address_list (i->emails)));
		}
		exit (EXIT_SUCCESS);
	}

	if (list_dkdm_cpls) {
		dump_dkdm_group (Config::instance()->dkdms(), 0, out);
		exit (EXIT_SUCCESS);
	}

	if (!duration_string && !valid_to) {
		throw KDMCLIError ("you must specify a --valid-duration or --valid-to");
	}

	if (!valid_from) {
		throw KDMCLIError ("you must specify --valid-from");
	}

	if (optind >= argc) {
		throw KDMCLIError ("no film, CPL ID or DKDM specified");
	}

	if (screens.empty()) {
		if (!cinema_name) {
			throw KDMCLIError ("you must specify either a cinema or one or more screens using certificate files");
		}

		screens = find_cinema (*cinema_name)->screens ();
	}

	if (duration_string) {
		valid_to = valid_from.get() + duration_from_string (*duration_string);
	}

	if (verbose) {
		out (String::compose("Making KDMs valid from %1 to %2", boost::posix_time::to_simple_string(valid_from.get()), boost::posix_time::to_simple_string(valid_to.get())));
	}

	string const thing = argv[optind];
	if (boost::filesystem::is_directory(thing) && boost::filesystem::is_regular_file(boost::filesystem::path(thing) / "metadata.xml")) {
		from_film (
			screens,
			thing,
			verbose,
			output,
			container_name_format,
			filename_format,
			*valid_from,
			*valid_to,
			formulation,
			disable_forensic_marking_picture,
			disable_forensic_marking_audio,
			email,
			zip,
			out
			);
	} else {
		if (boost::filesystem::is_regular_file(thing)) {
			dkdm = dcp::EncryptedKDM (dcp::file_to_string (thing));
		} else {
			dkdm = find_dkdm (thing);
		}

		if (!dkdm) {
			throw KDMCLIError ("could not find film or CPL ID corresponding to " + thing);
		}

		from_dkdm (
			screens,
			dcp::DecryptedKDM (*dkdm, Config::instance()->decryption_chain()->key().get()),
			verbose,
			output,
			container_name_format,
			filename_format,
			*valid_from,
			*valid_to,
			formulation,
			disable_forensic_marking_picture,
			disable_forensic_marking_audio,
			email,
			zip,
			out
			);
	}

	return {};
} catch (std::exception& e) {
	return string(e.what());
}

