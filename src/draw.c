/*
 *    draw.c
 *
 *    Routines for drawing text on images
 *
 *    Copyright 2000, Jeroen Vreeken
 *    This program is published under the GNU public license version 2
 *    See also the file 'COPYING'
 *
 */

#include <ctype.h>
#include "motion.h"
#include "util.h"
#include "logger.h"
#include "draw.h"

/* Highest ascii value is 126 (~) */
#define ASCII_MAX 127

unsigned char *char_arr_ptr[ASCII_MAX];

struct draw_char {
    unsigned char ascii;
    unsigned char pix[8][7];
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

#define NEWLINE "\\n"
/**
 * draw_textn
 */
static int draw_textn(unsigned char *image, int startx,  int starty,  int width, const char *text, int len, int factor)
{

    int x, y;
    int pos, line_offset, next_char_offs;
    unsigned char *image_ptr, *char_ptr;

    if (startx > width / 2)
        startx -= len * (6 * factor);

    if (startx + len * 6 * factor >= width)
        len = (width-startx-1)/(6*factor);

    if ((startx < 1) || (starty < 1) || (len < 1)) return 0;

    line_offset = width - (7 * factor);
    next_char_offs = (width * 8 * factor) - (6 * factor);

    image_ptr = image + startx + (starty * width);

    for (pos = 0; pos < len; pos++) {
        int pos_check = (int)text[pos];

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

    return 0;
}

/**
 * draw_text
 */
int draw_text(unsigned char *image, int width, int height, int startx, int starty, const char *text, int factor)
{
    int num_nl = 0;
    const char *end, *begin;
    int line_space, txtlen;

    /* Count the number of newlines in "text" so we scroll it up the image. */
    begin = end = text;
    txtlen = 0;
    while ((end = strstr(end, NEWLINE))) {
        if ((end - begin)>txtlen) txtlen = (end - begin);
        num_nl++;
        end += sizeof(NEWLINE)-1;
    }
    if (txtlen == 0) txtlen = strlen(text);

    /* Adjust the factor if it is out of bounds
     * txtlen at this point is the approx length of longest line
    */
    if ((txtlen * 7 * factor) > width){
        factor = (width / (txtlen * 7));
        if (factor <= 0) factor = 1;
    }

    if (((num_nl+1) * 8 * factor) > height){
        factor = (height / ((num_nl+1) * 8));
        if (factor <= 0) factor = 1;
    }

    line_space = factor * 9;

    starty -= line_space * num_nl;

    begin = end = text;

    while ((end = strstr(end, NEWLINE))) {
        int len = end-begin;

        draw_textn(image, startx, starty, width, begin, len, factor);
        end += sizeof(NEWLINE)-1;
        begin = end;
        starty += line_space;
    }

    draw_textn(image, startx, starty, width, begin, strlen(begin), factor);

    return 0;
}

/** initialize_chars */
int draw_init_chars(void) {
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

    return 0;
}

void draw_init_scale(struct ctx_cam *cam){

    /* Consider that web interface may change conf values at any moment.
     * The below can put two sections in the image so make sure that after
     * scaling does not occupy more than 1/4 of image (10 pixels * 2 lines)
     */

    cam->text_scale = cam->conf.text_scale;
    if (cam->text_scale <= 0) cam->text_scale = 1;

    if ((cam->text_scale * 10 * 2) > (cam->imgs.width / 4)) {
        cam->text_scale = (cam->imgs.width / (4 * 10 * 2));
        if (cam->text_scale <= 0) cam->text_scale = 1;
        MOTION_LOG(WRN, TYPE_ALL, NO_ERRNO
            ,_("Invalid text scale.  Adjusted to %d"), cam->text_scale);
    }

    if ((cam->text_scale * 10 * 2) > (cam->imgs.height / 4)) {
        cam->text_scale = (cam->imgs.height / (4 * 10 * 2));
        if (cam->text_scale <= 0) cam->text_scale = 1;
        MOTION_LOG(WRN, TYPE_ALL, NO_ERRNO
            ,_("Invalid text scale.  Adjusted to %d"), cam->text_scale);
    }

    /* If we had to modify the scale, change conf so we don't get another message */
    cam->conf.text_scale = cam->text_scale;

}

/** Draws a box around the movement. */
static void draw_location(struct ctx_coord *cent, struct ctx_images *imgs, int width, unsigned char *new,
                       int style, int mode, int process_thisframe)
{
    unsigned char *out = imgs->image_motion.image_norm;
    int x, y;

    out = imgs->image_motion.image_norm;

    /* Debug image always gets a 'normal' box. */
    if ((mode == LOCATE_BOTH) && process_thisframe) {
        int width_miny = width * cent->miny;
        int width_maxy = width * cent->maxy;

        for (x = cent->minx; x <= cent->maxx; x++) {
            int width_miny_x = x + width_miny;
            int width_maxy_x = x + width_maxy;

            out[width_miny_x] =~out[width_miny_x];
            out[width_maxy_x] =~out[width_maxy_x];
        }

        for (y = cent->miny; y <= cent->maxy; y++) {
            int width_minx_y = cent->minx + y * width;
            int width_maxx_y = cent->maxx + y * width;

            out[width_minx_y] =~out[width_minx_y];
            out[width_maxx_y] =~out[width_maxx_y];
        }
    }
    if (style == LOCATE_BOX) { /* Draw a box on normal images. */
        int width_miny = width * cent->miny;
        int width_maxy = width * cent->maxy;

        for (x = cent->minx; x <= cent->maxx; x++) {
            int width_miny_x = x + width_miny;
            int width_maxy_x = x + width_maxy;

            new[width_miny_x] =~new[width_miny_x];
            new[width_maxy_x] =~new[width_maxy_x];
        }

        for (y = cent->miny; y <= cent->maxy; y++) {
            int width_minx_y = cent->minx + y * width;
            int width_maxx_y = cent->maxx + y * width;

            new[width_minx_y] =~new[width_minx_y];
            new[width_maxx_y] =~new[width_maxx_y];
        }
    } else if (style == LOCATE_CROSS) { /* Draw a cross on normal images. */
        int centy = cent->y * width;

        for (x = cent->x - 10;  x <= cent->x + 10; x++) {
            new[centy + x] =~new[centy + x];
            out[centy + x] =~out[centy + x];
        }

        for (y = cent->y - 10; y <= cent->y + 10; y++) {
            new[cent->x + y * width] =~new[cent->x + y * width];
            out[cent->x + y * width] =~out[cent->x + y * width];
        }
    }
}


/** Draws a RED box around the movement. */
static void draw_red_location(struct ctx_coord *cent, struct ctx_images *imgs, int width, unsigned char *new,
                           int style, int mode, int process_thisframe)
{
    unsigned char *out = imgs->image_motion.image_norm;
    unsigned char *new_u, *new_v;
    int x, y, v, cwidth, cblock;

    cwidth = width / 2;
    cblock = imgs->motionsize / 4;
    x = imgs->motionsize;
    v = x + cblock;
    out = imgs->image_motion.image_norm;
    new_u = new + x;
    new_v = new + v;

    /* Debug image always gets a 'normal' box. */
    if ((mode == LOCATE_BOTH) && process_thisframe) {
        int width_miny = width * cent->miny;
        int width_maxy = width * cent->maxy;

        for (x = cent->minx; x <= cent->maxx; x++) {
            int width_miny_x = x + width_miny;
            int width_maxy_x = x + width_maxy;

            out[width_miny_x] =~out[width_miny_x];
            out[width_maxy_x] =~out[width_maxy_x];
        }

        for (y = cent->miny; y <= cent->maxy; y++) {
            int width_minx_y = cent->minx + y * width;
            int width_maxx_y = cent->maxx + y * width;

            out[width_minx_y] =~out[width_minx_y];
            out[width_maxx_y] =~out[width_maxx_y];
        }
    }

    if (style == LOCATE_REDBOX) { /* Draw a red box on normal images. */
        int width_miny = width * cent->miny;
        int width_maxy = width * cent->maxy;
        int cwidth_miny = cwidth * (cent->miny / 2);
        int cwidth_maxy = cwidth * (cent->maxy / 2);

        for (x = cent->minx + 2; x <= cent->maxx - 2; x += 2) {
            int width_miny_x = x + width_miny;
            int width_maxy_x = x + width_maxy;
            int cwidth_miny_x = x / 2 + cwidth_miny;
            int cwidth_maxy_x = x / 2 + cwidth_maxy;

            new_u[cwidth_miny_x] = 128;
            new_u[cwidth_maxy_x] = 128;
            new_v[cwidth_miny_x] = 255;
            new_v[cwidth_maxy_x] = 255;

            new[width_miny_x] = 128;
            new[width_maxy_x] = 128;

            new[width_miny_x + 1] = 128;
            new[width_maxy_x + 1] = 128;

            new[width_miny_x + width] = 128;
            new[width_maxy_x + width] = 128;

            new[width_miny_x + 1 + width] = 128;
            new[width_maxy_x + 1 + width] = 128;
        }

        for (y = cent->miny; y <= cent->maxy; y += 2) {
            int width_minx_y = cent->minx + y * width;
            int width_maxx_y = cent->maxx + y * width;
            int cwidth_minx_y = (cent->minx / 2) + (y / 2) * cwidth;
            int cwidth_maxx_y = (cent->maxx / 2) + (y / 2) * cwidth;

            new_u[cwidth_minx_y] = 128;
            new_u[cwidth_maxx_y] = 128;
            new_v[cwidth_minx_y] = 255;
            new_v[cwidth_maxx_y] = 255;

            new[width_minx_y] = 128;
            new[width_maxx_y] = 128;

            new[width_minx_y + width] = 128;
            new[width_maxx_y + width] = 128;

            new[width_minx_y + 1] = 128;
            new[width_maxx_y + 1] = 128;

            new[width_minx_y + width + 1] = 128;
            new[width_maxx_y + width + 1] = 128;
        }
    } else if (style == LOCATE_REDCROSS) { /* Draw a red cross on normal images. */
        int cwidth_maxy = cwidth * (cent->y / 2);

        for (x = cent->x - 10; x <= cent->x + 10; x += 2) {
            int cwidth_maxy_x = x / 2 + cwidth_maxy;

            new_u[cwidth_maxy_x] = 128;
            new_v[cwidth_maxy_x] = 255;
        }

        for (y = cent->y - 10; y <= cent->y + 10; y += 2) {
            int cwidth_minx_y = (cent->x / 2) + (y / 2) * cwidth;

            new_u[cwidth_minx_y] = 128;
            new_v[cwidth_minx_y] = 255;
        }
    }
}

void draw_locate_preview(struct ctx_cam *cam, struct ctx_image_data *img){
    /* draw locate box here when mode = LOCATE_PREVIEW */
    if (cam->locate_motion_mode == LOCATE_PREVIEW) {

        if (cam->locate_motion_style == LOCATE_BOX) {
            draw_location(&img->location, &cam->imgs, cam->imgs.width, cam->imgs.image_preview.image_norm,
                              LOCATE_BOX, LOCATE_NORMAL, cam->process_thisframe);
        } else if (cam->locate_motion_style == LOCATE_REDBOX) {
            draw_red_location(&img->location, &cam->imgs, cam->imgs.width, cam->imgs.image_preview.image_norm,
                                  LOCATE_REDBOX, LOCATE_NORMAL, cam->process_thisframe);
        } else if (cam->locate_motion_style == LOCATE_CROSS) {
            draw_location(&img->location, &cam->imgs, cam->imgs.width, cam->imgs.image_preview.image_norm,
                              LOCATE_CROSS, LOCATE_NORMAL, cam->process_thisframe);
        } else if (cam->locate_motion_style == LOCATE_REDCROSS) {
            draw_red_location(&img->location, &cam->imgs, cam->imgs.width, cam->imgs.image_preview.image_norm,
                                  LOCATE_REDCROSS, LOCATE_NORMAL, cam->process_thisframe);
        }
    }
}

void draw_locate(struct ctx_cam *cam, struct ctx_image_data *img){
    struct ctx_images *imgs = &cam->imgs;
    struct ctx_coord *location = &img->location;

    if (cam->locate_motion_mode == LOCATE_ON) {

        if (cam->locate_motion_style == LOCATE_BOX) {
            draw_location(location, imgs, imgs->width, img->image_norm, LOCATE_BOX,
                              LOCATE_BOTH, cam->process_thisframe);
        } else if (cam->locate_motion_style == LOCATE_REDBOX) {
            draw_red_location(location, imgs, imgs->width, img->image_norm, LOCATE_REDBOX,
                                  LOCATE_BOTH, cam->process_thisframe);
        } else if (cam->locate_motion_style == LOCATE_CROSS) {
            draw_location(location, imgs, imgs->width, img->image_norm, LOCATE_CROSS,
                              LOCATE_BOTH, cam->process_thisframe);
        } else if (cam->locate_motion_style == LOCATE_REDCROSS) {
            draw_red_location(location, imgs, imgs->width, img->image_norm, LOCATE_REDCROSS,
                                  LOCATE_BOTH, cam->process_thisframe);
        }
    }
}

/** Draw the smart mask on the motion images */
void draw_smartmask(struct ctx_cam *cam, unsigned char *out) {
    int i, x, v, width, height, line;
    struct ctx_images *imgs = &cam->imgs;
    unsigned char *smartmask = imgs->smartmask_final;
    unsigned char *out_y, *out_u, *out_v;

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
            if (smartmask[line + x] == 0 || smartmask[line + x + 1] == 0 ||
                smartmask[line + width + x] == 0 ||
                smartmask[line + width + x + 1] == 0) {

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
        if (smartmask[i] == 0)
            *out_y = 0;
        out_y++;
    }
}

/** Draw the fixed mask on the motion images */
void draw_fixed_mask(struct ctx_cam *cam, unsigned char *out){
    int i, x, v, width, height, line;
    struct ctx_images *imgs = &cam->imgs;
    unsigned char *mask = imgs->mask;
    unsigned char *out_y, *out_u, *out_v;

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
        if (mask[i] == 0)
            *out_y = 0;
        out_y++;
    }
}

/** Draw largest label on the motion images */
void draw_largest_label(struct ctx_cam *cam, unsigned char *out) {
    int i, x, v, width, height, line;
    struct ctx_images *imgs = &cam->imgs;
    int *labels = imgs->labels;
    unsigned char *out_y, *out_u, *out_v;

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
        if (*labels++ & 32768)
            *out_y = 0;
        out_y++;
    }
}
