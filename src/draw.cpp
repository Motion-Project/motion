/*
 *    This file is part of Motion.
 *
 *    Motion is free software: you can redistribute it and/or modify
 *    it under the terms of the GNU General Public License as published by
 *    the Free Software Foundation, either version 3 of the License, or
 *    (at your option) any later version.
 *
 *    Motion is distributed in the hope that it will be useful,
 *    but WITHOUT ANY WARRANTY; without even the implied warranty of
 *    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *    GNU General Public License for more details.
 *
 *    You should have received a copy of the GNU General Public License
 *    along with Motion.  If not, see <https://www.gnu.org/licenses/>.
 *
 */

#include "motion.hpp"
#include "util.hpp"
#include "i18n.hpp"
#include "logger.hpp"
#include "camera.hpp"
#include "conf.hpp"
#include "alg.hpp"
#include "draw.hpp"

struct draw_char {
    u_char ascii;
    u_char pix[8][7];
};

struct draw_char draw_table[]= {
    {
        ' ',
        {
            {0,0,0,0,0,0,0},
            {0,0,0,0,0,0,0},
            {0,0,0,0,0,0,0},
            {0,0,0,0,0,0,0},
            {0,0,0,0,0,0,0},
            {0,0,0,0,0,0,0},
            {0,0,0,0,0,0,0},
            {0,0,0,0,0,0,0}
        }
    },
    {
        '0',
        {
            {0,0,1,1,1,0,0},
            {0,1,2,2,2,1,0},
            {1,2,1,1,2,2,1},
            {1,2,1,2,1,2,1},
            {1,2,1,2,1,2,1},
            {1,2,2,1,1,2,1},
            {0,1,2,2,2,1,0},
            {0,0,1,1,1,0,0}
        }
    },
    {
        '1',
        {
            {0,0,0,1,0,0,0},
            {0,0,1,2,1,0,0},
            {0,1,2,2,1,0,0},
            {0,0,1,2,1,0,0},
            {0,0,1,2,1,0,0},
            {0,0,1,2,1,0,0},
            {0,1,2,2,2,1,0},
            {0,0,1,1,1,0,0}
        }
    },
    {
        '2',
        {
            {0,0,1,1,1,0,0},
            {0,1,2,2,2,1,0},
            {1,2,1,1,1,2,1},
            {0,1,1,2,2,1,0},
            {0,1,2,1,1,0,0},
            {1,2,1,1,1,1,0},
            {1,2,2,2,2,2,1},
            {0,1,1,1,1,1,0}
        }
    },
    {
        '3',
        {
            {0,0,1,1,1,0,0},
            {0,1,2,2,2,1,0},
            {1,2,1,1,1,2,1},
            {0,1,1,2,2,1,0},
            {0,1,0,1,1,2,1},
            {1,2,1,1,1,2,1},
            {0,1,2,2,2,1,0},
            {0,0,1,1,1,0,0}
        }
    },
    {
        '4',
        {
            {0,0,0,0,1,0,0},
            {0,0,0,1,2,1,0},
            {0,0,1,2,2,1,0},
            {0,1,2,1,2,1,0},
            {1,2,2,2,2,2,1},
            {0,1,1,1,2,1,0},
            {0,0,0,1,2,1,0},
            {0,0,0,0,1,0,0}
        }
    },
    {
        '5',
        {
            {0,1,1,1,1,1,0},
            {1,2,2,2,2,2,1},
            {1,2,1,1,1,1,0},
            {1,2,2,2,2,1,0},
            {0,1,1,1,1,2,0},
            {0,1,1,1,1,2,0},
            {1,2,2,2,2,1,0},
            {0,1,1,1,1,0,0}
        }
    },
    {
        '6',
        {
            {0,0,1,1,1,1,0},
            {0,1,2,2,2,2,1},
            {1,2,1,1,1,1,0},
            {1,2,2,2,2,1,0},
            {1,2,1,1,1,2,1},
            {1,2,1,1,1,2,1},
            {0,1,2,2,2,1,0},
            {0,0,1,1,1,0,0}
        }
    },
    {
        '7',
        {
            {0,1,1,1,1,1,0},
            {1,2,2,2,2,2,1},
            {0,1,1,1,1,2,1},
            {0,0,0,1,2,1,0},
            {0,0,1,2,1,0,0},
            {0,1,2,1,0,0,0},
            {0,1,2,1,0,0,0},
            {0,0,1,0,0,0,0}
        }
    },
    {
        '8',
        {
            {0,0,1,1,1,0,0},
            {0,1,2,2,2,1,0},
            {1,2,1,1,1,2,1},
            {0,1,2,2,2,1,0},
            {1,2,1,1,1,2,1},
            {1,2,1,1,1,2,1},
            {0,1,2,2,2,1,0},
            {0,0,1,1,1,0,0}
        }
    },
    {
        '9',
        {
            {0,0,1,1,1,0,0},
            {0,1,2,2,2,1,0},
            {1,2,1,1,1,2,1},
            {0,1,2,2,2,2,1},
            {0,1,1,1,1,2,1},
            {1,2,1,1,1,2,1},
            {0,1,2,2,2,1,0},
            {0,0,1,1,1,0,0}
        }
    },
    {
        '"',
        {
            {0,0,1,0,1,0,0},
            {0,1,2,1,2,1,0},
            {0,1,2,1,2,1,0},
            {0,0,1,0,1,0,0},
            {0,0,0,0,0,0,0},
            {0,0,0,0,0,0,0},
            {0,0,0,0,0,0,0},
            {0,0,0,0,0,0,0}
        }
    },
    {
        '/',
        {
            {0,0,0,0,1,0,0},
            {0,0,0,1,2,1,0},
            {0,0,0,1,2,1,0},
            {0,0,1,2,1,0,0},
            {0,0,1,2,1,0,0},
            {0,1,2,1,0,0,0},
            {0,1,2,1,0,0,0},
            {0,0,1,0,0,0,0}
        }
    },
    {
        '(',
        {
            {0,0,0,1,0,0,0},
            {0,0,1,2,1,0,0},
            {0,1,2,1,0,0,0},
            {0,1,2,1,0,0,0},
            {0,1,2,1,0,0,0},
            {0,1,2,1,0,0,0},
            {0,0,1,2,1,0,0},
            {0,0,0,1,0,0,0}
        }
    },
    {
        ')',
        {
            {0,0,0,1,0,0,0},
            {0,0,1,2,1,0,0},
            {0,0,0,1,2,1,0},
            {0,0,0,1,2,1,0},
            {0,0,0,1,2,1,0},
            {0,0,0,1,2,1,0},
            {0,0,1,2,1,0,0},
            {0,0,0,1,0,0,0}
        }
    },
    {
        '@',
        {
            {0,0,1,1,1,0,0},
            {0,1,2,2,2,1,0},
            {1,2,1,1,1,2,1},
            {1,2,1,2,2,2,1},
            {1,2,1,2,2,2,1},
            {1,2,1,1,1,1,0},
            {0,1,2,2,2,1,0},
            {0,0,1,1,1,0,0}
        }
    },
    {
        '~',
        {
            {0,0,0,0,0,0,0},
            {0,0,0,0,0,0,0},
            {0,0,1,0,0,0,0},
            {0,1,2,1,0,1,0},
            {1,2,1,2,1,2,1},
            {0,1,0,1,2,1,0},
            {0,0,0,0,1,0,0},
            {0,0,0,0,0,0,0}
        }
    },
    {
        '#',
        {
            {0,0,1,0,1,0,0},
            {0,1,2,1,2,1,0},
            {1,2,2,2,2,2,1},
            {0,1,2,1,2,1,0},
            {0,1,2,1,2,1,0},
            {1,2,2,2,2,2,1},
            {0,1,2,1,2,1,0},
            {0,0,1,0,1,0,0}
        }
    },
    {
        '<',
        {
            {0,0,0,0,0,1,0},
            {0,0,0,1,1,2,1},
            {0,1,1,2,2,1,0},
            {1,2,2,1,1,0,0},
            {0,1,1,2,2,1,0},
            {0,0,0,1,1,2,1},
            {0,0,0,0,0,1,0},
            {0,0,0,0,0,0,0}
        }
    },
    {
        '>',
        {
            {0,1,0,0,0,0,0},
            {1,2,1,1,0,0,0},
            {0,1,2,2,1,1,0},
            {0,0,1,1,2,2,1},
            {0,1,2,2,1,1,0},
            {1,2,1,1,0,0,0},
            {0,1,0,0,0,0,0},
            {0,0,0,0,0,0,0}
        }
    },
    {
        '|',
        {
            {0,0,0,1,0,0,0},
            {0,0,1,2,1,0,0},
            {0,0,1,2,1,0,0},
            {0,0,1,2,1,0,0},
            {0,0,1,2,1,0,0},
            {0,0,1,2,1,0,0},
            {0,0,1,2,1,0,0},
            {0,0,0,1,0,0,0}
        }
    },
    {
        ',',
        {
            {0,0,0,0,0,0,0},
            {0,0,0,0,0,0,0},
            {0,0,0,0,0,0,0},
            {0,0,1,1,0,0,0},
            {0,1,2,2,1,0,0},
            {0,1,2,2,1,0,0},
            {0,1,2,1,0,0,0},
            {0,0,1,0,0,0,0}
        }
    },
    {
        '.',
        {
            {0,0,0,0,0,0,0},
            {0,0,0,0,0,0,0},
            {0,0,0,0,0,0,0},
            {0,0,1,1,0,0,0},
            {0,1,2,2,1,0,0},
            {0,1,2,2,1,0,0},
            {0,0,1,1,0,0,0},
            {0,0,0,0,0,0,0}
        }
    },
    {
        ':',
        {
            {0,0,1,1,0,0,0},
            {0,1,2,2,1,0,0},
            {0,1,2,2,1,0,0},
            {0,0,1,1,0,0,0},
            {0,0,1,1,0,0,0},
            {0,1,2,2,1,0,0},
            {0,1,2,2,1,0,0},
            {0,0,1,1,0,0,0}
        }
    },
    {
        '-',
        {
            {0,0,0,0,0,0,0},
            {0,0,0,0,0,0,0},
            {0,0,1,1,1,0,0},
            {0,1,2,2,2,1,0},
            {0,0,1,1,1,0,0},
            {0,0,0,0,0,0,0},
            {0,0,0,0,0,0,0},
            {0,0,0,0,0,0,0}
        }
    },
    {
        '+',
        {
            {0,0,0,0,0,0,0},
            {0,0,0,1,0,0,0},
            {0,0,1,2,1,0,0},
            {0,1,2,2,2,1,0},
            {0,0,1,2,1,0,0},
            {0,0,0,1,0,0,0},
            {0,0,0,0,0,0,0},
            {0,0,0,0,0,0,0}
        }
    },
    {
        '_',
        {
            {0,0,0,0,0,0,0},
            {0,0,0,0,0,0,0},
            {0,0,0,0,0,0,0},
            {0,0,0,0,0,0,0},
            {0,0,0,0,0,0,0},
            {0,1,1,1,1,1,0},
            {1,2,2,2,2,2,1},
            {0,1,1,1,1,1,0}
        }
    },
    {
        '\'',
        {
            {0,0,0,1,0,0,0},
            {0,0,1,2,1,0,0},
            {0,0,1,2,1,0,0},
            {0,0,0,1,0,0,0},
            {0,0,0,0,0,0,0},
            {0,0,0,0,0,0,0},
            {0,0,0,0,0,0,0},
            {0,0,0,0,0,0,0}
        }
    },
    {
        'a',
        {
            {0,0,0,0,0,0,0},
            {0,0,0,0,0,0,0},
            {0,0,1,1,1,1,0},
            {0,1,2,2,2,2,1},
            {1,2,1,1,1,2,1},
            {1,2,1,1,1,2,1},
            {0,1,2,2,2,2,1},
            {0,0,1,1,1,1,0}
        }
    },
    {
        'b',
        {
            {0,1,0,0,0,0,0},
            {1,2,1,0,0,0,0},
            {1,2,1,1,1,0,0},
            {1,2,2,2,2,1,0},
            {1,2,1,1,1,2,1},
            {1,2,1,1,1,2,1},
            {1,2,2,2,2,1,0},
            {0,1,1,1,1,0,0}
        }
    },
    {
        'c',
        {
            {0,0,0,0,0,0,0},
            {0,0,0,0,0,0,0},
            {0,0,1,1,1,1,0},
            {0,1,2,2,2,2,1},
            {1,2,1,1,1,1,0},
            {1,2,1,1,1,1,0},
            {0,1,2,2,2,2,1},
            {0,0,1,1,1,1,0}
        }
    },
    {
        'd',
        {
            {0,0,0,0,0,1,0},
            {0,0,0,0,1,2,1},
            {0,0,1,1,1,2,1},
            {0,1,2,2,2,2,1},
            {1,2,1,1,1,2,1},
            {1,2,1,1,1,2,1},
            {0,1,2,2,2,2,1},
            {0,0,1,1,1,1,0}
        }
    },
    {
        'e',
        {
            {0,0,0,0,0,0,0},
            {0,0,0,0,0,0,0},
            {0,0,1,1,1,0,0},
            {0,1,2,2,2,1,0},
            {1,2,2,1,1,2,1},
            {1,2,1,2,2,1,0},
            {0,1,2,2,2,2,1},
            {0,0,1,1,1,1,0}
        }
    },
    {
        'f',
        {
            {0,0,0,0,1,1,0},
            {0,0,0,1,2,2,1},
            {0,0,1,2,1,1,0},
            {0,1,2,2,2,1,0},
            {0,0,1,2,1,0,0},
            {0,0,1,2,1,0,0},
            {0,0,1,2,1,0,0},
            {0,0,0,1,0,0,0}
        }
    },
    {
        'g',
        {
            {0,0,0,0,0,0,0},
            {0,0,1,1,1,1,0},
            {0,1,2,2,2,2,1},
            {1,2,1,1,1,2,1},
            {0,1,2,2,2,2,1},
            {0,1,1,1,1,2,1},
            {1,2,2,2,2,1,0},
            {0,1,1,1,1,0,0}
        }
    },
    {
        'h',
        {
            {0,1,0,0,0,0,0},
            {1,2,1,0,0,0,0},
            {1,2,1,1,1,0,0},
            {1,2,1,2,2,1,0},
            {1,2,2,1,1,2,1},
            {1,2,1,0,1,2,1},
            {1,2,1,0,1,2,1},
            {0,1,0,0,0,1,0}
        }
    },
    {
        'i',
        {
            {0,0,0,1,0,0,0},
            {0,0,1,2,1,0,0},
            {0,0,0,1,0,0,0},
            {0,0,1,2,1,0,0},
            {0,0,1,2,1,0,0},
            {0,0,1,2,1,0,0},
            {0,1,2,2,2,1,0},
            {0,0,1,1,1,0,0}
        }
    },
    {
        'j',
        {
            {0,0,0,1,0,0,0},
            {0,0,1,2,1,0,0},
            {0,0,0,1,0,0,0},
            {0,0,1,2,1,0,0},
            {0,0,1,2,1,0,0},
            {0,1,1,2,1,0,0},
            {1,2,2,1,0,0,0},
            {0,1,1,0,0,0,0}
        }
    },
    {
        'k',
        {
            {0,1,0,0,0,0,0},
            {1,2,1,0,0,0,0},
            {1,2,1,0,1,0,0},
            {1,2,1,1,2,1,0},
            {1,2,1,2,1,0,0},
            {1,2,2,1,2,1,0},
            {1,2,1,0,1,2,1},
            {0,1,0,0,0,1,0}
        }
    },
    {
        'l',
        {
            {0,0,1,1,0,0,0},
            {0,1,2,2,1,0,0},
            {0,0,1,2,1,0,0},
            {0,0,1,2,1,0,0},
            {0,0,1,2,1,0,0},
            {0,0,1,2,1,0,0},
            {0,0,0,1,2,1,0},
            {0,0,0,0,1,0,0}
        }
    },
    {
        'm',
        {
            {0,0,0,0,0,0,0},
            {0,0,0,0,0,0,0},
            {0,1,1,0,1,0,0},
            {1,2,2,1,2,1,0},
            {1,2,1,2,1,2,1},
            {1,2,1,2,1,2,1},
            {1,2,1,2,1,2,1},
            {0,1,0,1,0,1,0}
        }
    },
    {
        'n',
        {
            {0,0,0,0,0,0,0},
            {0,0,0,0,0,0,0},
            {0,1,0,1,1,0,0},
            {1,2,1,2,2,1,0},
            {1,2,2,1,1,2,1},
            {1,2,1,0,1,2,1},
            {1,2,1,0,1,2,1},
            {0,1,0,0,0,1,0}
        }
    },
    {
        'o',
        {
            {0,0,0,0,0,0,0},
            {0,0,0,0,0,0,0},
            {0,0,1,1,1,0,0},
            {0,1,2,2,2,1,0},
            {1,2,1,1,1,2,1},
            {1,2,1,1,1,2,1},
            {0,1,2,2,2,1,0},
            {0,0,1,1,1,0,0}
        }
    },
    {
        'p',
        {
            {0,0,0,0,0,0,0},
            {0,0,0,0,0,0,0},
            {0,1,1,1,1,0,0},
            {1,2,2,2,2,1,0},
            {1,2,1,1,1,2,1},
            {1,2,2,2,2,1,0},
            {1,2,1,1,1,0,0},
            {1,2,1,0,0,0,0},
        }
    },
    {
        'q',
        {
            {0,0,0,0,0,0,0},
            {0,0,0,0,0,0,0},
            {0,0,1,1,1,1,0},
            {0,1,2,2,2,2,1},
            {1,2,1,1,1,2,1},
            {0,1,2,2,2,2,1},
            {0,0,1,1,1,2,1},
            {0,0,0,0,1,2,1}
        }
    },
    {
        'r',
        {
            {0,0,0,0,0,0,0},
            {0,0,0,0,0,0,0},
            {0,1,0,1,1,0,0},
            {1,2,1,2,2,1,0},
            {1,2,2,1,1,2,1},
            {1,2,1,0,0,1,0},
            {1,2,1,0,0,0,0},
            {0,1,0,0,0,0,0}
        }
    },
    {
        's',
        {
            {0,0,0,0,0,0,0},
            {0,0,0,0,0,0,0},
            {0,0,1,1,1,1,0},
            {0,1,2,2,2,2,1},
            {1,2,2,2,1,1,0},
            {0,1,1,2,2,2,1},
            {1,2,2,2,2,1,0},
            {0,1,1,1,1,0,0}
        }
    },
    {
        't',
        {
            {0,0,0,1,0,0,0},
            {0,0,1,2,1,0,0},
            {0,0,1,2,1,0,0},
            {0,1,2,2,2,1,0},
            {0,0,1,2,1,0,0},
            {0,0,1,2,1,0,0},
            {0,0,0,1,2,1,0},
            {0,0,0,0,1,0,0}
        }
    },
    {
        'u',
        {
            {0,0,0,0,0,0,0},
            {0,0,0,0,0,0,0},
            {0,1,0,0,0,1,0},
            {1,2,1,0,1,2,1},
            {1,2,1,0,1,2,1},
            {1,2,1,1,2,2,1},
            {0,1,2,2,1,2,1},
            {0,0,1,1,0,1,0}
        }
    },
    {
        'v',
        {
            {0,0,0,0,0,0,0},
            {0,0,0,0,0,0,0},
            {0,1,0,0,0,1,0},
            {1,2,1,0,1,2,1},
            {1,2,1,0,1,2,1},
            {0,1,2,1,2,1,0},
            {0,0,1,2,1,0,0},
            {0,0,0,1,0,0,0}
        }
    },
    {
        'w',
        {
            {0,0,0,0,0,0,0},
            {0,0,0,0,0,0,0},
            {0,1,0,0,0,1,0},
            {1,2,1,0,1,2,1},
            {1,2,1,1,1,2,1},
            {1,2,1,2,1,2,1},
            {0,1,2,1,2,1,0},
            {0,0,1,0,1,0,0}
        }
    },
    {
        'x',
        {
            {0,0,0,0,0,0,0},
            {0,0,0,0,0,0,0},
            {0,1,0,0,1,0,0},
            {1,2,1,1,2,1,0},
            {0,1,2,2,1,0,0},
            {0,1,2,2,1,0,0},
            {1,2,1,1,2,1,0},
            {0,1,0,0,1,0,0}
        }
    },
    {
        'y',
        {
            {0,0,0,0,0,0,0},
            {0,0,0,0,0,0,0},
            {0,1,0,0,0,1,0},
            {1,2,1,0,1,2,1},
            {0,1,2,1,2,1,0},
            {0,0,1,2,1,0,0},
            {0,1,2,1,0,0,0},
            {1,2,1,0,0,0,0}
        }
    },
    {
        'z',
        {
            {0,0,0,0,0,0,0},
            {0,0,0,0,0,0,0},
            {0,1,1,1,1,0,0},
            {1,2,2,2,2,1,0},
            {0,1,1,2,1,0,0},
            {0,1,2,1,1,0,0},
            {1,2,2,2,2,1,0},
            {0,1,1,1,1,0,0}
        }
    },
    {
        'A',
        {
            {0,0,1,1,1,0,0},
            {0,1,2,2,2,1,0},
            {1,2,1,1,1,2,1},
            {1,2,1,1,1,2,1},
            {1,2,2,2,2,2,1},
            {1,2,1,1,1,2,1},
            {1,2,1,0,1,2,1},
            {0,1,0,0,0,1,0}
        }
    },
    {
        'B',
        {
            {0,1,1,1,1,0,0},
            {1,2,2,2,2,1,0},
            {1,2,1,1,1,2,1},
            {1,2,2,2,2,1,0},
            {1,2,1,1,1,2,1},
            {1,2,1,1,1,2,1},
            {1,2,2,2,2,1,0},
            {0,1,1,1,1,0,0}
        }
    },
    {
        'C',
        {
            {0,0,1,1,1,0,0},
            {0,1,2,2,2,1,0},
            {1,2,1,1,1,2,1},
            {1,2,1,0,0,1,0},
            {1,2,1,0,0,1,0},
            {1,2,1,1,1,2,1},
            {0,1,2,2,2,1,0},
            {0,0,1,1,1,0,0}
        }
    },
    {
        'D',
        {
            {0,1,1,1,1,0,0},
            {1,2,2,2,2,1,0},
            {1,2,1,1,1,2,1},
            {1,2,1,0,1,2,1},
            {1,2,1,0,1,2,1},
            {1,2,1,1,1,2,1},
            {1,2,2,2,2,1,0},
            {0,1,1,1,1,0,0}
        }
    },
    {
        'E',
        {
            {0,1,1,1,1,1,0},
            {1,2,2,2,2,2,1},
            {1,2,1,1,1,1,0},
            {1,2,2,2,2,1,0},
            {1,2,1,1,1,0,0},
            {1,2,1,1,1,1,0},
            {1,2,2,2,2,2,1},
            {0,1,1,1,1,1,0}
        }
    },
    {
        'F',
        {
            {0,1,1,1,1,1,0},
            {1,2,2,2,2,2,1},
            {1,2,1,1,1,1,0},
            {1,2,2,2,2,1,0},
            {1,2,1,1,1,0,0},
            {1,2,1,0,0,0,0},
            {1,2,1,0,0,0,0},
            {0,1,0,0,0,0,0}
        }
    },
    {
        'G',
        {
            {0,0,1,1,1,0,0},
            {0,1,2,2,2,1,0},
            {1,2,1,1,1,2,1},
            {1,2,1,1,1,1,0},
            {1,2,1,2,2,2,1},
            {1,2,1,1,1,2,1},
            {0,1,2,2,2,1,0},
            {0,0,1,1,1,0,0}
        }
    },
    {
        'H',
        {
            {0,1,0,0,0,1,0},
            {1,2,1,0,1,2,1},
            {1,2,1,1,1,2,1},
            {1,2,2,2,2,2,1},
            {1,2,1,1,1,2,1},
            {1,2,1,0,1,2,1},
            {1,2,1,0,1,2,1},
            {0,1,0,0,0,1,0}
        }
    },
    {
        'I',
        {
            {0,0,1,1,1,0,0},
            {0,1,2,2,2,1,0},
            {0,0,1,2,1,0,0},
            {0,0,1,2,1,0,0},
            {0,0,1,2,1,0,0},
            {0,0,1,2,1,0,0},
            {0,1,2,2,2,1,0},
            {0,0,1,1,1,0,0}
        }
    },
    {
        'J',
        {
            {0,0,1,1,1,1,0},
            {0,1,2,2,2,2,1},
            {0,0,1,1,1,2,1},
            {0,0,0,0,1,2,1},
            {0,1,0,0,1,2,1},
            {1,2,1,1,1,2,1},
            {0,1,2,2,2,1,0},
            {0,0,1,1,1,0,0}
        }
    },
    {
        'K',
        {
            {0,1,0,0,0,1,0},
            {1,2,1,0,1,2,1},
            {1,2,1,1,2,1,0},
            {1,2,1,2,1,0,0},
            {1,2,2,2,1,0,0},
            {1,2,1,1,2,1,0},
            {1,2,1,0,1,2,1},
            {0,1,0,0,0,1,0}
        }
    },
    {
        'L',
        {
            {0,1,0,0,0,0,0},
            {1,2,1,0,0,0,0},
            {1,2,1,0,0,0,0},
            {1,2,1,0,0,0,0},
            {1,2,1,0,0,0,0},
            {1,2,1,1,1,0,0},
            {1,2,2,2,2,1,0},
            {0,1,1,1,1,0,0}
        }
    },
    {
        'M',
        {
            {0,1,1,0,1,1,0},
            {1,2,2,1,2,2,1},
            {1,2,1,2,1,2,1},
            {1,2,1,1,1,2,},
            {1,2,1,0,1,2,1},
            {1,2,1,0,1,2,1},
            {1,2,1,0,1,2,1},
            {0,1,0,0,0,1,0}
        }
    },
    {
        'N',
        {
            {0,1,0,0,0,1,0},
            {1,2,1,0,1,2,1},
            {1,2,2,1,1,2,1},
            {1,2,1,2,1,2,1},
            {1,2,1,1,2,2,1},
            {1,2,1,0,1,2,1},
            {1,2,1,0,1,2,1},
            {0,1,0,0,0,1,0}
        }
    },
    {
        'O',
        {
            {0,0,1,1,1,0,0},
            {0,1,2,2,2,1,0},
            {1,2,1,1,1,2,1},
            {1,2,1,0,1,2,1},
            {1,2,1,0,1,2,1},
            {1,2,1,1,1,2,1},
            {0,1,2,2,2,1,0},
            {0,0,1,1,1,0,0}
        }
    },
    {
        'P',
        {
            {0,1,1,1,1,0,0},
            {1,2,2,2,2,1,0},
            {1,2,1,1,1,2,1},
            {1,2,2,2,2,1,0},
            {1,2,1,1,1,0,0},
            {1,2,1,0,0,0,0},
            {1,2,1,0,0,0,0},
            {0,1,0,0,0,0,0}
        }
    },
    {
        'Q',
        {
            {0,0,1,1,1,0,0},
            {0,1,2,2,2,1,0},
            {1,2,1,1,1,2,1},
            {1,2,1,1,1,2,1},
            {1,2,1,2,1,2,1},
            {1,2,1,1,2,1,0},
            {0,1,2,2,1,2,1},
            {0,0,1,1,0,1,0}
        }
    },
    {
        'R',
        {
            {0,1,1,1,1,0,0},
            {1,2,2,2,2,1,0},
            {1,2,1,1,1,2,1},
            {1,2,2,2,2,1,0},
            {1,2,1,2,1,0,0},
            {1,2,1,1,2,1,0},
            {1,2,1,0,1,2,1},
            {0,1,0,0,0,1,0}
        }
    },
    {
        'S',
        {
            {0,0,1,1,1,1,0},
            {0,1,2,2,2,2,1},
            {1,2,1,1,1,1,0},
            {0,1,2,2,2,1,0},
            {0,0,1,1,1,2,1},
            {0,1,1,1,1,2,1},
            {1,2,2,2,2,1,0},
            {0,1,1,1,1,0,0}
        }
    },
    {
        'T',
        {
            {0,1,1,1,1,1,0},
            {1,2,2,2,2,2,1},
            {0,1,1,2,1,1,0},
            {0,0,1,2,1,0,0},
            {0,0,1,2,1,0,0},
            {0,0,1,2,1,0,0},
            {0,0,1,2,1,0,0},
            {0,0,0,1,0,0,0}
        }
    },
    {
        'U',
        {
            {0,1,0,0,0,1,0},
            {1,2,1,0,1,2,1},
            {1,2,1,0,1,2,1},
            {1,2,1,0,1,2,1},
            {1,2,1,0,1,2,1},
            {1,2,1,1,1,2,1},
            {0,1,2,2,2,2,1},
            {0,0,1,1,1,1,0}
        }
    },
    {
        'V',
        {
            {0,1,0,0,0,1,0},
            {1,2,1,0,1,2,1},
            {1,2,1,0,1,2,1},
            {1,2,1,0,1,2,1},
            {1,2,1,0,1,2,1},
            {0,1,2,1,2,1,0},
            {0,0,1,2,1,0,0},
            {0,0,0,1,0,0,0}
        }
    },
    {
        'W',
        {
            {0,1,0,0,0,1,0},
            {1,2,1,0,1,2,1},
            {1,2,1,0,1,2,1},
            {1,2,1,1,1,2,1},
            {1,2,1,2,1,2,1},
            {1,2,1,2,1,2,1},
            {0,1,2,1,2,1,0},
            {0,0,1,0,1,0,0}
        }
    },
    {
        'X',
        {
            {0,1,0,0,0,1,0},
            {1,2,1,0,1,2,1},
            {0,1,2,1,2,1,0},
            {0,0,1,2,1,0,0},
            {0,0,1,2,1,0,0},
            {0,1,2,1,2,1,0},
            {1,2,1,0,1,2,1},
            {0,1,0,0,0,1,0}
        }
    },
    {
        'Y',
        {
            {0,1,0,0,0,1,0},
            {1,2,1,0,1,2,1},
            {0,1,2,1,2,1,0},
            {0,0,1,2,1,0,0},
            {0,0,1,2,1,0,0},
            {0,0,1,2,1,0,0},
            {0,0,1,2,1,0,0},
            {0,0,0,1,0,0,0}
        }
    },
    {
        'Z',
        {
            {0,1,1,1,1,1,0},
            {1,2,2,2,2,2,1},
            {0,1,1,1,2,1,0},
            {0,0,1,2,1,0,0},
            {0,1,2,1,0,0,0},
            {1,2,1,1,1,1,0},
            {1,2,2,2,2,2,1},
            {0,1,1,1,1,1,0}
        }
    }
};

void cls_draw::text_legacy_ln(u_char *image, int stx,  int sty
        , const char *text, int len, int factor)
{

    int x, y;
    int pos, line_offset, next_char_offs;
    u_char *image_ptr, *char_ptr;

    if (stx > cam->imgs.width / 2) {
        stx -= len * (6 * factor);
    }

    if (stx + len * 6 * factor >= cam->imgs.width) {
        len = (cam->imgs.width-stx-1)/(6*factor);
    }

    if ((stx < 1) || (sty < 1) || (len < 1)) {
        return;
    }

    line_offset = cam->imgs.width - (7 * factor);
    next_char_offs = (cam->imgs.width * 8 * factor) - (6 * factor);

    image_ptr = image + stx + (sty * cam->imgs.width);

    for (pos = 0; pos < len; pos++) {
        int pos_check = (int)text[pos];

        if ((pos_check <0) || (pos_check >126)) {
            pos_check = 45; /* Use a - for non ascii characters*/
        }
        char_ptr = char_arr_ptr[pos_check];

        for (y = 0; y < 8 * factor; y++) {
            for (x = 0; x < 7 * factor; x++) {

                if (pos_check < 0) {
                    image_ptr++;
                    continue;
                }

                char_ptr = char_arr_ptr[pos_check] + y/factor*7 + x/factor;

                switch(*char_ptr) {
                case 1:
                    *image_ptr = 0;
                     break;
                case 2:
                    *image_ptr = 255;
                    break;
                default:
                    break;
                }

                image_ptr++;
            }
            image_ptr += line_offset;
        }
        image_ptr -= next_char_offs;
    }

}

void cls_draw::text_legacy(u_char *image, int stx, int sty
    , const char *text, int factor)
{
    int num_nl = 0;
    const char *end, *begin;
    int line_space, txtlen;

    /* Count the number of newlines in "text" so we scroll it up the image. */
    begin = end = text;
    txtlen = 0;
    while ((end = strstr(end, NEWLINE))) {
        if ((end - begin)>txtlen) {
            txtlen = (int)(end - begin);
        }
        num_nl++;
        end += sizeof(NEWLINE)-1;
        begin = end;
    }
    if (txtlen == 0) {
        txtlen = (int)strlen(text);
    }

    /* Adjust the factor if it is out of bounds
     * txtlen at this point is the approx length of longest line
    */
    if ((txtlen * 7 * factor) > cam->imgs.width) {
        factor = (cam->imgs.width / (txtlen * 7));
        if (factor <= 0) {
            factor = 1;
        }
    }

    if (((num_nl+1) * 8 * factor) > cam->imgs.height) {
        factor = (cam->imgs.height / ((num_nl+1) * 8));
        if (factor <= 0) {
            factor = 1;
        }
    }

    line_space = factor * 9;

    sty -= line_space * num_nl;

    begin = end = text;

    while ((end = strstr(end, NEWLINE))) {
        int len = (int)(end-begin);

        text_legacy_ln(image, stx, sty, begin, len, factor);
        end += sizeof(NEWLINE)-1;
        begin = end;
        sty += line_space;
    }

    text_legacy_ln(image, stx, sty, begin, (int)strlen(begin), factor);

}

void cls_draw::init_chars(void)
{
    unsigned int i;
    size_t draw_table_size;

    draw_table_size = sizeof(draw_table) / sizeof(struct draw_char);

    /* First init all char ptrs to a space character. */
    for (i = 0; i < ASCII_MAX; i++) {
        char_arr_ptr[i] = &draw_table[0].pix[0][0];
    }

    /* Build char_arr_ptr table to point to each available ascii. */
    for (i = 0; i < draw_table_size; i++) {
        char_arr_ptr[(int)draw_table[i].ascii] = &draw_table[i].pix[0][0];
    }

}

void cls_draw::init_params()
{
    int indx;
    ctx_params_item *itm;

    uselegacy = false;
    scalefactor = 1;
    ft_usersz = false;
    ft_size = 10;
    ft_font ="";
    showdiffs = false;
    ft_color = "white";
    ft_flags = "";

    avfilter_graph_free(&fltgr);
    av_frame_free(&frm_in);

    for (indx=0;indx<params->params_array.size();indx++) {
        itm = &params->params_array[indx];
        if (itm->param_name == "method") {
            if (itm->param_value == "legacy") {
                uselegacy = true;
            } else {
                uselegacy = false;
            }
        }
        if (itm->param_name == "scale") {
            scalefactor = mtoi(itm->param_value);
        }
        if (itm->param_name == "changes") {
            showdiffs = mtob(itm->param_value);
        }
        if (itm->param_name == "ft_font") {
            ft_font = itm->param_value;
        }
        if (itm->param_name == "ft_size") {
            ft_size = mtoi(itm->param_value);
            if (itm->param_source == PARM_SRC_USER) {
                ft_usersz = true;
            }
        }
        if (itm->param_name == "ft_color") {
            ft_color = itm->param_value;
        }
        if (itm->param_name == "ft_flags") {
            ft_flags = itm->param_value;
        }
    }

    #if (MYFFVER <= 61007)
        if (uselegacy == false) {
            MOTION_LOG(NTC, NO_ERRNO
                ,_("Freetype text overlay is not supported with older FFMpeg versions"));
            MOTION_LOG(NTC, NO_ERRNO
                ,_("FFMpeg version 8.0+ with "
                "--enable-libfreetype --enable-libharfbuzz "
                "--enable-libfontconfig required"));
        }
        uselegacy = true;
    #endif

    if (uselegacy == true) {
        if ((scalefactor < 1) || (scalefactor >10)) {
            scalefactor = 1;
            MOTION_LOG(WRN, NO_ERRNO
                ,_("Invalid text scale.  Adjusted to %d"), scalefactor);
        }
        if ((scalefactor * 10 * 2) > (cam->imgs.width / 4)) {
            scalefactor = (cam->imgs.width / (4 * 10 * 2));
            if (scalefactor <= 0) {
                scalefactor = 1;
            }
            MOTION_LOG(WRN, NO_ERRNO
                ,_("Invalid text scale.  Adjusted to %d"), scalefactor);
        }

        if ((scalefactor * 10 * 2) > (cam->imgs.height / 4)) {
            scalefactor = (cam->imgs.height / (4 * 10 * 2));
            if (scalefactor <= 0) {
                scalefactor = 1;
            }
            MOTION_LOG(WRN, NO_ERRNO
                ,_("Invalid text scale.  Adjusted to %d"), scalefactor);
        }
    } else {
        scalefactor = 1;
        if ((ft_size < 5) || (ft_size >100)) {
            MOTION_LOG(WRN, NO_ERRNO
                ,_("Invalid font size %d.  Adjusted to 10"), ft_size);
            ft_size = 10;
        }
        init_ft();
    }
}

void cls_draw::init_ft()
{
    size_t pos;
    int retcd;
    char errstr[128];
    std::string atxt, dtxt_tl, dtxt_tr, dtxt_bl, dtxt_br;

    ft_textl = cam->cfg->text_left;
    pos= ft_textl.find(R"(\n)");
    while (pos != std::string::npos) {
        ft_textl.replace(pos, 2, "\n");
        pos = ft_textl.find(R"(\n)",pos+1);
    }

    ft_textr = cam->cfg->text_right;
    pos= ft_textr.find(R"(\n)");
    while (pos != std::string::npos) {
        ft_textr.replace(pos, 2, "\n");
        pos = ft_textr.find(R"(\n)",pos+1);
    }

    atxt = "video_size=";
    atxt += std::to_string(cam->imgs.width)+"x";
    atxt += std::to_string(cam->imgs.height);
    atxt += ":pix_fmt=0:time_base=1/1:pixel_aspect=1/1";

    dtxt_tl = "text='%{metadata:mytext_tl}'";
    dtxt_tl +=":fontfile='" +ft_font +"'";
    dtxt_tl +=":fontsize="+std::to_string(ft_size);
    dtxt_tl +=":fontcolor="+ft_color;
    dtxt_tl +=":x=10";
    dtxt_tl +=":y=10";

    dtxt_tr = "text='%{metadata:mytext_tr}'";
    dtxt_tr +=":fontfile='" +ft_font +"'";
    dtxt_tr +=":fontsize="+std::to_string(ft_size);
    dtxt_tr +=":fontcolor="+ft_color;
    dtxt_tr +=":x=(w-text_w-10)";
    dtxt_tr +=":y=10";

    dtxt_bl = "text='%{metadata:mytext_bl}'";
    dtxt_bl +=":fontfile='" +ft_font +"'";
    dtxt_bl +=":fontsize="+std::to_string(ft_size);
    dtxt_bl +=":fontcolor="+ft_color;
    dtxt_bl +=":x=10";
    dtxt_bl +=":y=(h-text_h-10)";

    dtxt_br = "text='%{metadata:mytext_br}'";
    dtxt_br +=":fontfile='" +ft_font +"'";
    dtxt_br +=":fontsize="+std::to_string(ft_size);
    dtxt_br +=":fontcolor="+ft_color;
    dtxt_br +=":x=(w-text_w-10)";
    dtxt_br +=":y=(h-text_h-10)";

    fltgr = avfilter_graph_alloc();
    if (fltgr == NULL) {
        MOTION_LOG(ERR, NO_ERRNO,_("filter graph null"));
        uselegacy = true;
        return;
    }

    fltsrc  = avfilter_get_by_name("buffer");
    retcd = avfilter_graph_create_filter(
        &fltctx_src, fltsrc, "in", atxt.c_str(), nullptr, fltgr);
    if (retcd < 0) {
        av_strerror(retcd, errstr, sizeof(errstr));
        MOTION_LOG(ERR, NO_ERRNO
            ,_("create filter buffer failed: %s"), errstr);
        uselegacy = true;
        avfilter_graph_free(&fltgr);
        return;
    }

    fltdraw_tl = avfilter_get_by_name("drawtext");
    if (fltdraw_tl == nullptr) {
        MOTION_LOG(ERR, NO_ERRNO
            ,_("fltdraw_tl allocation failed"));
        uselegacy = true;
        avfilter_graph_free(&fltgr);
        return;
    }

    retcd = avfilter_graph_create_filter(
        &fltctx_draw_tl, fltdraw_tl, "textflt_tl"
        ,dtxt_tl.c_str(), nullptr, fltgr);
    if (retcd < 0) {
        av_strerror(retcd, errstr, sizeof(errstr));
        MOTION_LOG(ERR, NO_ERRNO
            ,_("create filter fltdraw tl failed %s"),errstr);
        abort();
        uselegacy = true;
        avfilter_graph_free(&fltgr);
        return;
    }

    fltdraw_tr = avfilter_get_by_name("drawtext");
    if (fltdraw_tr == nullptr) {
        MOTION_LOG(ERR, NO_ERRNO
            ,_("fltdraw_tr allocation failed"));
        uselegacy = true;
        avfilter_graph_free(&fltgr);
        return;
    }

    retcd = avfilter_graph_create_filter(
        &fltctx_draw_tr, fltdraw_tr, "textflt_tr"
        ,dtxt_tr.c_str(), nullptr, fltgr);
    if (retcd < 0) {
        av_strerror(retcd, errstr, sizeof(errstr));
        MOTION_LOG(ERR, NO_ERRNO
            ,_("create filter fltdraw tr failed: %s"), errstr);
        uselegacy = true;
        avfilter_graph_free(&fltgr);
        return;
    }

    fltdraw_bl = avfilter_get_by_name("drawtext");
    if (fltdraw_bl == nullptr) {
        MOTION_LOG(ERR, NO_ERRNO
            ,_("fltdraw_bl allocation failed"));
        uselegacy = true;
        avfilter_graph_free(&fltgr);
        return;
    }

    retcd = avfilter_graph_create_filter(
        &fltctx_draw_bl, fltdraw_bl, "textflt_bl"
        ,dtxt_bl.c_str(), nullptr, fltgr);
    if (retcd < 0) {
        av_strerror(retcd, errstr, sizeof(errstr));
        MOTION_LOG(ERR, NO_ERRNO
            ,_("create filter fltdraw bl failed: %s"), errstr);
        uselegacy = true;
        avfilter_graph_free(&fltgr);
        return;
    }

    fltdraw_br = avfilter_get_by_name("drawtext");
    if (fltdraw_br == nullptr) {
        MOTION_LOG(ERR, NO_ERRNO
            ,_("fltdraw_br allocation failed"));
        uselegacy = true;
        avfilter_graph_free(&fltgr);
        return;
    }

    retcd = avfilter_graph_create_filter(
        &fltctx_draw_br, fltdraw_br, "textflt_br"
        ,dtxt_br.c_str(), nullptr, fltgr);
    if (retcd < 0) {
        av_strerror(retcd, errstr, sizeof(errstr));
        MOTION_LOG(ERR, NO_ERRNO
            ,_("create filter fltdraw br failed: %s"), errstr);
        uselegacy = true;
        avfilter_graph_free(&fltgr);
        return;
    }

    fltsink = avfilter_get_by_name("buffersink");
    if (fltsink == nullptr) {
        MOTION_LOG(ERR, NO_ERRNO
            ,_("fltsink allocation failed"));
        uselegacy = true;
        avfilter_graph_free(&fltgr);
        return;
    }

    retcd = avfilter_graph_create_filter(
        &fltctx_sink, fltsink, "out"
        ,nullptr, nullptr, fltgr);
    if (retcd < 0) {
        av_strerror(retcd, errstr, sizeof(errstr));
        MOTION_LOG(ERR, NO_ERRNO
            ,_("create filter fltsink failed: %s"), errstr);
        uselegacy = true;
        avfilter_graph_free(&fltgr);
        return;
    }

    retcd = avfilter_link(fltctx_src, 0, fltctx_draw_tl, 0);
    if (retcd >= 0) {retcd = avfilter_link(fltctx_draw_tl, 0, fltctx_draw_tr, 0);}
    if (retcd >= 0) {retcd = avfilter_link(fltctx_draw_tr, 0, fltctx_draw_bl, 0);}
    if (retcd >= 0) {retcd = avfilter_link(fltctx_draw_bl, 0, fltctx_draw_br, 0);}
    if (retcd >= 0) {retcd = avfilter_link(fltctx_draw_br, 0, fltctx_sink, 0);}
    if (retcd < 0) {
        av_strerror(retcd, errstr, sizeof(errstr));
        MOTION_LOG(ERR, NO_ERRNO
            ,_("create filter link fltsrc->draw->fltsink failed: %s"), errstr);
        uselegacy = true;
        avfilter_graph_free(&fltgr);
        return;
    }

    retcd = avfilter_graph_config(fltgr, nullptr);
    if (retcd < 0) {
        av_strerror(retcd, errstr, sizeof(errstr));
        MOTION_LOG(ERR, NO_ERRNO,_("graph config failed: %s"), errstr);
        uselegacy = true;
        avfilter_graph_free(&fltgr);
        return;
    }

    frm_in = av_frame_alloc();
    if (frm_in == NULL) {
        MOTION_LOG(ERR, NO_ERRNO,_("frm_in failed"));
        uselegacy = true;
        avfilter_graph_free(&fltgr);
        return;
    }

    frm_in->format = AV_PIX_FMT_YUV420P;
    frm_in->width  = cam->imgs.width;
    frm_in->height = cam->imgs.height;

    retcd = av_frame_get_buffer(frm_in, 32);
    if (retcd < 0) {
        av_strerror(retcd, errstr, sizeof(errstr));
        MOTION_LOG(ERR, NO_ERRNO,_("frm_in buffer failed: %s"), errstr);
        uselegacy = true;
        avfilter_graph_free(&fltgr);
        av_frame_free(&frm_in);
        return;
    }

}

void cls_draw::location(ctx_coord *cent, ctx_images *imgs
    , int width, u_char *new_var)
{
    u_char *out = imgs->image_motion.image_norm;
    int x, y, centy;
    int width_miny, width_maxy;
    int width_miny_x, width_maxy_x;
    int width_minx_y, width_maxx_y;

    out = imgs->image_motion.image_norm;

    width_miny = width * cent->miny;
    width_maxy = width * cent->maxy;
    for (x = cent->minx; x <= cent->maxx; x++) {
        width_miny_x = x + width_miny;
        width_maxy_x = x + width_maxy;
        out[width_miny_x] =~out[width_miny_x];
        out[width_maxy_x] =~out[width_maxy_x];
    }

    for (y = cent->miny; y <= cent->maxy; y++) {
        width_minx_y = cent->minx + y * width;
        width_maxx_y = cent->maxx + y * width;

        out[width_minx_y] =~out[width_minx_y];
        out[width_maxx_y] =~out[width_maxx_y];
    }

    if (cam->cfg->locate_motion_style == "box") {
        width_miny = width * cent->miny;
        width_maxy = width * cent->maxy;

        for (x = cent->minx; x <= cent->maxx; x++) {
            width_miny_x = x + width_miny;
            width_maxy_x = x + width_maxy;
            new_var[width_miny_x] =~new_var[width_miny_x];
            new_var[width_maxy_x] =~new_var[width_maxy_x];
        }

        for (y = cent->miny; y <= cent->maxy; y++) {
            width_minx_y = cent->minx + y * width;
            width_maxx_y = cent->maxx + y * width;
            new_var[width_minx_y] =~new_var[width_minx_y];
            new_var[width_maxx_y] =~new_var[width_maxx_y];
        }
    } else if (cam->cfg->locate_motion_style == "cross") {
        centy = cent->y * width;
        for (x = cent->x - 10;  x <= cent->x + 10; x++) {
            new_var[centy + x] =~new_var[centy + x];
            out[centy + x] =~out[centy + x];
        }
        for (y = cent->y - 10; y <= cent->y + 10; y++) {
            new_var[cent->x + y * width] =~new_var[cent->x + y * width];
            out[cent->x + y * width] =~out[cent->x + y * width];
        }
    }
}

void cls_draw::red_location(ctx_coord *cent
        , ctx_images *imgs, int width, u_char *new_var)
{
    u_char *out = imgs->image_motion.image_norm;
    u_char *new_u, *new_v;
    int x, y, v, cwidth, cblock;
    int width_miny, width_maxy;
    int width_miny_x, width_maxy_x;
    int width_minx_y, width_maxx_y;
    int cwidth_miny, cwidth_maxy;
    int cwidth_miny_x, cwidth_maxy_x;
    int cwidth_minx_y,cwidth_maxx_y;

    cwidth = width / 2;
    cblock = imgs->motionsize / 4;
    x = imgs->motionsize;
    v = x + cblock;
    out = imgs->image_motion.image_norm;
    new_u = new_var + x;
    new_v = new_var + v;

    width_miny = width * cent->miny;
    width_maxy = width * cent->maxy;

    for (x = cent->minx; x <= cent->maxx; x++) {
        width_miny_x = x + width_miny;
        width_maxy_x = x + width_maxy;
        out[width_miny_x] =~out[width_miny_x];
        out[width_maxy_x] =~out[width_maxy_x];
    }

    for (y = cent->miny; y <= cent->maxy; y++) {
        width_minx_y = cent->minx + y * width;
        width_maxx_y = cent->maxx + y * width;

        out[width_minx_y] =~out[width_minx_y];
        out[width_maxx_y] =~out[width_maxx_y];
    }

    if (cam->cfg->locate_motion_style == "redbox") {
        width_miny = width * cent->miny;
        width_maxy = width * cent->maxy;
        cwidth_miny = cwidth * (cent->miny / 2);
        cwidth_maxy = cwidth * (cent->maxy / 2);

        for (x = cent->minx + 2; x <= cent->maxx - 2; x += 2) {
            width_miny_x = x + width_miny;
            width_maxy_x = x + width_maxy;
            cwidth_miny_x = x / 2 + cwidth_miny;
            cwidth_maxy_x = x / 2 + cwidth_maxy;

            new_u[cwidth_miny_x] = 128;
            new_u[cwidth_maxy_x] = 128;
            new_v[cwidth_miny_x] = 255;
            new_v[cwidth_maxy_x] = 255;

            new_var[width_miny_x] = 128;
            new_var[width_maxy_x] = 128;

            new_var[width_miny_x + 1] = 128;
            new_var[width_maxy_x + 1] = 128;

            new_var[width_miny_x + width] = 128;
            new_var[width_maxy_x + width] = 128;

            new_var[width_miny_x + 1 + width] = 128;
            new_var[width_maxy_x + 1 + width] = 128;
        }

        for (y = cent->miny; y <= cent->maxy; y += 2) {
            width_minx_y = cent->minx + y * width;
            width_maxx_y = cent->maxx + y * width;
            cwidth_minx_y = (cent->minx / 2) + (y / 2) * cwidth;
            cwidth_maxx_y = (cent->maxx / 2) + (y / 2) * cwidth;

            new_u[cwidth_minx_y] = 128;
            new_u[cwidth_maxx_y] = 128;
            new_v[cwidth_minx_y] = 255;
            new_v[cwidth_maxx_y] = 255;

            new_var[width_minx_y] = 128;
            new_var[width_maxx_y] = 128;

            new_var[width_minx_y + width] = 128;
            new_var[width_maxx_y + width] = 128;

            new_var[width_minx_y + 1] = 128;
            new_var[width_maxx_y + 1] = 128;

            new_var[width_minx_y + width + 1] = 128;
            new_var[width_maxx_y + width + 1] = 128;
        }
    } else if (cam->cfg->locate_motion_style == "redcross") {
        cwidth_maxy = cwidth * (cent->y / 2);

        for (x = cent->x - 10; x <= cent->x + 10; x += 2) {
            cwidth_maxy_x = x / 2 + cwidth_maxy;
            new_u[cwidth_maxy_x] = 128;
            new_v[cwidth_maxy_x] = 255;
        }

        for (y = cent->y - 10; y <= cent->y + 10; y += 2) {
            cwidth_minx_y = (cent->x / 2) + (y / 2) * cwidth;
            new_u[cwidth_minx_y] = 128;
            new_v[cwidth_minx_y] = 255;
        }
    }
}

void cls_draw::locate()
{
    ctx_images *imgs;
    ctx_coord *p_loc;
    u_char *image_norm;

    if (cam->cfg->locate_motion_mode == "preview") {
        imgs = &cam->imgs;
        p_loc = &cam->imgs.image_preview.location;
        image_norm = cam->imgs.image_preview.image_norm;
    } else if (cam->cfg->locate_motion_mode == "on") {
        imgs = &cam->imgs;
        p_loc = &cam->current_image->location;
        image_norm = cam->current_image->image_norm;
    } else {
        return;
    }

    if ((cam->cfg->locate_motion_style == "box") ||
        (cam->cfg->locate_motion_style == "cross")) {
        location(p_loc, imgs, imgs->width, image_norm);
    } else if ((cam->cfg->locate_motion_style == "redbox")||
        (cam->cfg->locate_motion_style == "redcross")) {
        red_location(p_loc, imgs, imgs->width, image_norm);
    }

}

void cls_draw::smartmask()
{
    int i, x, v, width, height, line;
    ctx_images *imgs = &cam->imgs;
    u_char *mask_final = cam->alg->smartmask_final;
    u_char *out_y, *out_u, *out_v;
    u_char *out = cam->imgs.image_motion.image_norm;

    i = imgs->motionsize;
    v = i + ((imgs->motionsize) / 4);
    width = imgs->width;
    height = imgs->height;

    /* Set V to 255 to make smartmask appear red. */
    out_v = out + v;
    out_u = out + i;
    for (i = 0; i < height; i += 2) {
        line = i * width;
        for (x = 0; x < width; x += 2) {
            if (mask_final[line + x] == 0 || mask_final[line + x + 1] == 0 ||
                mask_final[line + width + x] == 0 ||
                mask_final[line + width + x + 1] == 0) {

                *out_v = 255;
                *out_u = 128;
            }
            out_v++;
            out_u++;
        }
    }
    out_y = out;
    /* Set colour intensity for smartmask. */
    for (i = 0; i < imgs->motionsize; i++) {
        if (mask_final[i] == 0) {
            *out_y = 0;
        }
        out_y++;
    }
}

void cls_draw::fixed_mask()
{
    int i, x, v, width, height, line;
    ctx_images *imgs = &cam->imgs;
    u_char *mask = imgs->mask;
    u_char *out_y, *out_u, *out_v;
    u_char *out = cam->imgs.image_motion.image_norm;

    i = imgs->motionsize;
    v = i + ((imgs->motionsize) / 4);
    width = imgs->width;
    height = imgs->height;

    /* Set U and V to 0 to make fixed mask appear green. */
    out_v = out + v;
    out_u = out + i;
    for (i = 0; i < height; i += 2) {
        line = i * width;
        for (x = 0; x < width; x += 2) {
            if (mask[line + x] == 0 || mask[line + x + 1] == 0 ||
                mask[line + width + x] == 0 ||
                mask[line + width + x + 1] == 0) {

                *out_v = 0;
                *out_u = 0;
            }
            out_v++;
            out_u++;
        }
    }
    out_y = out;
    /* Set colour intensity for mask. */
    for (i = 0; i < imgs->motionsize; i++) {
        if (mask[i] == 0) {
            *out_y = 0;
        }
        out_y++;
    }
}

void cls_draw::largest_label()
{
    int i, x, v, width, height, line;
    ctx_images *imgs = &cam->imgs;
    int *labels = imgs->labels;
    u_char *out_y, *out_u, *out_v;
    u_char *out = cam->imgs.image_motion.image_norm;

    i = imgs->motionsize;
    v = i + ((imgs->motionsize) / 4);
    width = imgs->width;
    height = imgs->height;

    /* Set U to 255 to make label appear blue. */
    out_u = out + i;
    out_v = out + v;
    for (i = 0; i < height; i += 2) {
        line = i * width;
        for (x = 0; x < width; x += 2) {
            if (labels[line + x] & 32768 || labels[line + x + 1] & 32768 ||
                labels[line + width + x] & 32768 ||
                labels[line + width + x + 1] & 32768) {

                *out_u = 255;
                *out_v = 128;
            }
            out_u++;
            out_v++;
        }
    }
    out_y = out;
    /* Set intensity for coloured label to have better visibility. */
    for (i = 0; i < imgs->motionsize; i++) {
        if (*labels++ & 32768) {
            *out_y = 0;
        }
        out_y++;
    }
}

void cls_draw::text_ft_base(u_char *image_norm
    , std::string txtl, std::string txtr, std::string txbl, std::string txbr)
{
    int retcd;
    std::string tmp;

    mystrftime(cam, tmp,txtl,"");
    av_dict_set(&frm_in->metadata, "mytext_tl", tmp.c_str(), 0);
    mystrftime(cam, tmp,txtr,"");
    av_dict_set(&frm_in->metadata, "mytext_tr", tmp.c_str(), 0);
    mystrftime(cam, tmp,txbl,"");
    av_dict_set(&frm_in->metadata, "mytext_bl", tmp.c_str(), 0);
    mystrftime(cam, tmp,txbr,"");
    av_dict_set(&frm_in->metadata, "mytext_br", tmp.c_str(), 0);

    retcd = av_image_fill_arrays(frm_in->data, frm_in->linesize
        , image_norm, AV_PIX_FMT_YUV420P
        , cam->imgs.width, cam->imgs.height, 1);
    if (retcd < 0) {
        MOTION_LOG(ERR, NO_ERRNO,_("frm_in fill failed"));
        uselegacy = true;
        return;
    }

    retcd = av_buffersrc_add_frame_flags(
        fltctx_src, frm_in, AV_BUFFERSRC_FLAG_KEEP_REF);
    if (retcd < 0) {
        MOTION_LOG(ERR, NO_ERRNO,_("frm_in fltsrc failed"));
        uselegacy = true;
        return;
    }

    frm_out = av_frame_alloc();
    retcd = av_buffersink_get_frame(fltctx_sink, frm_out);
    if (retcd < 0) {
        MOTION_LOG(ERR, NO_ERRNO,_("Buffersink get frame error %d"),retcd);
    } else {
        retcd = av_image_copy_to_buffer(
            (uint8_t *)image_norm
            , ((cam->imgs.width*cam->imgs.height*3)/2)
            , (const uint8_t * const*)frm_out
            , frm_out->linesize
            , (enum AVPixelFormat)frm_out->format
            , frm_out->width, frm_out->height, 1);
        if (retcd < 0) {
            MOTION_LOG(ERR, NO_ERRNO,_("Copy to buffer error %d"),retcd);
        }
    }
    av_frame_free(&frm_out);

}

void cls_draw::text()
{
    std::string tmp;

    if (showdiffs == true) {
        if (cam->pause_detection == false) {
            tmp = std::to_string(cam->current_image->diffs);
        } else {
            tmp = "-";
        }
    } else {
        tmp="";
    }

    if (uselegacy == true) {
        if (showdiffs == true) {
            text_legacy(cam->current_image->image_norm
                , cam->imgs.width - 10, 10
                , tmp.c_str(), scalefactor);
        }
        if (cam->cfg->text_left != "") {
            mystrftime(cam, tmp, cam->cfg->text_left, "");
            text_legacy(cam->current_image->image_norm
                , 10, cam->imgs.height - (10 * scalefactor)
                , tmp.c_str(), scalefactor);
        }
        if (cam->cfg->text_right != "") {
            mystrftime(cam, tmp, cam->cfg->text_right, "");
            text_legacy(cam->current_image->image_norm
                ,cam->imgs.width - 10
                ,cam->imgs.height - (10 * scalefactor)
                ,tmp.c_str(), scalefactor);
        }
    } else {
        if ((cam->cfg->text_left != "") ||
            (cam->cfg->text_right != "") ||
            (showdiffs == true)) {
            text_ft_base(cam->current_image->image_norm
                ,tmp,"",ft_textl,ft_textr);
        }
    }
}

void cls_draw::text_motion()
{
    std::string tmp;

    tmp = std::format("D:{:09}\nL:{:03} N:{:03}"
        ,cam->current_image->diffs
        ,cam->current_image->total_labels
        ,cam->noise);

    if (uselegacy == true) {
        text_legacy(cam->imgs.image_motion.image_norm
            ,cam->imgs.width - 10
            ,cam->imgs.height - (30 * scalefactor)
            ,tmp.c_str(), scalefactor);

        tmp = std::format("Device %d SETUP",cam->cfg->device_id);
        text_legacy(cam->imgs.image_motion.image_norm
            ,cam->imgs.width - 10
            ,cam->imgs.height - (10 * scalefactor)
            ,tmp.c_str(), scalefactor);
    } else {
        text_ft_base(cam->imgs.image_motion.image_norm,tmp,"","","");
    }
}

void cls_draw::text_debug()
{
    std::string tmp, tnote;

    if (cam->current_image->trigger) {
        tnote = "Trigger";
    } else if (cam->current_image->motion) {
        tnote = "Motion";
    } else if (cam->current_image->precap) {
        tnote = "Precap";
    } else if (cam->current_image->postcap) {
        tnote = "Postcap";
    } else {
        tnote = "Other";
    }
    tmp = std::format("D:{:09}\nL:{:03} N:{:03}"
        ,cam->current_image->diffs
        ,cam->current_image->total_labels
        ,cam->noise);

    if (uselegacy == true) {
        text_legacy(cam->current_image->image_norm
            , 10, 20, tmp.c_str(), scalefactor);
        text_legacy(cam->current_image->image_norm
            , 10, 30, tnote.c_str(),scalefactor);
        mystrftime(cam, tmp, "%Y-%m-%d\n%T-%q","");
        text_legacy(cam->current_image->image_norm
            , 10, 40, tmp.c_str(),scalefactor);
    } else {
        text_ft_base(cam->current_image->image_norm,tmp,tnote,"","%Y-%m-%d\n%T-%q");
    }

}

void cls_draw::text_lost()
{
    std::string tmp, tnote;

    if (uselegacy == true) {
        if (cam->device_status == STATUS_OPENED) {
            tnote = "CONNECTION TO CAMERA LOST\\nSINCE %Y-%m-%d %T";
        } else {
            tnote = "UNABLE TO OPEN VIDEO DEVICE\\nSINCE %Y-%m-%d %T";
        }
        mystrftime(cam, tmp, tnote, "");
        text_legacy(cam->current_image->image_norm
            , 10, 20 * scalefactor
            , tmp.c_str(), scalefactor);
    } else {
        if (cam->device_status == STATUS_OPENED) {
            tnote = "CONNECTION TO CAMERA LOST\nSINCE %Y-%m-%d %T";
        } else {
            tnote = "UNABLE TO OPEN VIDEO DEVICE\nSINCE %Y-%m-%d %T";
        }
        text_ft_base(cam->current_image->image_norm,tnote,"","","");
    }
}

cls_draw::cls_draw(cls_camera *p_cam)
{
    cam = p_cam;
    params = cam->cfg->params[cam->cfg->pmap[PARM_PCAT_TEXT]];

    fltctx_src = nullptr;
    fltctx_draw_tl = nullptr;
    fltctx_draw_tr = nullptr;
    fltctx_draw_bl = nullptr;
    fltctx_draw_br = nullptr;
    fltctx_sink = nullptr;
    fltgr = nullptr;
    frm_in = nullptr;
    frm_out = nullptr;

    init_chars();
    init_params();
}

cls_draw::~cls_draw()
{
    avfilter_graph_free(&fltgr);
    av_frame_free(&frm_in);
    av_frame_free(&frm_out);

}

