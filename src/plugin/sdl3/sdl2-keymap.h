/* taken from qemu and adapted to dosemu by stsp */

static inline int sdl2_scancode_to_keynum(SDL_Scancode scan)
{
    switch (scan) {
    case SDL_SCANCODE_A:                 return NUM_A;
    case SDL_SCANCODE_B:                 return NUM_B;
    case SDL_SCANCODE_C:                 return NUM_C;
    case SDL_SCANCODE_D:                 return NUM_D;
    case SDL_SCANCODE_E:                 return NUM_E;
    case SDL_SCANCODE_F:                 return NUM_F;
    case SDL_SCANCODE_G:                 return NUM_G;
    case SDL_SCANCODE_H:                 return NUM_H;
    case SDL_SCANCODE_I:                 return NUM_I;
    case SDL_SCANCODE_J:                 return NUM_J;
    case SDL_SCANCODE_K:                 return NUM_K;
    case SDL_SCANCODE_L:                 return NUM_L;
    case SDL_SCANCODE_M:                 return NUM_M;
    case SDL_SCANCODE_N:                 return NUM_N;
    case SDL_SCANCODE_O:                 return NUM_O;
    case SDL_SCANCODE_P:                 return NUM_P;
    case SDL_SCANCODE_Q:                 return NUM_Q;
    case SDL_SCANCODE_R:                 return NUM_R;
    case SDL_SCANCODE_S:                 return NUM_S;
    case SDL_SCANCODE_T:                 return NUM_T;
    case SDL_SCANCODE_U:                 return NUM_U;
    case SDL_SCANCODE_V:                 return NUM_V;
    case SDL_SCANCODE_W:                 return NUM_W;
    case SDL_SCANCODE_X:                 return NUM_X;
    case SDL_SCANCODE_Y:                 return NUM_Y;
    case SDL_SCANCODE_Z:                 return NUM_Z;

    case SDL_SCANCODE_1:                 return NUM_1;
    case SDL_SCANCODE_2:                 return NUM_2;
    case SDL_SCANCODE_3:                 return NUM_3;
    case SDL_SCANCODE_4:                 return NUM_4;
    case SDL_SCANCODE_5:                 return NUM_5;
    case SDL_SCANCODE_6:                 return NUM_6;
    case SDL_SCANCODE_7:                 return NUM_7;
    case SDL_SCANCODE_8:                 return NUM_8;
    case SDL_SCANCODE_9:                 return NUM_9;
    case SDL_SCANCODE_0:                 return NUM_0;

    case SDL_SCANCODE_RETURN:            return NUM_RETURN;
    case SDL_SCANCODE_ESCAPE:            return NUM_ESC;
    case SDL_SCANCODE_BACKSPACE:         return NUM_BKSP;
    case SDL_SCANCODE_TAB:               return NUM_TAB;
    case SDL_SCANCODE_SPACE:             return NUM_SPACE;
    case SDL_SCANCODE_MINUS:             return NUM_DASH;
    case SDL_SCANCODE_EQUALS:            return NUM_EQUALS;
    case SDL_SCANCODE_LEFTBRACKET:       return NUM_LBRACK;
    case SDL_SCANCODE_RIGHTBRACKET:      return NUM_RBRACK;
    case SDL_SCANCODE_BACKSLASH:         return NUM_BACKSLASH;
#if 0
    case SDL_SCANCODE_NONUSHASH:         return NUM_NONUSHASH;
#endif
    case SDL_SCANCODE_SEMICOLON:         return NUM_SEMICOLON;
    case SDL_SCANCODE_APOSTROPHE:        return NUM_APOSTROPHE;
    case SDL_SCANCODE_GRAVE:             return NUM_GRAVE;
    case SDL_SCANCODE_COMMA:             return NUM_COMMA;
    case SDL_SCANCODE_PERIOD:            return NUM_PERIOD;
    case SDL_SCANCODE_SLASH:             return NUM_SLASH;
    case SDL_SCANCODE_CAPSLOCK:          return NUM_CAPS;

    case SDL_SCANCODE_F1:                return NUM_F1;
    case SDL_SCANCODE_F2:                return NUM_F2;
    case SDL_SCANCODE_F3:                return NUM_F3;
    case SDL_SCANCODE_F4:                return NUM_F4;
    case SDL_SCANCODE_F5:                return NUM_F5;
    case SDL_SCANCODE_F6:                return NUM_F6;
    case SDL_SCANCODE_F7:                return NUM_F7;
    case SDL_SCANCODE_F8:                return NUM_F8;
    case SDL_SCANCODE_F9:                return NUM_F9;
    case SDL_SCANCODE_F10:               return NUM_F10;
    case SDL_SCANCODE_F11:               return NUM_F11;
    case SDL_SCANCODE_F12:               return NUM_F12;

    case SDL_SCANCODE_PRINTSCREEN:       return NUM_PRTSCR_SYSRQ;
    case SDL_SCANCODE_SCROLLLOCK:        return NUM_SCROLL;
    case SDL_SCANCODE_PAUSE:             return NUM_PAUSE_BREAK;
    case SDL_SCANCODE_INSERT:            return NUM_INS;
    case SDL_SCANCODE_HOME:              return NUM_HOME;
    case SDL_SCANCODE_PAGEUP:            return NUM_PGUP;
    case SDL_SCANCODE_DELETE:            return NUM_DEL;
    case SDL_SCANCODE_END:               return NUM_END;
    case SDL_SCANCODE_PAGEDOWN:          return NUM_PGDN;
    case SDL_SCANCODE_RIGHT:             return NUM_RIGHT;
    case SDL_SCANCODE_LEFT:              return NUM_LEFT;
    case SDL_SCANCODE_DOWN:              return NUM_DOWN;
    case SDL_SCANCODE_UP:                return NUM_UP;
    case SDL_SCANCODE_NUMLOCKCLEAR:      return NUM_NUM;

    case SDL_SCANCODE_KP_DIVIDE:         return NUM_PAD_SLASH;
    case SDL_SCANCODE_KP_MULTIPLY:       return NUM_PAD_AST;
    case SDL_SCANCODE_KP_MINUS:          return NUM_PAD_MINUS;
    case SDL_SCANCODE_KP_PLUS:           return NUM_PAD_PLUS;
    case SDL_SCANCODE_KP_ENTER:          return NUM_PAD_ENTER;
    case SDL_SCANCODE_KP_1:              return NUM_PAD_1;
    case SDL_SCANCODE_KP_2:              return NUM_PAD_2;
    case SDL_SCANCODE_KP_3:              return NUM_PAD_3;
    case SDL_SCANCODE_KP_4:              return NUM_PAD_4;
    case SDL_SCANCODE_KP_5:              return NUM_PAD_5;
    case SDL_SCANCODE_KP_6:              return NUM_PAD_6;
    case SDL_SCANCODE_KP_7:              return NUM_PAD_7;
    case SDL_SCANCODE_KP_8:              return NUM_PAD_8;
    case SDL_SCANCODE_KP_9:              return NUM_PAD_9;
    case SDL_SCANCODE_KP_0:              return NUM_PAD_0;
    case SDL_SCANCODE_KP_PERIOD:         return NUM_PAD_DECIMAL;

    case SDL_SCANCODE_NONUSBACKSLASH:    return NUM_LESSGREATER;
    case SDL_SCANCODE_APPLICATION:       return NUM_MENU;
#if 0
    case SDL_SCANCODE_POWER:             return NUM_POWER;
    case SDL_SCANCODE_KP_EQUALS:         return NUM_KP_EQUALS;
#endif

    case SDL_SCANCODE_F13:               return NUM_F13;
    case SDL_SCANCODE_F14:               return NUM_F14;
    case SDL_SCANCODE_F15:               return NUM_F15;
    case SDL_SCANCODE_F16:               return NUM_F16;
    case SDL_SCANCODE_F17:               return NUM_F17;
    case SDL_SCANCODE_F18:               return NUM_F18;
    case SDL_SCANCODE_F19:               return NUM_F19;
    case SDL_SCANCODE_F20:               return NUM_F20;
    case SDL_SCANCODE_F21:               return NUM_F21;
    case SDL_SCANCODE_F22:               return NUM_F22;
    case SDL_SCANCODE_F23:               return NUM_F23;
    case SDL_SCANCODE_F24:               return NUM_F24;

#if 0
    case SDL_SCANCODE_EXECUTE:           return NUM_EXECUTE;
#endif
//    case SDL_SCANCODE_HELP:              return NUM_HELP;
    case SDL_SCANCODE_MENU:              return NUM_MENU;
#if 0
    case SDL_SCANCODE_SELECT:            return NUM_SELECT;
    case SDL_SCANCODE_STOP:              return NUM_STOP;
    case SDL_SCANCODE_AGAIN:             return NUM_AGAIN;
    case SDL_SCANCODE_UNDO:              return NUM_UNDO;
    case SDL_SCANCODE_CUT:               return NUM_CUT;
    case SDL_SCANCODE_COPY:              return NUM_COPY;
    case SDL_SCANCODE_PASTE:             return NUM_PASTE;
    case SDL_SCANCODE_FIND:              return NUM_FIND;
#endif
    case SDL_SCANCODE_MUTE:              return NUM_MUTE;
    case SDL_SCANCODE_VOLUMEUP:          return NUM_VOLUMEUP;
    case SDL_SCANCODE_VOLUMEDOWN:        return NUM_VOLUMEDOWN;

#if 0
    case SDL_SCANCODE_KP_COMMA:          return NUM_KP_COMMA;
    case SDL_SCANCODE_KP_EQUALSAS400:    return NUM_KP_EQUALSAS400;

    case SDL_SCANCODE_INTERNATIONAL1:    return NUM_INTERNATIONAL1;
    case SDL_SCANCODE_INTERNATIONAL2:    return NUM_INTERNATIONAL2;
    case SDL_SCANCODE_INTERNATIONAL3:    return NUM_INTERNATIONAL3;
    case SDL_SCANCODE_INTERNATIONAL4:    return NUM_INTERNATIONAL4;
    case SDL_SCANCODE_INTERNATIONAL5:    return NUM_INTERNATIONAL5;
    case SDL_SCANCODE_INTERNATIONAL6:    return NUM_INTERNATIONAL6;
    case SDL_SCANCODE_INTERNATIONAL7:    return NUM_INTERNATIONAL7;
    case SDL_SCANCODE_INTERNATIONAL8:    return NUM_INTERNATIONAL8;
    case SDL_SCANCODE_INTERNATIONAL9:    return NUM_INTERNATIONAL9;
    case SDL_SCANCODE_LANG1:             return NUM_LANG1;
    case SDL_SCANCODE_LANG2:             return NUM_LANG2;
    case SDL_SCANCODE_LANG3:             return NUM_LANG3;
    case SDL_SCANCODE_LANG4:             return NUM_LANG4;
    case SDL_SCANCODE_LANG5:             return NUM_LANG5;
    case SDL_SCANCODE_LANG6:             return NUM_LANG6;
    case SDL_SCANCODE_LANG7:             return NUM_LANG7;
    case SDL_SCANCODE_LANG8:             return NUM_LANG8;
    case SDL_SCANCODE_LANG9:             return NUM_LANG9;
    case SDL_SCANCODE_ALTERASE:          return NUM_ALTERASE;
#endif
    case SDL_SCANCODE_SYSREQ:            return NUM_SYSRQ;
#if 0
    case SDL_SCANCODE_CANCEL:            return NUM_CANCEL;
    case SDL_SCANCODE_CLEAR:             return NUM_CLEAR;
    case SDL_SCANCODE_PRIOR:             return NUM_PRIOR;
    case SDL_SCANCODE_RETURN2:           return NUM_RETURN2;
    case SDL_SCANCODE_SEPARATOR:         return NUM_SEPARATOR;
    case SDL_SCANCODE_OUT:               return NUM_OUT;
    case SDL_SCANCODE_OPER:              return NUM_OPER;
    case SDL_SCANCODE_CLEARAGAIN:        return NUM_CLEARAGAIN;
    case SDL_SCANCODE_CRSEL:             return NUM_CRSEL;
    case SDL_SCANCODE_EXSEL:             return NUM_EXSEL;
    case SDL_SCANCODE_KP_00:             return NUM_KP_00;
    case SDL_SCANCODE_KP_000:            return NUM_KP_000;
    case SDL_SCANCODE_THOUSANDSSEPARATOR: return NUM_THOUSANDSSEPARATOR;
    case SDL_SCANCODE_DECIMALSEPARATOR:  return NUM_DECIMALSEPARATOR;
    case SDL_SCANCODE_CURRENCYUNIT:      return NUM_CURRENCYUNIT;
    case SDL_SCANCODE_CURRENCYSUBUNIT:   return NUM_CURRENCYSUBUNIT;
    case SDL_SCANCODE_KP_LEFTPAREN:      return NUM_KP_LEFTPAREN;
    case SDL_SCANCODE_KP_RIGHTPAREN:     return NUM_KP_RIGHTPAREN;
    case SDL_SCANCODE_KP_LEFTBRACE:      return NUM_KP_LEFTBRACE;
    case SDL_SCANCODE_KP_RIGHTBRACE:     return NUM_KP_RIGHTBRACE;
    case SDL_SCANCODE_KP_TAB:            return NUM_KP_TAB;
    case SDL_SCANCODE_KP_BACKSPACE:      return NUM_KP_BACKSPACE;
    case SDL_SCANCODE_KP_A:              return NUM_KP_A;
    case SDL_SCANCODE_KP_B:              return NUM_KP_B;
    case SDL_SCANCODE_KP_C:              return NUM_KP_C;
    case SDL_SCANCODE_KP_D:              return NUM_KP_D;
    case SDL_SCANCODE_KP_E:              return NUM_KP_E;
    case SDL_SCANCODE_KP_F:              return NUM_KP_F;
    case SDL_SCANCODE_KP_XOR:            return NUM_KP_XOR;
    case SDL_SCANCODE_KP_POWER:          return NUM_KP_POWER;
    case SDL_SCANCODE_KP_PERCENT:        return NUM_KP_PERCENT;
    case SDL_SCANCODE_KP_LESS:           return NUM_KP_LESS;
    case SDL_SCANCODE_KP_GREATER:        return NUM_KP_GREATER;
    case SDL_SCANCODE_KP_AMPERSAND:      return NUM_KP_AMPERSAND;
    case SDL_SCANCODE_KP_DBLAMPERSAND:   return NUM_KP_DBLAMPERSAND;
    case SDL_SCANCODE_KP_VERTICALBAR:    return NUM_KP_VERTICALBAR;
    case SDL_SCANCODE_KP_DBLVERTICALBAR: return NUM_KP_DBLVERTICALBAR;
    case SDL_SCANCODE_KP_COLON:          return NUM_KP_COLON;
    case SDL_SCANCODE_KP_HASH:           return NUM_KP_HASH;
    case SDL_SCANCODE_KP_SPACE:          return NUM_KP_SPACE;
    case SDL_SCANCODE_KP_AT:             return NUM_KP_AT;
    case SDL_SCANCODE_KP_EXCLAM:         return NUM_KP_EXCLAM;
    case SDL_SCANCODE_KP_MEMSTORE:       return NUM_KP_MEMSTORE;
    case SDL_SCANCODE_KP_MEMRECALL:      return NUM_KP_MEMRECALL;
    case SDL_SCANCODE_KP_MEMCLEAR:       return NUM_KP_MEMCLEAR;
    case SDL_SCANCODE_KP_MEMADD:         return NUM_KP_MEMADD;
    case SDL_SCANCODE_KP_MEMSUBTRACT:    return NUM_KP_MEMSUBTRACT;
    case SDL_SCANCODE_KP_MEMMULTIPLY:    return NUM_KP_MEMMULTIPLY;
    case SDL_SCANCODE_KP_MEMDIVIDE:      return NUM_KP_MEMDIVIDE;
    case SDL_SCANCODE_KP_PLUSMINUS:      return NUM_KP_PLUSMINUS;
    case SDL_SCANCODE_KP_CLEAR:          return NUM_KP_CLEAR;
    case SDL_SCANCODE_KP_CLEARENTRY:     return NUM_KP_CLEARENTRY;
    case SDL_SCANCODE_KP_BINARY:         return NUM_KP_BINARY;
    case SDL_SCANCODE_KP_OCTAL:          return NUM_KP_OCTAL;
    case SDL_SCANCODE_KP_DECIMAL:        return NUM_KP_DECIMAL;
    case SDL_SCANCODE_KP_HEXADECIMAL:    return NUM_KP_HEXADECIMAL;
#endif
    case SDL_SCANCODE_LCTRL:             return NUM_L_CTRL;
    case SDL_SCANCODE_LSHIFT:            return NUM_L_SHIFT;
    case SDL_SCANCODE_LALT:              return NUM_L_ALT;
    case SDL_SCANCODE_LGUI:              return NUM_LWIN;
    case SDL_SCANCODE_RCTRL:             return NUM_R_CTRL;
    case SDL_SCANCODE_RSHIFT:            return NUM_R_SHIFT;
    case SDL_SCANCODE_RALT:              return NUM_R_ALT;
    case SDL_SCANCODE_RGUI:              return NUM_RWIN;
#if 0
    case SDL_SCANCODE_MODE:              return NUM_MODE;
    case SDL_SCANCODE_AUDIONEXT:         return NUM_AUDIONEXT;
    case SDL_SCANCODE_AUDIOPREV:         return NUM_AUDIOPREV;
    case SDL_SCANCODE_AUDIOSTOP:         return NUM_AUDIOSTOP;
    case SDL_SCANCODE_AUDIOPLAY:         return NUM_AUDIOPLAY;
    case SDL_SCANCODE_AUDIOMUTE:         return NUM_AUDIOMUTE;
    case SDL_SCANCODE_MEDIASELECT:       return NUM_MEDIASELECT;
    case SDL_SCANCODE_WWW:               return NUM_WWW;
    case SDL_SCANCODE_MAIL:              return NUM_MAIL;
    case SDL_SCANCODE_CALCULATOR:        return NUM_CALCULATOR;
    case SDL_SCANCODE_COMPUTER:          return NUM_COMPUTER;
    case SDL_SCANCODE_AC_SEARCH:         return NUM_AC_SEARCH;
    case SDL_SCANCODE_AC_HOME:           return NUM_AC_HOME;
    case SDL_SCANCODE_AC_BACK:           return NUM_AC_BACK;
    case SDL_SCANCODE_AC_FORWARD:        return NUM_AC_FORWARD;
    case SDL_SCANCODE_AC_STOP:           return NUM_AC_STOP;
    case SDL_SCANCODE_AC_REFRESH:        return NUM_AC_REFRESH;
#endif
    case SDL_SCANCODE_AC_BOOKMARKS:      return NUM_AC_BOOKMARKS;
#if 0
    case SDL_SCANCODE_BRIGHTNESSDOWN:    return NUM_BRIGHTNESSDOWN;
    case SDL_SCANCODE_BRIGHTNESSUP:      return NUM_BRIGHTNESSUP;
    case SDL_SCANCODE_DISPLAYSWITCH:     return NUM_DISPLAYSWITCH;
    case SDL_SCANCODE_KBDILLUMTOGGLE:    return NUM_KBDILLUMTOGGLE;
    case SDL_SCANCODE_KBDILLUMDOWN:      return NUM_KBDILLUMDOWN;
    case SDL_SCANCODE_KBDILLUMUP:        return NUM_KBDILLUMUP;
    case SDL_SCANCODE_EJECT:             return NUM_EJECT;
    case SDL_SCANCODE_SLEEP:             return NUM_SLEEP;
    case SDL_SCANCODE_APP1:              return NUM_APP1;
    case SDL_SCANCODE_APP2:              return NUM_APP2;
#endif
    default:
        break;
    }
    return 0;
}
