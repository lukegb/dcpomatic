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


#include "file_log.h"
#include "cross.h"
#include "config.h"
#include <cstdio>
#include <iostream>
#include <cerrno>


using std::cout;
using std::string;
using std::max;
using std::shared_ptr;


/** @param file Filename to write log to */
FileLog::FileLog (boost::filesystem::path file)
	: _file (file)
{
	set_types (Config::instance()->log_types());
}


FileLog::FileLog (boost::filesystem::path file, int types)
	: _file (file)
{
	set_types (types);
}


void
FileLog::do_log (shared_ptr<const LogEntry> entry)
{
	auto f = fopen_boost (_file, "a");
	if (!f) {
		cout << "(could not log to " << _file.string() << " error " << errno << "): " << entry->get() << "\n";
		return;
	}

	fprintf (f, "%s\n", entry->get().c_str());
	fclose (f);
}


string
FileLog::head_and_tail (int amount) const
{
	boost::mutex::scoped_lock lm (_mutex);

	uintmax_t head_amount = amount;
	uintmax_t tail_amount = amount;
	boost::system::error_code ec;
	uintmax_t size = boost::filesystem::file_size (_file, ec);
	if (size == static_cast<uintmax_t>(-1)) {
		return "";
	}

	if (size < (head_amount + tail_amount)) {
		head_amount = size;
		tail_amount = 0;
	}

	auto f = fopen_boost (_file, "r");
	if (!f) {
		return "";
	}

	string out;

	std::vector<char> buffer(max(head_amount, tail_amount) + 1);

	int N = fread (buffer.data(), 1, head_amount, f);
	buffer[N] = '\0';
	out += string (buffer.data());

	if (tail_amount > 0) {
		out +=  "\n .\n .\n .\n";

		fseek (f, - tail_amount - 1, SEEK_END);

		N = fread (buffer.data(), 1, tail_amount, f);
		buffer[N] = '\0';
		out += string (buffer.data()) + "\n";
	}

	fclose (f);

	return out;
}
