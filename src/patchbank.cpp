/**
 * @file   patchbank.cpp
 * @brief  Implementation of top-level PatchBank class, for managing
 *         collections of patches.
 *
 * Copyright (C) 2010-2012 Adam Nielsen <malvineous@shikadi.net>
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

#include <camoto/gamemusic/patchbank.hpp>

using namespace camoto::gamemusic;


PatchBank::PatchBank()
	throw ()
{
}

PatchBank::~PatchBank()
	throw ()
{
}

std::string PatchBank::getName() const
	throw ()
{
	return std::string();
}

unsigned int PatchBank::getPatchCount() const
	throw ()
{
	return this->patches.size();
}

void PatchBank::setPatchCount(unsigned int newCount)
	throw ()
{
	this->patches.resize(newCount);
	return;
}

const PatchPtr PatchBank::getPatch(unsigned int index) const
	throw ()
{
	assert(index < this->getPatchCount());
	return this->patches[index];
}

void PatchBank::setPatch(unsigned int index, PatchPtr newPatch)
	throw (bad_patch)
{
	assert(index < this->getPatchCount());
	this->patches[index] = newPatch;
	return;
}
