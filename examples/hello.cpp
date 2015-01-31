#include <iostream>
#include <camoto/stream_file.hpp>
#include <camoto/gamemusic.hpp>

using namespace camoto;
using namespace camoto::gamemusic;

int main(void)
{
	// Get hold of the Manager class
	ManagerPtr manager = getManager();

	// Use the manager to look up a particular music format
	MusicTypePtr musicType = manager->getMusicTypeByCode("cmf-creativelabs");

	// Open a music file on disk
	stream::file_sptr file(new stream::file());
	file->open("funky.cmf");

	// We cheat here - we should check and load any supplementary files, but
	// for the sake of keeping this example simple we know this format doesn't
	// need any supps.
	camoto::SuppData supps;

	// Use the format handler to read in the file we opened.
	MusicPtr song = musicType->read(file, supps);

	// Find out how many instruments (patches) the song has.
	std::cout << "There are " << song->patches->size()
		<< " instruments in this song.\n";

	// Look through the tags to see if there's a title present.
	Metadata::TypeMap::iterator i = song->metadata.find(Metadata::Title);
	if (i == song->metadata.end()) {
		std::cout << "This song has no title.\n";
	} else {
		std::cout << "This song is called: " << i->second << "\n";
	}

	// No cleanup required because all the Ptr variables are shared pointers,
	// which get destroyed automatically when they go out of scope (and nobody
	// else is using them!)

	return 0;
}
