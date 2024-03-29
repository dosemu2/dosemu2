#include "init.h"
#include "translate.h"

static const t_unicode iso8859_5_g1_chars[] = {
0x00A0, 0x0401, 0x0402, 0x0403, 0x0404, 0x0405, 0x0406, 0x0407, /* 0xA0-0xA7 */
0x0408, 0x0409, 0x040A, 0x040B, 0x040C, 0x00AD, 0x040E, 0x040F, /* 0xA8-0xAF */
0x0410, 0x0411, 0x0412, 0x0413, 0x0414, 0x0415, 0x0416, 0x0417, /* 0xB0-0xB7 */
0x0418, 0x0419, 0x041A, 0x041B, 0x041C, 0x041D, 0x041E, 0x041F, /* 0xB8-0xBF */
0x0420, 0x0421, 0x0422, 0x0423, 0x0424, 0x0425, 0x0426, 0x0427, /* 0xC0-0xC7 */
0x0428, 0x0429, 0x042A, 0x042B, 0x042C, 0x042D, 0x042E, 0x042F, /* 0xC8-0xCF */
0x0430, 0x0431, 0x0432, 0x0433, 0x0434, 0x0435, 0x0436, 0x0437, /* 0xD0-0xD7 */
0x0438, 0x0439, 0x043A, 0x043B, 0x043C, 0x043D, 0x043E, 0x043F, /* 0xD8-0xDF */
0x0440, 0x0441, 0x0442, 0x0443, 0x0444, 0x0445, 0x0446, 0x0447, /* 0xE0-0xE7 */
0x0448, 0x0449, 0x044A, 0x044B, 0x044C, 0x044D, 0x044E, 0x044F, /* 0xE8-0xEF */
0x2116, 0x0451, 0x0452, 0x0453, 0x0454, 0x0455, 0x0456, 0x0457, /* 0xF0-0xF7 */
0x0458, 0x0459, 0x045A, 0x045B, 0x045C, 0x00A7, 0x045E, 0x045F, /* 0xF8-0xFF */
};
struct char_set iso8859_5_g1 = {
	1,
	CHARS(iso8859_5_g1_chars),
	144, "L", 1, 96,
};

struct char_set iso8859_5 = {
	.c0 = &ascii_c0,
	.g0 = &ascii_g0,
	.c1 = &ascii_c1,
	.g1 = &iso8859_5_g1,
	.names = { "iso8859-5", 0 },
};

CONSTRUCTOR(static void init(void))
{
	register_charset(&iso8859_5_g1);
	register_charset(&iso8859_5);
}
