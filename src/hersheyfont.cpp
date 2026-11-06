// hersheyfont.cpp - based on
//
// hersheyfont.c - Copyright (C) 2013 Kamal Mostafa <kamal@whence.com>
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

#include <string>
#include <fstream>

#include <errno.h>

#include <iostream>

#include "hersheyfont.hpp"

#undef DEBUG

namespace hersheyfont
{

    //
    // Convert "Hershey-values" (offset by ASCII 'R') to integers
    //
    static inline short hershey_val(char c) { return (c - 'R'); }

    //
    // Load the (hg) structure with data from the jhf_line.
    //
    int cls_font::load_glyph(glyph &hg, std::string &jhf_line) const
    {
        char *end;

        unsigned long glyphnum, nverts;
        int leftpos, rightpos;

        // Hmm. this prevents the argument from being const.
        std::size_t len = jhf_line.length();
        if (jhf_line[len - 1] == '\n')
        {
            jhf_line[len - 1] = 0;
            len--;
        }

        if (len < 10)
        {
            fprintf(stderr, "%s: line length %ld is too short\n", __FUNCTION__, len);
            errno = ERANGE;
            return 0;
        }

        errno = 0;

        glyphnum = strtoul(jhf_line.substr(0, 5).c_str(), &end, 10);
        if (errno)
            return 0;

        nverts = strtoul(jhf_line.substr(5, 3).c_str(), &end, 10);
        if (errno)
            return 0;

        len -= 8;

        if (nverts * 2 != len)
        {
            fprintf(stderr, "%s: expected %ld (not %ld) coord bytes for nverts=%lu\n", __FUNCTION__, nverts * 2, len, nverts);
            errno = ERANGE;
            return 0;
        }

        leftpos = hershey_val(jhf_line[8]);
        rightpos = hershey_val(jhf_line[9]);
        if (leftpos > rightpos)
        {
            fprintf(stderr, "%s: bogus leftpos %d > rightpos %d\n", __FUNCTION__, leftpos, rightpos);
            errno = ERANGE;
            return 0;
        }

        // skip over the (leftpos,rightpos) pair
        nverts--;
        const int vertcharsoffset = 10;

        hg.glyphnum = (unsigned int)glyphnum;
        hg.width = (unsigned short)(rightpos - leftpos); // FIXME add + 1 ??

        // split out the seperate line paths
        int i;
        int j = 0;
        for (i = 1; i <= nverts; i++)
        {
            // pen up
            if (i == nverts || (jhf_line[vertcharsoffset + i * 2] == ' ' && jhf_line[vertcharsoffset + i * 2 + 1] == 'R'))
            {

                int npathverts = i - j;
                path glyph_path;

                // Copy the vertices with y-invert and offsets to
                // place the glyph origin at its lower left baseline corner with
                // standard Cartesian coordinates.
                int xoffset = -leftpos; // shift left edge to 0
                int yoffset = 16 - 7;   // (height - baseline) FIXME? hardcoded
                int k;
                for (k = 0; k < npathverts; k++)
                {
                    vertex v;
                    v.x = (short)(xoffset + hershey_val(jhf_line[vertcharsoffset + (k + j) * 2]));
                    v.y = (short)(yoffset - hershey_val(jhf_line[vertcharsoffset + (k + j) * 2 + 1]));
                    glyph_path.verts.push_back(v);
                }

                j = i + 1;
                hg.paths.push_back(glyph_path);
            }
        }

        return 1;
    }

    //
    // Load a Hershey font .jhf file into a new struct hershey_font.
    //
    int cls_font::jhf_font_load(const std::string &jhffile)
    {
        std::ifstream file(jhffile);
        if (!file.is_open())
        {
            std::cerr << "Error opening file: " << jhffile << std::endl;
            return 0;
        }

        std::string line;
        int glyph_index = 32;

        while (std::getline(file, line))
        {
            if (!load_glyph(m_font.glyphs[glyph_index++], line))
            {
                std::cerr << "Error loading glyph from line: " << line << std::endl;
                file.close();
                return 0;
            }
        }

        file.close();
        m_loaded = true;
        return 1;
    }

    //
    // Load Hershey fontname (e.g. "rowmans") into a new struct hershey_font.
    // The associated .jhf font file will be loaded from <config-path>/fonts/.
    //
    int cls_font::load(const std::string& config_dir, const std::string &fontname)
    {
        // If loading fails, don't try again for the same font name.
        if (m_font.fontname == fontname && (m_loaded || m_load_failed))
        {
            return m_loaded ? 1 : 0;
        }

        std::string jhffile;

        // Check for environment variable override for font directory
        const char *env_dir = getenv("HERSHEY_FONTS_DIR");
        if (env_dir == nullptr)
        {
            jhffile = config_dir + "/fonts/";
        }
        else
        {
            jhffile = std::string(env_dir);
        }

        jhffile += std::string(fontname) + ".jhf";
        m_loaded = false;
        m_font = font(); // Reset the font structure
        m_font.fontname = fontname;

        int r = jhf_font_load(jhffile);
        m_loaded = r != 0;
        m_load_failed = r == 0;
        return r;
    }

}