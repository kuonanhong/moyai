#include "Font.h"

bool Font::loadFromTTF(const char *path, const wchar_t *codes, int pixelsz ) {        
	pixel_size = pixelsz;
	font = texture_font_new( atlas, path, pixelsz );
	if(!font){
		return false;
	}
	texture_font_load_glyphs( font, codes);
	return true;
}
