/* Generated by re2c 3.0 */
/**
 * SDL_shader_tools; tools for SDL GPU shader support.
 *
 * Please see the file LICENSE.txt in the source's root directory.
 */

// This was originally based on examples/pp-c.re from re2c: https://re2c.org/
//   re2c is public domain code.
//
// You build mojoshader_lexer.c from the .re file with re2c...
// re2c -is -o mojoshader_lexer.c mojoshader_lexer.re
//
// Changes to the lexer are done to the .re file, not the C code!
//
// Please note that this isn't a perfect C lexer, since it is used for both
//  HLSL and shader assembly language, and follows the quirks of Microsoft's
//  tools (!!! FIXME: that was true for MojoShader, but we can fix anything
//  that pops up now).

#define __SDL_SHADER_INTERNAL__ 1
#include "SDL_shader_internal.h"

typedef unsigned char uchar;

#define YYMAXFILL 8

#define RET(t) return update_state(s, eoi, cursor, token, (Token) t)
#define YYCTYPE uchar
#define YYCURSOR cursor
#define YYLIMIT limit
#define YYMARKER s->lexer_marker
#define YYFILL(n) { if ((n) == 1) { cursor = sentinel; limit = cursor + YYMAXFILL; eoi = 1; } }

static uchar sentinel[YYMAXFILL];

static Token update_state(IncludeState *s, int eoi, const uchar *cur, const uchar *tok, const Token val)
{
    if (eoi) {
        s->bytes_left = 0;
        s->source = (const char *) s->source_base + s->orig_length;
        if ( (tok >= sentinel) && (tok < (sentinel+YYMAXFILL)) ) {
            s->token = s->source;
        } else {
            s->token = (const char *) tok;
        }
    } else {
        s->bytes_left -= (size_t) (cur - ((const uchar *) s->source));
        s->source = (const char *) cur;
        s->token = (const char *) tok;
    }
    s->tokenlen = (size_t) (s->source - s->token);
    s->tokenval = val;
    return val;
}

Token preprocessor_lexer(IncludeState *s)
{
    const uchar *cursor = (const uchar *) s->source;
    const uchar *token = cursor;
    const uchar *matchptr;
    const uchar *limit = cursor + s->bytes_left;
    int eoi = 0;



    // preprocessor directives are only valid at start of line.
    if (s->tokenval == ((Token) '\n')) {
        goto ppdirective;  // may jump back to scanner_loop.
    }

    // Microsoft's preprocessor (and GCC!) allows multiline comments
    //  before a preprocessor directive, even though C/C++
    //  doesn't. See if we've hit this case.
    if (s->tokenval == TOKEN_MULTI_COMMENT) {
        goto ppdirective;  // may jump back to scanner_loop.
    }

scanner_loop:
    if (YYLIMIT <= YYCURSOR) { YYFILL(1); }
    token = cursor;


{
	YYCTYPE yych;
	unsigned int yyaccept = 0;
	if ((YYLIMIT - YYCURSOR) < 4) YYFILL(4);
	yych = *YYCURSOR;
	switch (yych) {
		case 0x00: goto yy1;
		case '\t':
		case '\v':
		case '\f':
		case ' ': goto yy4;
		case '\n': goto yy5;
		case '\r': goto yy7;
		case '!': goto yy8;
		case '"': goto yy9;
		case '#': goto yy10;
		case '%': goto yy11;
		case '&': goto yy12;
		case '\'': goto yy13;
		case '(': goto yy14;
		case ')': goto yy15;
		case '*': goto yy16;
		case '+': goto yy17;
		case ',': goto yy18;
		case '-': goto yy19;
		case '.': goto yy20;
		case '/': goto yy22;
		case '0': goto yy23;
		case '1':
		case '2':
		case '3':
		case '4':
		case '5':
		case '6':
		case '7':
		case '8':
		case '9': goto yy25;
		case ':': goto yy27;
		case ';': goto yy28;
		case '<': goto yy29;
		case '=': goto yy31;
		case '>': goto yy32;
		case '?': goto yy34;
		case '@': goto yy35;
		case 'A':
		case 'B':
		case 'C':
		case 'D':
		case 'E':
		case 'F':
		case 'G':
		case 'H':
		case 'I':
		case 'J':
		case 'K':
		case 'L':
		case 'M':
		case 'N':
		case 'O':
		case 'P':
		case 'Q':
		case 'R':
		case 'S':
		case 'T':
		case 'U':
		case 'V':
		case 'W':
		case 'X':
		case 'Y':
		case 'Z':
		case '_':
		case 'a':
		case 'b':
		case 'c':
		case 'd':
		case 'e':
		case 'f':
		case 'g':
		case 'h':
		case 'i':
		case 'j':
		case 'k':
		case 'l':
		case 'm':
		case 'n':
		case 'o':
		case 'p':
		case 'q':
		case 'r':
		case 's':
		case 't':
		case 'u':
		case 'v':
		case 'w':
		case 'x':
		case 'y':
		case 'z': goto yy36;
		case '[': goto yy38;
		case '\\': goto yy39;
		case ']': goto yy40;
		case '^': goto yy41;
		case '{': goto yy42;
		case '|': goto yy43;
		case '}': goto yy44;
		case '~': goto yy45;
		default: goto yy2;
	}
yy1:
	++YYCURSOR;
	{ if (eoi) { RET(TOKEN_EOI); } goto bad_chars; }
yy2:
	++YYCURSOR;
yy3:
	{ goto bad_chars; }
yy4:
	++YYCURSOR;
	if (YYLIMIT <= YYCURSOR) YYFILL(1);
	yych = *YYCURSOR;
	if (yych <= '\n') {
		if (yych == '\t') goto yy4;
	} else {
		if (yych <= '\f') goto yy4;
		if (yych == ' ') goto yy4;
	}
	{ if (s->report_whitespace) { RET(' '); } goto scanner_loop; }
yy5:
	++YYCURSOR;
yy6:
	{ s->line++; RET('\n'); }
yy7:
	yych = *++YYCURSOR;
	if (yych == '\n') goto yy5;
	goto yy6;
yy8:
	yych = *++YYCURSOR;
	if (yych == '=') goto yy46;
	{ RET('!'); }
yy9:
	++YYCURSOR;
	{ goto stringliteral; }
yy10:
	yych = *++YYCURSOR;
	if (yych == '#') goto yy47;
	{ RET(TOKEN_HASH); }
yy11:
	yych = *++YYCURSOR;
	if (yych == '=') goto yy48;
	{ RET('%'); }
yy12:
	yych = *++YYCURSOR;
	if (yych == '&') goto yy49;
	if (yych == '=') goto yy50;
	{ RET('&'); }
yy13:
	yyaccept = 0;
	yych = *(YYMARKER = ++YYCURSOR);
	if (yych <= '\n') {
		if (yych <= 0x00) goto yy3;
		if (yych <= '\t') goto yy52;
		goto yy3;
	} else {
		if (yych == '\r') goto yy3;
		goto yy52;
	}
yy14:
	++YYCURSOR;
	{ RET('('); }
yy15:
	++YYCURSOR;
	{ RET(')'); }
yy16:
	yych = *++YYCURSOR;
	if (yych == '=') goto yy56;
	{ RET('*'); }
yy17:
	yych = *++YYCURSOR;
	if (yych == '+') goto yy57;
	if (yych == '=') goto yy58;
	{ RET('+'); }
yy18:
	++YYCURSOR;
	{ RET(','); }
yy19:
	yych = *++YYCURSOR;
	if (yych == '-') goto yy59;
	if (yych == '=') goto yy60;
	{ RET('-'); }
yy20:
	yych = *++YYCURSOR;
	if (yych <= '/') goto yy21;
	if (yych <= '9') goto yy61;
yy21:
	{ RET('.'); }
yy22:
	yych = *++YYCURSOR;
	if (yych <= '.') {
		if (yych == '*') goto yy63;
	} else {
		if (yych <= '/') goto yy64;
		if (yych == '=') goto yy65;
	}
	{ RET('/'); }
yy23:
	yyaccept = 1;
	yych = *(YYMARKER = ++YYCURSOR);
	if (yych == 'X') goto yy68;
	if (yych == 'x') goto yy68;
	goto yy26;
yy24:
	{ RET(TOKEN_INT_LITERAL); }
yy25:
	yyaccept = 1;
	YYMARKER = ++YYCURSOR;
	if ((YYLIMIT - YYCURSOR) < 3) YYFILL(3);
	yych = *YYCURSOR;
yy26:
	if (yych <= 'L') {
		if (yych <= '9') {
			if (yych == '.') goto yy61;
			if (yych <= '/') goto yy24;
			goto yy25;
		} else {
			if (yych == 'E') goto yy66;
			if (yych <= 'K') goto yy24;
			goto yy67;
		}
	} else {
		if (yych <= 'e') {
			if (yych == 'U') goto yy67;
			if (yych <= 'd') goto yy24;
			goto yy66;
		} else {
			if (yych <= 'l') {
				if (yych <= 'k') goto yy24;
				goto yy67;
			} else {
				if (yych == 'u') goto yy67;
				goto yy24;
			}
		}
	}
yy27:
	++YYCURSOR;
	{ RET(':'); }
yy28:
	++YYCURSOR;
	{ if (s->asm_comments) { goto singlelinecomment; } RET(';'); }
yy29:
	yych = *++YYCURSOR;
	if (yych <= ';') goto yy30;
	if (yych <= '<') goto yy69;
	if (yych <= '=') goto yy70;
yy30:
	{ RET('<'); }
yy31:
	yych = *++YYCURSOR;
	if (yych == '=') goto yy71;
	{ RET('='); }
yy32:
	yych = *++YYCURSOR;
	if (yych <= '<') goto yy33;
	if (yych <= '=') goto yy72;
	if (yych <= '>') goto yy73;
yy33:
	{ RET('>'); }
yy34:
	++YYCURSOR;
	{ RET('?'); }
yy35:
	++YYCURSOR;
	{ RET('@'); }
yy36:
	++YYCURSOR;
	if (YYLIMIT <= YYCURSOR) YYFILL(1);
	yych = *YYCURSOR;
	if (yych <= 'Z') {
		if (yych <= '/') goto yy37;
		if (yych <= '9') goto yy36;
		if (yych >= 'A') goto yy36;
	} else {
		if (yych <= '_') {
			if (yych >= '_') goto yy36;
		} else {
			if (yych <= '`') goto yy37;
			if (yych <= 'z') goto yy36;
		}
	}
yy37:
	{ RET(TOKEN_IDENTIFIER); }
yy38:
	++YYCURSOR;
	{ RET('['); }
yy39:
	yyaccept = 0;
	yych = *(YYMARKER = ++YYCURSOR);
	if (yych <= 0x08) goto yy3;
	if (yych <= '\r') goto yy75;
	if (yych == ' ') goto yy75;
	goto yy3;
yy40:
	++YYCURSOR;
	{ RET(']'); }
yy41:
	yych = *++YYCURSOR;
	if (yych == '=') goto yy79;
	{ RET('^'); }
yy42:
	++YYCURSOR;
	{ RET('{'); }
yy43:
	yych = *++YYCURSOR;
	if (yych == '=') goto yy80;
	if (yych == '|') goto yy81;
	{ RET('|'); }
yy44:
	++YYCURSOR;
	{ RET('}'); }
yy45:
	++YYCURSOR;
	{ RET('~'); }
yy46:
	++YYCURSOR;
	{ RET(TOKEN_NEQ); }
yy47:
	++YYCURSOR;
	{ RET(TOKEN_HASHHASH); }
yy48:
	++YYCURSOR;
	{ RET(TOKEN_MODASSIGN); }
yy49:
	++YYCURSOR;
	{ RET(TOKEN_ANDAND); }
yy50:
	++YYCURSOR;
	{ RET(TOKEN_ANDASSIGN); }
yy51:
	++YYCURSOR;
	if (YYLIMIT <= YYCURSOR) YYFILL(1);
	yych = *YYCURSOR;
yy52:
	if (yych <= '\r') {
		if (yych <= '\t') {
			if (yych >= 0x01) goto yy51;
		} else {
			if (yych <= '\n') goto yy53;
			if (yych <= '\f') goto yy51;
		}
	} else {
		if (yych <= '\'') {
			if (yych <= '&') goto yy51;
			goto yy54;
		} else {
			if (yych == '\\') goto yy55;
			goto yy51;
		}
	}
yy53:
	YYCURSOR = YYMARKER;
	if (yyaccept <= 1) {
		if (yyaccept == 0) {
			goto yy3;
		} else {
			goto yy24;
		}
	} else {
		goto yy62;
	}
yy54:
	++YYCURSOR;
	goto yy24;
yy55:
	++YYCURSOR;
	if (YYLIMIT <= YYCURSOR) YYFILL(1);
	yych = *YYCURSOR;
	if (yych <= 'b') {
		if (yych <= '7') {
			if (yych <= '&') {
				if (yych == '"') goto yy51;
				goto yy53;
			} else {
				if (yych <= '\'') goto yy51;
				if (yych <= '/') goto yy53;
				goto yy51;
			}
		} else {
			if (yych <= '[') {
				if (yych == '?') goto yy51;
				goto yy53;
			} else {
				if (yych <= '\\') goto yy51;
				if (yych <= '`') goto yy53;
				goto yy51;
			}
		}
	} else {
		if (yych <= 'r') {
			if (yych <= 'm') {
				if (yych == 'f') goto yy51;
				goto yy53;
			} else {
				if (yych <= 'n') goto yy51;
				if (yych <= 'q') goto yy53;
				goto yy51;
			}
		} else {
			if (yych <= 'u') {
				if (yych == 't') goto yy51;
				goto yy53;
			} else {
				if (yych <= 'v') goto yy51;
				if (yych == 'x') goto yy82;
				goto yy53;
			}
		}
	}
yy56:
	++YYCURSOR;
	{ RET(TOKEN_MULTASSIGN); }
yy57:
	++YYCURSOR;
	{ RET(TOKEN_INCREMENT); }
yy58:
	++YYCURSOR;
	{ RET(TOKEN_ADDASSIGN); }
yy59:
	++YYCURSOR;
	{ RET(TOKEN_DECREMENT); }
yy60:
	++YYCURSOR;
	{ RET(TOKEN_SUBASSIGN); }
yy61:
	yyaccept = 2;
	YYMARKER = ++YYCURSOR;
	if ((YYLIMIT - YYCURSOR) < 3) YYFILL(3);
	yych = *YYCURSOR;
	if (yych <= 'G') {
		if (yych <= 'D') {
			if (yych <= '/') goto yy62;
			if (yych <= '9') goto yy61;
		} else {
			if (yych <= 'E') goto yy66;
			if (yych <= 'F') goto yy83;
		}
	} else {
		if (yych <= 'e') {
			if (yych <= 'H') goto yy83;
			if (yych >= 'e') goto yy66;
		} else {
			if (yych == 'g') goto yy62;
			if (yych <= 'h') goto yy83;
		}
	}
yy62:
	{ RET(TOKEN_FLOAT_LITERAL); }
yy63:
	++YYCURSOR;
	{ goto multilinecomment; }
yy64:
	++YYCURSOR;
	{ goto singlelinecomment; }
yy65:
	++YYCURSOR;
	{ RET(TOKEN_DIVASSIGN); }
yy66:
	yych = *++YYCURSOR;
	if (yych <= ',') {
		if (yych == '+') goto yy84;
		goto yy53;
	} else {
		if (yych <= '-') goto yy84;
		if (yych <= '/') goto yy53;
		if (yych <= '9') goto yy85;
		goto yy53;
	}
yy67:
	++YYCURSOR;
	if (YYLIMIT <= YYCURSOR) YYFILL(1);
	yych = *YYCURSOR;
	if (yych <= 'U') {
		if (yych == 'L') goto yy67;
		if (yych <= 'T') goto yy24;
		goto yy67;
	} else {
		if (yych <= 'l') {
			if (yych <= 'k') goto yy24;
			goto yy67;
		} else {
			if (yych == 'u') goto yy67;
			goto yy24;
		}
	}
yy68:
	yych = *++YYCURSOR;
	if (yych <= '@') {
		if (yych <= '/') goto yy53;
		if (yych <= '9') goto yy86;
		goto yy53;
	} else {
		if (yych <= 'F') goto yy86;
		if (yych <= '`') goto yy53;
		if (yych <= 'f') goto yy86;
		goto yy53;
	}
yy69:
	yych = *++YYCURSOR;
	if (yych == '=') goto yy87;
	{ RET(TOKEN_LSHIFT); }
yy70:
	++YYCURSOR;
	{ RET(TOKEN_LEQ); }
yy71:
	++YYCURSOR;
	{ RET(TOKEN_EQL); }
yy72:
	++YYCURSOR;
	{ RET(TOKEN_GEQ); }
yy73:
	yych = *++YYCURSOR;
	if (yych == '=') goto yy88;
	{ RET(TOKEN_RSHIFT); }
yy74:
	++YYCURSOR;
	if ((YYLIMIT - YYCURSOR) < 2) YYFILL(2);
	yych = *YYCURSOR;
yy75:
	if (yych <= '\f') {
		if (yych <= 0x08) goto yy53;
		if (yych != '\n') goto yy74;
	} else {
		if (yych <= '\r') goto yy78;
		if (yych == ' ') goto yy74;
		goto yy53;
	}
yy76:
	++YYCURSOR;
yy77:
	{ s->line++; goto scanner_loop; }
yy78:
	yych = *++YYCURSOR;
	if (yych == '\n') goto yy76;
	goto yy77;
yy79:
	++YYCURSOR;
	{ RET(TOKEN_XORASSIGN); }
yy80:
	++YYCURSOR;
	{ RET(TOKEN_ORASSIGN); }
yy81:
	++YYCURSOR;
	{ RET(TOKEN_OROR); }
yy82:
	++YYCURSOR;
	if (YYLIMIT <= YYCURSOR) YYFILL(1);
	yych = *YYCURSOR;
	if (yych <= '@') {
		if (yych <= '/') goto yy53;
		if (yych <= '9') goto yy51;
		goto yy53;
	} else {
		if (yych <= 'F') goto yy51;
		if (yych <= '`') goto yy53;
		if (yych <= 'f') goto yy51;
		goto yy53;
	}
yy83:
	++YYCURSOR;
	goto yy62;
yy84:
	yych = *++YYCURSOR;
	if (yych <= '/') goto yy53;
	if (yych >= ':') goto yy53;
yy85:
	++YYCURSOR;
	if (YYLIMIT <= YYCURSOR) YYFILL(1);
	yych = *YYCURSOR;
	if (yych <= 'G') {
		if (yych <= '9') {
			if (yych <= '/') goto yy62;
			goto yy85;
		} else {
			if (yych == 'F') goto yy83;
			goto yy62;
		}
	} else {
		if (yych <= 'f') {
			if (yych <= 'H') goto yy83;
			if (yych <= 'e') goto yy62;
			goto yy83;
		} else {
			if (yych == 'h') goto yy83;
			goto yy62;
		}
	}
yy86:
	++YYCURSOR;
	if (YYLIMIT <= YYCURSOR) YYFILL(1);
	yych = *YYCURSOR;
	if (yych <= 'T') {
		if (yych <= '@') {
			if (yych <= '/') goto yy24;
			if (yych <= '9') goto yy86;
			goto yy24;
		} else {
			if (yych <= 'F') goto yy86;
			if (yych == 'L') goto yy67;
			goto yy24;
		}
	} else {
		if (yych <= 'k') {
			if (yych <= 'U') goto yy67;
			if (yych <= '`') goto yy24;
			if (yych <= 'f') goto yy86;
			goto yy24;
		} else {
			if (yych <= 'l') goto yy67;
			if (yych == 'u') goto yy67;
			goto yy24;
		}
	}
yy87:
	++YYCURSOR;
	{ RET(TOKEN_LSHIFTASSIGN); }
yy88:
	++YYCURSOR;
	{ RET(TOKEN_RSHIFTASSIGN); }
}


stringliteral:
    /* !!! FIXME: forbid newlines in string literals? */
    /* !!! FIXME: the ANY section used to be `(ESC|ANY\[\r\n\\"])` ...is that redundant or was it like that for a reason? */
    if (YYLIMIT <= YYCURSOR) { YYFILL(1); }

{
	YYCTYPE yych;
	if (YYLIMIT <= YYCURSOR) YYFILL(1);
	yych = *YYCURSOR;
	if (yych <= 0x00) goto yy90;
	if (yych == '"') goto yy92;
	goto yy91;
yy90:
	++YYCURSOR;
	{
                        if (eoi) {
                            RET(TOKEN_INCOMPLETE_STRING_LITERAL);
                        }
                        goto stringliteral;
                    }
yy91:
	++YYCURSOR;
	{ goto stringliteral; }
yy92:
	++YYCURSOR;
	{ RET(TOKEN_STRING_LITERAL); }
}


multilinecomment:
    if (YYLIMIT <= YYCURSOR) { YYFILL(1); }
    matchptr = cursor;
/* The "*\/" is just to avoid screwing up text editor syntax highlighting. */

{
	YYCTYPE yych;
	if ((YYLIMIT - YYCURSOR) < 2) YYFILL(2);
	yych = *YYCURSOR;
	if (yych <= '\f') {
		if (yych <= 0x00) goto yy94;
		if (yych == '\n') goto yy97;
		goto yy95;
	} else {
		if (yych <= '\r') goto yy99;
		if (yych == '*') goto yy100;
		goto yy95;
	}
yy94:
	++YYCURSOR;
	{
                        if (eoi) {
                            RET(TOKEN_INCOMPLETE_COMMENT);
                        }
                        goto multilinecomment;
                    }
yy95:
	++YYCURSOR;
yy96:
	{ goto multilinecomment; }
yy97:
	++YYCURSOR;
yy98:
	{
                        s->line++;
                        goto multilinecomment;
                    }
yy99:
	yych = *++YYCURSOR;
	if (yych == '\n') goto yy97;
	goto yy98;
yy100:
	yych = *++YYCURSOR;
	if (yych != '/') goto yy96;
	++YYCURSOR;
	{ RET(TOKEN_MULTI_COMMENT); }
}


singlelinecomment:
    if (YYLIMIT <= YYCURSOR) { YYFILL(1); }
    matchptr = cursor;

{
	YYCTYPE yych;
	if ((YYLIMIT - YYCURSOR) < 2) YYFILL(2);
	yych = *YYCURSOR;
	if (yych <= '\n') {
		if (yych <= 0x00) goto yy102;
		if (yych <= '\t') goto yy103;
		goto yy104;
	} else {
		if (yych == '\r') goto yy106;
		goto yy103;
	}
yy102:
	++YYCURSOR;
	{
                        cursor = matchptr;  // so we RET(TOKEN_EOI) next.
                        RET(TOKEN_SINGLE_COMMENT);
                    }
yy103:
	++YYCURSOR;
	{ goto singlelinecomment; }
yy104:
	++YYCURSOR;
yy105:
	{
                        cursor = matchptr;  // so we RET('\n') next.
                        RET(TOKEN_SINGLE_COMMENT);
                    }
yy106:
	yych = *++YYCURSOR;
	if (yych == '\n') goto yy104;
	goto yy105;
}


ppdirective:
    if (YYLIMIT <= YYCURSOR) { YYFILL(1); }

{
	YYCTYPE yych;
	unsigned int yyaccept = 0;
	if ((YYLIMIT - YYCURSOR) < 8) YYFILL(8);
	yych = *YYCURSOR;
	if (yych <= '\f') {
		if (yych <= 0x08) {
			if (yych >= 0x01) goto yy109;
		} else {
			if (yych == '\n') goto yy109;
			goto yy111;
		}
	} else {
		if (yych <= ' ') {
			if (yych <= 0x1F) goto yy109;
			goto yy111;
		} else {
			if (yych == '#') goto yy112;
			goto yy109;
		}
	}
yy108:
	YYCURSOR = YYMARKER;
	if (yyaccept == 0) {
		goto yy110;
	} else {
		goto yy126;
	}
yy109:
	++YYCURSOR;
yy110:
	{
                            token = cursor = (const uchar *) s->source;
                            limit = cursor + s->bytes_left;
                            goto scanner_loop;
                        }
yy111:
	++YYCURSOR;
	if (YYLIMIT <= YYCURSOR) YYFILL(1);
	yych = *YYCURSOR;
	if (yych <= '\n') {
		if (yych == '\t') goto yy111;
	} else {
		if (yych <= '\f') goto yy111;
		if (yych == ' ') goto yy111;
	}
	{ goto ppdirective; }
yy112:
	yyaccept = 0;
	yych = *(YYMARKER = ++YYCURSOR);
	if (yych <= 'h') {
		if (yych <= 0x1F) {
			if (yych == '\t') goto yy114;
			goto yy110;
		} else {
			if (yych <= ' ') goto yy114;
			if (yych <= 'c') goto yy110;
			if (yych <= 'e') goto yy114;
			goto yy110;
		}
	} else {
		if (yych <= 'o') {
			if (yych <= 'i') goto yy114;
			if (yych == 'l') goto yy114;
			goto yy110;
		} else {
			if (yych <= 'p') goto yy114;
			if (yych == 'u') goto yy114;
			goto yy110;
		}
	}
yy113:
	++YYCURSOR;
	if ((YYLIMIT - YYCURSOR) < 7) YYFILL(7);
	yych = *YYCURSOR;
yy114:
	if (yych <= 'h') {
		if (yych <= ' ') {
			if (yych == '\t') goto yy113;
			if (yych <= 0x1F) goto yy108;
			goto yy113;
		} else {
			if (yych <= 'c') goto yy108;
			if (yych <= 'd') goto yy115;
			if (yych <= 'e') goto yy116;
			goto yy108;
		}
	} else {
		if (yych <= 'o') {
			if (yych <= 'i') goto yy117;
			if (yych == 'l') goto yy118;
			goto yy108;
		} else {
			if (yych <= 'p') goto yy119;
			if (yych == 'u') goto yy120;
			goto yy108;
		}
	}
yy115:
	yych = *++YYCURSOR;
	if (yych == 'e') goto yy121;
	goto yy108;
yy116:
	yych = *++YYCURSOR;
	if (yych <= 'm') {
		if (yych == 'l') goto yy122;
		goto yy108;
	} else {
		if (yych <= 'n') goto yy123;
		if (yych == 'r') goto yy124;
		goto yy108;
	}
yy117:
	yych = *++YYCURSOR;
	if (yych == 'f') goto yy125;
	if (yych == 'n') goto yy127;
	goto yy108;
yy118:
	yych = *++YYCURSOR;
	if (yych == 'i') goto yy128;
	goto yy108;
yy119:
	yych = *++YYCURSOR;
	if (yych == 'r') goto yy129;
	goto yy108;
yy120:
	yych = *++YYCURSOR;
	if (yych == 'n') goto yy130;
	goto yy108;
yy121:
	yych = *++YYCURSOR;
	if (yych == 'f') goto yy131;
	goto yy108;
yy122:
	yych = *++YYCURSOR;
	if (yych == 'i') goto yy132;
	if (yych == 's') goto yy133;
	goto yy108;
yy123:
	yych = *++YYCURSOR;
	if (yych == 'd') goto yy134;
	goto yy108;
yy124:
	yych = *++YYCURSOR;
	if (yych == 'r') goto yy135;
	goto yy108;
yy125:
	yyaccept = 1;
	yych = *(YYMARKER = ++YYCURSOR);
	if (yych == 'd') goto yy136;
	if (yych == 'n') goto yy137;
yy126:
	{ RET(TOKEN_PP_IF); }
yy127:
	yych = *++YYCURSOR;
	if (yych == 'c') goto yy138;
	goto yy108;
yy128:
	yych = *++YYCURSOR;
	if (yych == 'n') goto yy139;
	goto yy108;
yy129:
	yych = *++YYCURSOR;
	if (yych == 'a') goto yy140;
	goto yy108;
yy130:
	yych = *++YYCURSOR;
	if (yych == 'd') goto yy141;
	goto yy108;
yy131:
	yych = *++YYCURSOR;
	if (yych == 'i') goto yy142;
	goto yy108;
yy132:
	yych = *++YYCURSOR;
	if (yych == 'f') goto yy143;
	goto yy108;
yy133:
	yych = *++YYCURSOR;
	if (yych == 'e') goto yy144;
	goto yy108;
yy134:
	yych = *++YYCURSOR;
	if (yych == 'i') goto yy145;
	goto yy108;
yy135:
	yych = *++YYCURSOR;
	if (yych == 'o') goto yy146;
	goto yy108;
yy136:
	yych = *++YYCURSOR;
	if (yych == 'e') goto yy147;
	goto yy108;
yy137:
	yych = *++YYCURSOR;
	if (yych == 'd') goto yy148;
	goto yy108;
yy138:
	yych = *++YYCURSOR;
	if (yych == 'l') goto yy149;
	goto yy108;
yy139:
	yych = *++YYCURSOR;
	if (yych == 'e') goto yy150;
	goto yy108;
yy140:
	yych = *++YYCURSOR;
	if (yych == 'g') goto yy151;
	goto yy108;
yy141:
	yych = *++YYCURSOR;
	if (yych == 'e') goto yy152;
	goto yy108;
yy142:
	yych = *++YYCURSOR;
	if (yych == 'n') goto yy153;
	goto yy108;
yy143:
	++YYCURSOR;
	{ RET(TOKEN_PP_ELIF); }
yy144:
	++YYCURSOR;
	{ RET(TOKEN_PP_ELSE); }
yy145:
	yych = *++YYCURSOR;
	if (yych == 'f') goto yy154;
	goto yy108;
yy146:
	yych = *++YYCURSOR;
	if (yych == 'r') goto yy155;
	goto yy108;
yy147:
	yych = *++YYCURSOR;
	if (yych == 'f') goto yy156;
	goto yy108;
yy148:
	yych = *++YYCURSOR;
	if (yych == 'e') goto yy157;
	goto yy108;
yy149:
	yych = *++YYCURSOR;
	if (yych == 'u') goto yy158;
	goto yy108;
yy150:
	++YYCURSOR;
	{ RET(TOKEN_PP_LINE); }
yy151:
	yych = *++YYCURSOR;
	if (yych == 'm') goto yy159;
	goto yy108;
yy152:
	yych = *++YYCURSOR;
	if (yych == 'f') goto yy160;
	goto yy108;
yy153:
	yych = *++YYCURSOR;
	if (yych == 'e') goto yy161;
	goto yy108;
yy154:
	++YYCURSOR;
	{ RET(TOKEN_PP_ENDIF); }
yy155:
	++YYCURSOR;
	{ RET(TOKEN_PP_ERROR); }
yy156:
	++YYCURSOR;
	{ RET(TOKEN_PP_IFDEF); }
yy157:
	yych = *++YYCURSOR;
	if (yych == 'f') goto yy162;
	goto yy108;
yy158:
	yych = *++YYCURSOR;
	if (yych == 'd') goto yy163;
	goto yy108;
yy159:
	yych = *++YYCURSOR;
	if (yych == 'a') goto yy164;
	goto yy108;
yy160:
	++YYCURSOR;
	{ RET(TOKEN_PP_UNDEF); }
yy161:
	++YYCURSOR;
	{ RET(TOKEN_PP_DEFINE); }
yy162:
	++YYCURSOR;
	{ RET(TOKEN_PP_IFNDEF); }
yy163:
	yych = *++YYCURSOR;
	if (yych == 'e') goto yy165;
	goto yy108;
yy164:
	++YYCURSOR;
	{ RET(TOKEN_PP_PRAGMA); }
yy165:
	++YYCURSOR;
	{ RET(TOKEN_PP_INCLUDE); }
}


bad_chars:
    if (YYLIMIT <= YYCURSOR) { YYFILL(1); }

{
	YYCTYPE yych;
	if (YYLIMIT <= YYCURSOR) YYFILL(1);
	yych = *YYCURSOR;
	if (yych <= '#') {
		if (yych <= '\r') {
			if (yych <= 0x00) goto yy167;
			if (yych <= 0x08) goto yy168;
			goto yy169;
		} else {
			if (yych <= 0x1F) goto yy168;
			if (yych == '"') goto yy168;
			goto yy169;
		}
	} else {
		if (yych <= '@') {
			if (yych <= '$') goto yy168;
			if (yych <= '?') goto yy169;
			goto yy168;
		} else {
			if (yych == '`') goto yy168;
			if (yych <= '~') goto yy169;
			goto yy168;
		}
	}
yy167:
	++YYCURSOR;
	{
                        if (eoi)
                        {
                            SDL_assert( !((token >= sentinel) && (token < sentinel+YYMAXFILL)) );
                            eoi = 0;
                            cursor = (uchar *) s->source_base + s->orig_length;
                            RET(TOKEN_BAD_CHARS);  // next call will be EOI.
                        }
                        goto bad_chars;
                    }
yy168:
	++YYCURSOR;
	{ goto bad_chars; }
yy169:
	++YYCURSOR;
	{ cursor--; RET(TOKEN_BAD_CHARS); }
}


    SDL_assert(0 && "Shouldn't hit this code");
    RET(TOKEN_UNKNOWN);
}

/* end of SDL_shader_lexer.re (or .c) ... */

