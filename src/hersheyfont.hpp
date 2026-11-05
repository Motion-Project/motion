// hersheyfont.hpp - based on:
//
// hersheyfont.h - Copyright (C) 2013 Kamal Mostafa <kamal@whence.com>
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License along
// with this program; if not, write to the Free Software Foundation, Inc.,
// 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
//

#pragma once
#include <vector>
#include <array>
#include <cassert>
#include <string>

namespace hersheyfont {

struct vertex {
	short			x;
	short			y;
};

struct path {
	std::vector<vertex>	verts;
};

struct glyph {
	unsigned int		glyphnum;
	unsigned short		width;
	std::vector<path>	paths;
};

struct font {
    std::string		fontname;
	std::array<glyph, 256>	glyphs;	/* indexed by ASCII value */
};

class cls_font{
    private:
        font m_font;
        bool m_loaded = false;

    int load_glyph( glyph& hg, std::string& jhf_line ) const;
    int jhf_font_load( const std::string& jhffile );

    public:
    //
    // Load Hershey fontname (e.g. "rowmans") into a new struct hershey_font.
    // The associated .jhf font file will be loaded from .../share/hershey-fonts/.
    //
    int load( const std::string& fontname );

    const glyph& get_glyph(unsigned char c) {
        assert(m_loaded);
        return m_font.glyphs[c];
    }

    const std::string& get_fontname() const {
        return m_font.fontname;
    }

    bool is_loaded() const {
        return m_loaded;
    }

};
}