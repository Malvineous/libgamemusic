/**
 * @file   patchbank-fmt-ibk.hpp
 * @brief  PatchBank interface to .IBK (Instrument Bank) file format.
 *
 * Copyright (C) 2010-2011 Adam Nielsen <malvineous@shikadi.net>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "patchbank-fmt-ibk.hpp"

using namespace camoto::gamemusic;

PatchBank_IBK::PatchBank_IBK(std::iostream data)
	throw ()
{
	// TODO: read file into vector
	// TODO: use INS reader, if this is just a list of INS files?
}

PatchBank_IBK::~PatchBank_IBK()
	throw ()
{
}

std::string PatchBank_IBK::getName()
	throw ()
{
	// This format doesn't support an overarching name for the whole bank.
	return std::string();
}

void PatchBank_IBK::setPatchCount(unsigned int newCount)
	throw ()
{
	// TODO: can you even change the size of an IBK file?
}

void PatchBank_IBK::setPatch(unsigned int index, PatchPtr newPatch)
	throw (bad_patch)
{
	this->SingleTypePatchBank<OPLPatch>::setPatch(index, newPatch);
	// TODO: write out newPatch to the stream
	return;
}
