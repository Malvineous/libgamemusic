#include <iostream>
#include <camoto/stream_file.hpp>
#include <camoto/gamemusic.hpp>

using namespace camoto;
using namespace camoto::gamemusic;

int main(void)
{
	// Use the manager to look up a particular music format
	auto musicType = MusicManager::byCode("cmf-creativelabs");

	// Open a music file on disk
	stream::input_file file("funky.cmf");

	// We cheat here - we should check and load any supplementary files, but
	// for the sake of keeping this example simple we know this format doesn't
	// need any supps.
	camoto::SuppData supps;

	// Use the format handler to read in the file we opened.
	auto song = musicType->read(file, supps);

	// Find out how many instruments (patches) the song has.
	std::cout << "There are " << song->patches->size()
		<< " instruments in this song.\n";

	// Look through the tags to see if there's a title present.
	bool hasTitle = false;
	for (auto& a : song->attributes()) {
		if (a.name.compare(CAMOTO_ATTRIBUTE_TITLE) == 0) {
			if (!a.textValue.empty()) {
				std::cout << "This song is called: " << a.textValue << "\n";
				hasTitle = true;
				break;
			}
		}
	}
	if (!hasTitle) {
		std::cout << "This song has no title.\n";
	}

	// No cleanup required because all the Ptr variables are shared pointers,
	// which get destroyed automatically when they go out of scope (and nobody
	// else is using them!)

	return 0;
}
