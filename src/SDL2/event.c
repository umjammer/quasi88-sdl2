/***********************************************************************
 * イベント処理 (システム依存)
 *
 *	詳細は、 event.h 参照
 ************************************************************************/

#include <SDL2/SDL.h>

#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "quasi88.h"
#include "getconf.h"
#include "keyboard.h"

#include "drive.h"

#include "emu.h"
#include "device.h"
#include "screen.h"
#include "event.h"

#include "intr.h"			/* test */
#include "screen.h"			/* test */

int 	show_fps = FALSE;		/* test */

static	int 	display_fps_init(void);	/* test */
static	void	display_fps(void);		/* test */

int 	use_cmdkey = 1;			/* Commandキーでメニューへ遷移     */

int 	keyboard_type = 1;		/* キーボードの種類                */
char	*file_keyboard = NULL;	/* キー設定ファイル名		   */

int 	use_joydevice = TRUE;	/* ジョイスティックデバイスを開く? */


#define	JOY_MAX   	KEY88_PAD_MAX		/* ジョイスティック上限(2個) */

#define	BUTTON_MAX	KEY88_PAD_BUTTON_MAX	/* ボタン上限(8個)	     */

#define	AXIS_U		0x01
#define	AXIS_D		0x02
#define	AXIS_L		0x04
#define	AXIS_R		0x08

typedef struct {

    SDL_Joystick *dev;		/* オープンしたジョイスティックの構造体 */
    int		  num;		/* QUASI88 でのジョイスティック番号 0〜 */

    int		  axis;		/* 方向ボタン押下状態			*/
    int		  nr_button;	/* 有効なボタンの数			*/

} T_JOY_INFO;

static T_JOY_INFO joy_info[ JOY_MAX ];

static	int	joystick_num;		/* オープンしたジョイスティックの数 */

static	const char *debug_sdlkeysym(int code); /* デバッグ用 */

/*==========================================================================
 * キー配列について
 *
 *  一般キー(文字キー) は、
 *	106 キーボードの場合、PC-8801 と同じなので問題なし。
 *	101 キーボードの場合、一部配置が異なるし、キーが足りない。
 *	とりあえず、以下のように配置してみる。
 *		` → \、 = → ^、 [ ] はそのまま、 \ → @、' → :、右CTRL → _
 *
 *  特殊キー(機能キー) は、
 *	ホスト側のキー刻印と似た雰囲気の機能を、PC-8801のキーに割り当てよう。
 *	Pause は STOP、 PrintScreen は COPY など。個人的な主観で決める。
 *
 *  テンキーは、
 *	PC-8801と106キーでキー刻印が若干異なるが、そのままのキー刻印を使う。
 *	となると、 = と , が無い。 mac ならあるが。
 *
 *  最下段のキーの配置は、適当に割り振る。 (カッコのキーには割り振らない)
 *
 *	PC-8801        かな GRPH 決定  スペース 変換  PC    全角
 *	101キー   Ctrl      Alt        スペース             Alt          Ctrl
 *	104キー   Ctrl Win  Alt        スペース             Alt  Win App Ctrl
 *	109キー   Ctrl Win  Alt 無変換 スペース 変換 (ひら) Alt  Win App Ctrl
 *	mac ?     Ctrl      Opt (Cmd)  スペース      (cmd) (Opt)        (Ctrl)
 *
 * SDLのキー入力についての推測 (Windows & 106キーの場合)
 *	○環境変数 SDL_VIDEODRIVER が windib と directx とで挙動が異なる。
 *	○「SHIFT」を押しながら 「1」 を押しても、keysym は 「1」 のまま。
 *	  つまり、 「!」 は入力されないみたい。
 *	  大文字 「A」 も入力されない。 keysym は 「a」 となる。
 *	○キー配列は 101 がベースになっている。
 *	  「^」 を押しても keysym は 「=」 になる。
 *	○いくつかのキーで、 keycode が重複している
 *	  windib  だと、カーソルキーとテンキーなど、たくさん。
 *	  directx なら、重複は無い ?
 *	○いくつかのキーで、 keysym が重複している
 *	  windib  だと、￥ と ]  (ともに ￥ になる)
 *	  directx なら、重複は無い ?
 *	○いくつかのキーで、キーシンボルが未定義
 *	  無変換、変換、カタカナひらがな が未定義
 *	  windib  だと、＼ が未定義
 *	  directx だと、＾￥＠：、半角/全角 が未定義
 *	○いくつかのキーで、キーを離した時の検知が不安定(?)
 *	  windib  だと 半角/全角、カタカナひらがな、PrintScreen
 *	  directx だと ALT
 *	○キーロックに難あり(?)
 *	  NumLockはロックできる。
 *	  windib  だと SHIFT + CapsLock がロック可。
 *	  directx だと CapsLock、カタカナひらがな、半角/全角がロック可。
 *	○NumLock中のテンキー入力に難あり(?)
 *	  windib  だと NumLock中に SHIFT + テンキーで、SHIFTが一時的にオフ
 *	  NumLockしてなければ問題なし。
 *	  windib  だと この問題はない。
 *
 *	○メニューモードでは、UNICODE を有効にする。
 *	  こうすれば、「SHIFT」+「1」 を 「!」 と認識できるし、「SHIFT」+「2」
 *	  は 「"」になる。しかし、  directx だと、入力できない文字があるぞ。
 *
 *	○ところで、日本語Windowsでの101キーボードと、英語Windowsでの
 *	  101キーボードって、同じ挙動なんだろうか・・・
 *	  directx の時のキーコード割り当てが明らかに不自然なのだが。
 *===========================================================================*/

/* ソフトウェアNumLock をオンした際の、キーバインディング変更テーブル */

typedef struct {
    int		type;		/* KEYCODE_INVALID / SYM / SCAN		*/
    int		code;		/* キーシンボル、ないし、スキャンコード	*/
    int		new_key88;	/* NumLock ON 時の QUASI88キーコード	*/
    int		org_key88;	/* NumLock OFF時の QUASI88キーコード	*/
} T_BINDING;


/* キーバインディングをデフォルト(初期値)から変更する際の、テーブル */

typedef struct {
    int		type;		/* KEYCODE_INVALID / SYM / SCAN		*/
    int		code;		/* キーシンボル、ないし、スキャンコード	*/
    int		key88;		/* 変更する QUASI88キーコード           */
} T_REMAPPING;



/*----------------------------------------------------------------------
 * SDL の keysym を QUASI88 の キーコードに変換するテーブル
 *
 *	キーシンボル SDLK_xxx が押されたら、 
 *	keysym2key88[ SDLK_xxx ] が押されたとする。
 *
 *	keysym2key88[] には、 KEY88_xxx をセットしておく。
 *	初期値は keysym2key88_default[] と同じ
 *----------------------------------------------------------------------*/
// static int keysym2key88[ SDLK_LAST ];
static int keysym2key88[ 410 ];


/*----------------------------------------------------------------------
 * SDL の scancode を QUASI88 の キーコードに変換するテーブル
 *
 *	スキャンコード code が押されたら、
 *	keycode2key88[ code ] が押されたとする。
 *
 *	keycode2key88[] には、 KEY88_xxx または -1 をセットしておく。
 *	これは keysym2key88[] に優先される。(ただし -1 の場合は無効)
 *	初期値は 全て -1、変換可能なスキャンコードは 0〜255までに制限。
 *----------------------------------------------------------------------*/
static int scancode2key88[ 283 ];

 

/*----------------------------------------------------------------------
 * ソフトウェア NumLock オン時の キーコード変換情報
 *
 *	binding[].code (SDL の keysym ないし keycode) が押されたら、
 *	binding[].new_key88 (KEY88_xxx) が押されたことにする。
 *
 *	ソフトウェア NumLock オン時は、この情報にしたがって、
 *	keysym2key88[] 、 keycode2key88[] を書き換える。
 *	変更できるキーの個数は、64個まで (これだけあればいいだろう)
 *----------------------------------------------------------------------*/
static T_BINDING binding[ 64 ];





/*----------------------------------------------------------------------
 * SDLK_xxx → KEY88_xxx 変換テーブル (デフォルト)
 *----------------------------------------------------------------------*/

//static const int keysym2key88_default[ SDLK_LAST ] =
static const int keysym2key88_default[ 410 ] =
{
  0,				/*	SDLK_UNKNOWN	0x00 ('\0')	*/
  0, 0, 0, 0, 0, 0, 0,
  KEY88_INS_DEL,	/*	SDLK_BACKSPACE	0x08 ('\b')	*/
  KEY88_TAB,		/*	SDLK_TAB	0x09 ('\t')	*/
  0, 0, 0,
  KEY88_RETURNL,	/*	SDLK_RETURN	0x0D ('\r')	*/
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  KEY88_ESC,		/*	SDLK_ESCAPE	0x1B ('\033')	*/
  0, 0, 0, 0,
  KEY88_SPACE,		/*	SDLK_SPACE	0x20 (' ')	*/
  KEY88_EXCLAM,		/*	SDLK_EXCLAIM	0x21 ('!')	*/
  KEY88_QUOTEDBL,	/*	SDLK_QUOTEDBL	0x22 ('"')	*/
  KEY88_NUMBERSIGN,	/*	SDLK_HASH	0x23 ('#')	*/
  KEY88_DOLLAR,		/*	SDLK_DOLLAR	0x24 ('$')	*/
  KEY88_PERCENT,	/*	SDLK_PERCENT	0x25 ('%')	*/
  KEY88_AMPERSAND,	/*	SDLK_AMPERSAND	0x26 ('&')	*/
  KEY88_APOSTROPHE,	/*	SDLK_QUOTE	0x27 ('\'')	*/
  KEY88_PARENLEFT,	/*	SDLK_LEFTPAREN	0x28 ('(')	*/
  KEY88_PARENRIGHT,	/*	SDLK_RIGHTPAREN	0x29 (')')	*/
  KEY88_ASTERISK,	/*	SDLK_ASTERISK	0x2A ('*')	*/
  KEY88_PLUS,		/*	SDLK_PLUS	0x2B ('+')	*/
  KEY88_COMMA,		/*	SDLK_COMMA	0x2C (',')	*/
  KEY88_MINUS,		/*	SDLK_MINUS	0x2D ('-')	*/
  KEY88_PERIOD,		/*	SDLK_PERIOD	0x2E ('.')	*/
  KEY88_SLASH,		/*	SDLK_SLASH	0x2F ('/')	*/
  KEY88_0,			/*	SDLK_0	0x30 ('0')	*/
  KEY88_1,			/*	SDLK_1	0x31 ('1')	*/
  KEY88_2,			/*	SDLK_2	0x32 ('2')	*/
  KEY88_3,			/*	SDLK_3	0x33 ('3')	*/
  KEY88_4,			/*	SDLK_4	0x34 ('4')	*/
  KEY88_5,			/*	SDLK_5	0x35 ('5')	*/
  KEY88_6,			/*	SDLK_6	0x36 ('6')	*/
  KEY88_7,			/*	SDLK_7	0x37 ('7')	*/
  KEY88_8,			/*	SDLK_8	0x38 ('8')	*/
  KEY88_9,			/*	SDLK_9	0x39 ('9')	*/
  KEY88_COLON,		/*	SDLK_COLON	0x3A (':')	*/
  KEY88_SEMICOLON,	/*	SDLK_SEMICOLON	0x3B (';')	*/
  KEY88_LESS,		/*	SDLK_LESS	0x3C ('<')	*/
  KEY88_EQUAL,		/*	SDLK_EQUALS	0x3D ('=')	*/
  KEY88_GREATER,	/*	SDLK_GREATER	0x3E ('>')	*/
  KEY88_QUESTION,	/*	SDLK_QUESTION	0x3F ('?')	*/
  KEY88_AT,			/*	SDLK_AT	0x40 ('@')	*/
  KEY88_A,			/*			0x41	*/
  KEY88_B,			/*			0x42	*/
  KEY88_C,			/*			0x43	*/
  KEY88_D,			/*			0x44	*/
  KEY88_E,			/*			0x45	*/
  KEY88_F,			/*			0x46	*/
  KEY88_G,			/*			0x47	*/
  KEY88_H,			/*			0x48	*/
  KEY88_I,			/*			0x49	*/
  KEY88_J,			/*			0x4A	*/
  KEY88_K,			/*			0x4B	*/
  KEY88_L,			/*			0x4C	*/
  KEY88_M,			/*			0x4D	*/
  KEY88_N,			/*			0x4E	*/
  KEY88_O,			/*			0x4F	*/
  KEY88_P,			/*			0x50	*/
  KEY88_Q,			/*			0x51	*/
  KEY88_R,			/*			0x52	*/
  KEY88_S,			/*			0x53	*/
  KEY88_T,			/*			0x54	*/
  KEY88_U,			/*			0x55	*/
  KEY88_V,			/*			0x56	*/
  KEY88_W,			/*			0x57	*/
  KEY88_X,			/*			0x58	*/
  KEY88_Y,			/*			0x59	*/
  KEY88_Z,			/*			0x5A	*/
  KEY88_BRACKETLEFT,/*	SDLK_LEFTBRACKET	0x5B ('[')	*/
  KEY88_YEN,		/*	SDLK_BACKSLASH	0x5C ('\\')	*/
  KEY88_BRACKETRIGHT,/*	SDLK_RIGHTBRACKET	0x5D (']')	*/
  KEY88_CARET,		/*	SDLK_CARET	0x5E ('^')	*/
  KEY88_UNDERSCORE,	/*	SDLK_UNDERSCORE	0x5F ('_')	*/
  KEY88_BACKQUOTE,	/*	SDLK_BACKQUOTE	0x60 ('`')	*/
  KEY88_a,			/*	SDLK_a	0x61 ('a')	*/
  KEY88_b,			/*	SDLK_b	0x62 ('b')	*/
  KEY88_c,			/*	SDLK_c	0x63 ('c')	*/
  KEY88_d,			/*	SDLK_d	0x64 ('d')	*/
  KEY88_e,			/*	SDLK_e	0x65 ('e')	*/
  KEY88_f,			/*	SDLK_f	0x66 ('f')	*/
  KEY88_g,			/*	SDLK_g	0x67 ('g')	*/
  KEY88_h,			/*	SDLK_h	0x68 ('h')	*/
  KEY88_i,			/*	SDLK_i	0x69 ('i')	*/
  KEY88_j,			/*	SDLK_j	0x6A ('j')	*/
  KEY88_k,			/*	SDLK_k	0x6B ('k')	*/
  KEY88_l,			/*	SDLK_l	0x6C ('l')	*/
  KEY88_m,			/*	SDLK_m	0x6D ('m')	*/
  KEY88_n,			/*	SDLK_n	0x6E ('n')	*/
  KEY88_o,			/*	SDLK_o	0x6F ('o')	*/
  KEY88_p,			/*	SDLK_p	0x70 ('p')	*/
  KEY88_q,			/*	SDLK_q	0x71 ('q')	*/
  KEY88_r,			/*	SDLK_r	0x72 ('r')	*/
  KEY88_s,			/*	SDLK_s	0x73 ('s')	*/
  KEY88_t,			/*	SDLK_t	0x74 ('t')	*/
  KEY88_u,			/*	SDLK_u	0x75 ('u')	*/
  KEY88_v,			/*	SDLK_v	0x76 ('v')	*/
  KEY88_w,			/*	SDLK_w	0x77 ('w')	*/
  KEY88_x,			/*	SDLK_x	0x78 ('x')	*/
  KEY88_y,			/*	SDLK_y	0x79 ('y')	*/
  KEY88_z,			/*	SDLK_z	0x7A ('z')	*/
  0, 0, 0, 0,
  KEY88_DEL,		/*	SDLK_DELETE	0x7F ('\177')	*/
  KEY88_CAPS,		/*	SDLK_CAPSLOCK	40000039	ここからunicode*/
  KEY88_F1,			/*	SDLK_F1	4000003A	*/
  KEY88_F2,			/*	SDLK_F2	4000003B	*/
  KEY88_F3,			/*	SDLK_F3	4000003C	*/
  KEY88_F4,			/*	SDLK_F4	4000003D	*/
  KEY88_F5,			/*	SDLK_F5	4000003E	*/
  KEY88_F6,			/*	SDLK_F6	4000003F	*/
  KEY88_F7,			/*	SDLK_F7	40000040	*/
  KEY88_F8,			/*	SDLK_F8	40000041	*/
  KEY88_F9,			/*	SDLK_F9	40000042	*/
  KEY88_F10,		/*	SDLK_F10	40000043	*/
  KEY88_F11,		/*	SDLK_F11	40000044	*/
  KEY88_F12,		/*	SDLK_F12	40000045	*/
  KEY88_COPY,		/*	SDLK_PRINTSCREEN	40000046	*/
  KEY88_KANA,		/*	SDLK_SCROLLLOCK	40000047	*/
  KEY88_STOP,		/*	SDLK_PAUSE	40000048	*/
  KEY88_INS,		/*	SDLK_INSERT	40000049	*/
  KEY88_HOME,		/*	SDLK_HOME	4000004A	*/
  KEY88_ROLLDOWN,	/*	SDLK_PAGEUP	4000004B	*/
  0,
  0,				/*	SDLK_END	4000004D	*/
  KEY88_ROLLUP,		/*	SDLK_PAGEDOWN	4000004E	*/
  KEY88_RIGHT,		/*	SDLK_RIGHT	4000004F	*/
  KEY88_LEFT,		/*	SDLK_LEFT	40000050	*/
  KEY88_DOWN,		/*	SDLK_DOWN	40000051	*/
  KEY88_UP,			/*	SDLK_UP	40000052	*/
  0,				/*	SDLK_NUMLOCKCLEAR	40000053	*/
  KEY88_KP_DIVIDE,	/*	SDLK_KP_DIVIDE	40000054	*/
  KEY88_KP_MULTIPLY,/*	SDLK_KP_MULTIPLY	40000055	*/
  KEY88_KP_SUB,		/*	SDLK_KP_MINUS	40000056	*/
  KEY88_KP_ADD,		/*	SDLK_KP_PLUS	40000057	*/
  KEY88_RETURNR,	/*	SDLK_KP_ENTER	40000058	*/
  KEY88_KP_1,		/*	SDLK_KP_1	40000059	*/
  KEY88_KP_2,		/*	SDLK_KP_2	4000005A	*/
  KEY88_KP_3,		/*	SDLK_KP_3	4000005B	*/
  KEY88_KP_4,		/*	SDLK_KP_4	4000005C	*/
  KEY88_KP_5,		/*	SDLK_KP_5	4000005D	*/
  KEY88_KP_6,		/*	SDLK_KP_6	4000005E	*/
  KEY88_KP_7,		/*	SDLK_KP_7	4000005F	*/
  KEY88_KP_8,		/*	SDLK_KP_8	40000060	*/
  KEY88_KP_9,		/*	SDLK_KP_9	40000061	*/
  KEY88_KP_0,		/*	SDLK_KP_0	40000062	*/
  KEY88_KP_PERIOD,	/*	SDLK_KP_PERIOD	40000063	*/
  0,
  0,				/*	SDLK_APPLICATION	40000065	*/
  0,				/*	SDLK_POWER	40000066	*/
  KEY88_KP_EQUAL,	/*	SDLK_KP_EQUALS	40000067	*/
  KEY88_F13,		/*	SDLK_F13	40000068	*/
  KEY88_F14,		/*	SDLK_F14	40000069	*/
  KEY88_F15,		/*	SDLK_F15	4000006A	*/
  KEY88_F16,		/*	SDLK_F16	4000006B	*/
  KEY88_F17,		/*	SDLK_F17	4000006C	*/
  KEY88_F18,		/*	SDLK_F18	4000006D	*/
  KEY88_F19,		/*	SDLK_F19	4000006E	*/
  KEY88_F20,		/*	SDLK_F20	4000006F	*/
  0,				/*	SDLK_F21	40000070	*/
  0,				/*	SDLK_F22	40000071	*/
  0,				/*	SDLK_F23	40000072	*/
  0,				/*	SDLK_F24	40000073	*/
  0,				/*	SDLK_EXECUTE	40000074	*/
  KEY88_HELP,		/*	SDLK_HELP	40000075	*/
  0,				/*	SDLK_MENU	40000076	*/
  0,				/*	SDLK_SELECT	40000077	*/
  KEY88_STOP,		/*	SDLK_STOP	40000078	*/
  0,				/*	SDLK_AGAIN	40000079	*/
  0,				/*	SDLK_UNDO	4000007A	*/
  0,				/*	SDLK_CUT	4000007B	*/
  0,				/*	SDLK_COPY	4000007C	*/
  0,				/*	SDLK_PASTE	4000007D	*/
  0,				/*	SDLK_FIND	4000007E	*/
  0,				/*	SDLK_MUTE	4000007F	*/
  0,				/*	SDLK_VOLUMEUP	40000080	*/
  0,				/*	SDLK_VOLUMEDOWN	40000081	*/
  0, 0, 0,
  0,				/*	SDLK_KP_COMMA	40000085	*/
  0,				/*	SDLK_KP_EQUALSAS400	40000086	*/
  0, 0, 0, 0, 0, 0, 0, 0, 0,
  0, 0, 0, 0, 0, 0, 0, 0, 0,
  0,				/*	SDLK_ALTERASE	40000099	*/
  0,				/*	SDLK_SYSREQ	4000009A	*/
  0,				/*	SDLK_CANCEL	4000009B	*/
  0,				/*	SDLK_CLEAR	4000009C	*/
  0,				/*	SDLK_PRIOR	4000009D	*/
  0,				/*	SDLK_RETURN2	4000009E	*/
  0,				/*	SDLK_SEPARATOR	4000009F	*/
  0,				/*	SDLK_OUT	400000A0	*/
  0,				/*	SDLK_OPER	400000A1	*/
  0,				/*	SDLK_CLEARAGAIN	400000A2	*/
  0,				/*	SDLK_CRSEL	400000A3	*/
  0,				/*	SDLK_EXSEL	400000A4	*/
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  0,				/*	SDLK_KP_00	400000B0	*/
  0,				/*	SDLK_KP_000	400000B1	*/
  0,				/*	SDLK_THOUSANDSSEPARATOR	400000B2	*/
  0,				/*	SDLK_DECIMALSEPARATOR	400000B3	*/
  0,				/*	SDLK_CURRENCYUNIT	400000B4	*/
  0,				/*	SDLK_CURRENCYSUBUNIT	400000B5	*/
  0,				/*	SDLK_KP_LEFTPAREN	400000B6	*/
  0,				/*	SDLK_KP_RIGHTPAREN	400000B7	*/
  0,				/*	SDLK_KP_LEFTBRACE	400000B8	*/
  0,				/*	SDLK_KP_RIGHTBRACE	400000B9	*/
  0,				/*	SDLK_KP_TAB	400000BA	*/
  0,				/*	SDLK_KP_BACKSPACE	400000BB	*/
  0,				/*	SDLK_KP_A	400000BC	*/
  0,				/*	SDLK_KP_B	400000BD	*/
  0,				/*	SDLK_KP_C	400000BE	*/
  0,				/*	SDLK_KP_D	400000BF	*/
  0,				/*	SDLK_KP_E	400000C0	*/
  0,				/*	SDLK_KP_F	400000C1	*/
  0,				/*	SDLK_KP_XOR	400000C2	*/
  0,				/*	SDLK_KP_POWER	400000C3	*/
  0,				/*	SDLK_KP_PERCENT	400000C4	*/
  0,				/*	SDLK_KP_LESS	400000C5	*/
  0,				/*	SDLK_KP_GREATER	400000C6	*/
  0,				/*	SDLK_KP_AMPERSAND	400000C7	*/
  0,				/*	SDLK_KP_DBLAMPERSAND	400000C8	*/
  0,				/*	SDLK_KP_VERTICALBAR	400000C9	*/
  0,				/*	SDLK_KP_DBLVERTICALBAR	400000CA	*/
  0,				/*	SDLK_KP_COLON	400000CB	*/
  0,				/*	SDLK_KP_HASH	400000CC	*/
  0,				/*	SDLK_KP_SPACE	400000CD	*/
  0,				/*	SDLK_KP_AT	400000CE	*/
  0,				/*	SDLK_KP_EXCLAM	400000CF	*/
  0,				/*	SDLK_KP_MEMSTORE	400000D0	*/
  0,				/*	SDLK_KP_MEMRECALL	400000D1	*/
  0,				/*	SDLK_KP_MEMCLEAR	400000D2	*/
  0,				/*	SDLK_KP_MEMADD	400000D3	*/
  0,				/*	SDLK_KP_MEMSUBTRACT	400000D4	*/
  0,				/*	SDLK_KP_MEMMULTIPLY	400000D5	*/
  0,				/*	SDLK_KP_MEMDIVIDE	400000D6	*/
  0,				/*	SDLK_KP_PLUSMINUS	400000D7	*/
  0,				/*	SDLK_KP_CLEAR	400000D8	*/
  0,				/*	SDLK_KP_CLEARENTRY	400000D9	*/
  0,				/*	SDLK_KP_BINARY	400000DA	*/
  0,				/*	SDLK_KP_OCTAL	400000DB	*/
  0,				/*	SDLK_KP_DECIMAL	400000DC	*/
  0,				/*	SDLK_KP_HEXADECIMAL	400000DD	*/
  0, 0,
  KEY88_CTRL,		/*	SDLK_LCTRL	400000E0	*/
  KEY88_SHIFTL,		/*	SDLK_LSHIFT	400000E1	*/
  KEY88_GRAPH,		/*	SDLK_LALT	400000E2	*/
  0,				/*	SDLK_LGUI	400000E3	*/
  KEY88_CTRL,		/*	SDLK_RCTRL	400000E4	*/
  KEY88_SHIFTR,		/*	SDLK_RSHIFT	400000E5	*/
  KEY88_GRAPH,		/*	SDLK_RALT	400000E6	*/
  0,				/*	SDLK_RGUI	400000E7	*/
  0, 0, 0, 0, 0, 0, 0, 0,
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  0,
  0,		/*	SDLK_MODE	40000101	*/
  0,		/*	SDLK_AUDIONEXT	40000102	*/
  0,		/*	SDLK_AUDIOPREV	40000103	*/
  0,		/*	SDLK_AUDIOSTOP	40000104	*/
  0,		/*	SDLK_AUDIOPLAY	40000105	*/
  0,		/*	SDLK_AUDIOMUTE	40000106	*/
  0,		/*	SDLK_MEDIASELECT	40000107	*/
  0,		/*	SDLK_WWW	40000108	*/
  0,		/*	SDLK_MAIL	40000109	*/
  0,		/*	SDLK_CALCULATOR	4000010A	*/
  0,		/*	SDLK_COMPUTER	4000010B	*/
  0,		/*	SDLK_AC_SEARCH	4000010C	*/
  0,		/*	SDLK_AC_HOME	4000010D	*/
  0,		/*	SDLK_AC_BACK	4000010E	*/
  0,		/*	SDLK_AC_FORWARD	4000010F	*/
  0,		/*	SDLK_AC_STOP	40000110	*/
  0,		/*	SDLK_AC_REFRESH	40000111	*/
  0,		/*	SDLK_AC_BOOKMARKS	40000112	*/
  0,		/*	SDLK_BRIGHTNESSDOWN	40000113	*/
  0,		/*	SDLK_BRIGHTNESSUP	40000114	*/
  0,		/*	SDLK_DISPLAYSWITCH	40000115	*/
  0,		/*	SDLK_KBDILLUMTOGGLE	40000116	*/
  0,		/*	SDLK_KBDILLUMDOWN	40000117	*/
  0,		/*	SDLK_KBDILLUMUP	40000118	*/
  0,		/*	SDLK_EJECT	40000119	*/
  0,		/*	SDLK_SLEEP	4000011A	*/

};



/*----------------------------------------------------------------------
 * keysym2key88[]   の初期値は、keysym2key88_default[] と同じ、
 * scancode2key88[] の初期値は、全て -1 (未使用) であるが、
 * キーボードの種類に応じて、keysym2key88[] と scancode2key88[] の一部を
 * 変更することにする。以下は、その変更の情報。
 *----------------------------------------------------------------------*/

static const T_REMAPPING remapping_x11_106[] =
{
    {	KEYCODE_SYM,  SDLK_OPER,	KEY88_KANA,	    },
    {	KEYCODE_SYM,  SDLK_RALT,	KEY88_ZENKAKU,	    },
/*  {	KEYCODE_SYM,  SDLK_RCTRL,	KEY88_UNDERSCORE,   },*/
    {	KEYCODE_SYM,  SDLK_MENU,	KEY88_SYS_MENU,     },
    {	KEYCODE_SCAN,     49,		KEY88_ZENKAKU,	    },   /* 半角全角 */
    {	KEYCODE_SCAN,    133,		KEY88_YEN,	    },   /* \ |      */
    {	KEYCODE_SCAN,    123,		KEY88_UNDERSCORE,   },   /* \ _ ロ   */
    {	KEYCODE_SCAN,    131,		KEY88_KETTEI,	    },
    {	KEYCODE_SCAN,    129,		KEY88_HENKAN,	    },
    {	KEYCODE_SCAN,    120,		KEY88_KANA,	    },   /* カタひら */
    {	KEYCODE_INVALID, 0,		0,		    },
};

static const T_REMAPPING remapping_x11_101[] =
{
    {	KEYCODE_SYM,  SDLK_BACKQUOTE,	KEY88_YEN,	    },
    {	KEYCODE_SYM,  SDLK_EQUALS,	KEY88_CARET,	    },
    {	KEYCODE_SYM,  SDLK_BACKSLASH,	KEY88_AT,	    },
    {	KEYCODE_SYM,  SDLK_QUOTE,	KEY88_COLON,	    },
    {	KEYCODE_SYM,  SDLK_OPER,	KEY88_KANA,	    },
    {	KEYCODE_SYM,  SDLK_RALT,	KEY88_ZENKAKU,	    },
    {	KEYCODE_SYM,  SDLK_RCTRL,	KEY88_UNDERSCORE,   },
    {	KEYCODE_SYM,  SDLK_MENU,	KEY88_SYS_MENU,     },
    {	KEYCODE_INVALID, 0,		0,		    },
};

static const T_REMAPPING remapping_windib_106[] =
{
    {	KEYCODE_SYM,  SDLK_BACKQUOTE,	0,		    },   /* 半角全角 */
    {	KEYCODE_SYM,  SDLK_EQUALS,	KEY88_CARET,	    },   /* ^        */
    {	KEYCODE_SYM,  SDLK_LEFTBRACKET,	KEY88_AT,	    },   /* @        */
    {	KEYCODE_SYM,  SDLK_RIGHTBRACKET,KEY88_BRACKETLEFT,  },   /* [        */
    {	KEYCODE_SYM,  SDLK_QUOTE,	KEY88_COLON,	    },   /* :        */
    {	KEYCODE_SYM,  SDLK_OPER,	KEY88_KANA,	    },   /* 左Window */
    {	KEYCODE_SYM,  SDLK_RALT,	KEY88_ZENKAKU,	    },   /* 右Alt    */
/*  {	KEYCODE_SYM,  SDLK_RCTRL,	KEY88_UNDERSCORE,   },*/ /* 右Ctrl   */
    {	KEYCODE_SYM,  SDLK_MENU,	KEY88_SYS_MENU,     },   /* Menu     */
    {	KEYCODE_SCAN,    125,		KEY88_YEN,	    },   /* \ |      */
    {	KEYCODE_SCAN,     43,		KEY88_BRACKETRIGHT, },   /* ] }      */
    {	KEYCODE_SCAN,    115,		KEY88_UNDERSCORE,   },   /* \ _ ロ   */
    {	KEYCODE_SCAN,    123,		KEY88_KETTEI,	    },   /* 無変換   */
    {	KEYCODE_SCAN,    121,		KEY88_HENKAN,	    },   /* 変換     */
/*  {	KEYCODE_SCAN,    112,		0,		    },*/ /* カタひら */
    {	KEYCODE_INVALID, 0,		0,		    },
};

static const T_REMAPPING remapping_windib_101[] =
{
    {	KEYCODE_SYM,  SDLK_BACKQUOTE,	KEY88_YEN,	    },   /* `        */
    {	KEYCODE_SYM,  SDLK_EQUALS,	KEY88_CARET,	    },   /* =        */
    {	KEYCODE_SYM,  SDLK_BACKSLASH,	KEY88_AT,	    },   /* \        */
    {	KEYCODE_SYM,  SDLK_QUOTE,	KEY88_COLON,	    },   /* '        */
    {	KEYCODE_SYM,  SDLK_OPER,	KEY88_KANA,	    },   /* 左Window */
    {	KEYCODE_SYM,  SDLK_RALT,	KEY88_ZENKAKU,	    },   /* 右Alt    */
    {	KEYCODE_SYM,  SDLK_RCTRL,	KEY88_UNDERSCORE,   },   /* 右Ctrl   */
    {	KEYCODE_SYM,  SDLK_MENU,	KEY88_SYS_MENU,     },   /* Menu     */
    {	KEYCODE_INVALID, 0,		0,		    },
};

static const T_REMAPPING remapping_directx_106[] =
{
    {	KEYCODE_SYM,  SDLK_BACKSLASH,	KEY88_UNDERSCORE,   },   /* \ _ ロ   */
    {	KEYCODE_SYM,  SDLK_OPER,	KEY88_KANA,	    },   /* 左Window */
    {	KEYCODE_SYM,  SDLK_RALT,	KEY88_ZENKAKU,	    },   /* 右Alt    */
/*  {	KEYCODE_SYM,  SDLK_RCTRL,	KEY88_UNDERSCORE,   },*/ /* 右Ctrl   */
    {	KEYCODE_SYM,  SDLK_MENU,	KEY88_SYS_MENU,     },   /* Menu     */
/*  {	KEYCODE_SCAN,    148,		0,		    },*/ /* 半角全角 */
    {	KEYCODE_SCAN,    144,		KEY88_CARET,	    },   /* ^        */
    {	KEYCODE_SCAN,    125,		KEY88_YEN,	    },   /* \        */
    {	KEYCODE_SCAN,    145,		KEY88_AT,	    },   /* @        */
    {	KEYCODE_SCAN,    146,		KEY88_COLON,	    },   /* :        */
    {	KEYCODE_SCAN,    123,		KEY88_KETTEI,	    },   /* 無変換   */
    {	KEYCODE_SCAN,    121,		KEY88_HENKAN,	    },   /* 変換     */
    {	KEYCODE_SCAN,    112,		KEY88_KANA,	    },   /* カタひら */
    {	KEYCODE_INVALID, 0,		0,		    },
};

static const T_REMAPPING remapping_directx_101[] =
{
    {	KEYCODE_SYM,  SDLK_BACKQUOTE,	KEY88_YEN,	    },   /* `        */
    {	KEYCODE_SYM,  SDLK_EQUALS,	KEY88_CARET,	    },   /* =        */
    {	KEYCODE_SYM,  SDLK_BACKSLASH,	KEY88_AT,	    },   /* \        */
    {	KEYCODE_SYM,  SDLK_QUOTE,	KEY88_COLON,	    },   /* '        */
    {	KEYCODE_SYM,  SDLK_OPER,	KEY88_KANA,	    },   /* 左Window */
    {	KEYCODE_SYM,  SDLK_RALT,	KEY88_ZENKAKU,	    },   /* 右Alt    */
    {	KEYCODE_SYM,  SDLK_RCTRL,	KEY88_UNDERSCORE,   },   /* 右Ctrl   */
    {	KEYCODE_SYM,  SDLK_MENU,	KEY88_SYS_MENU,     },   /* Menu     */
    {	KEYCODE_SCAN,    148,		KEY88_YEN,	    },
    {	KEYCODE_SCAN,    144,		KEY88_CARET,	    },
    {	KEYCODE_SCAN,    145,		KEY88_AT,	    },
    {	KEYCODE_SCAN,    146,		KEY88_COLON,	    },
    {	KEYCODE_SCAN,    125,		KEY88_YEN,	    },
    {	KEYCODE_INVALID, 0,		0,		    },
};

static const T_REMAPPING remapping_toolbox_106[] =
{
    {	KEYCODE_SYM,  SDLK_OPER,	KEY88_SYS_MENU,	    },
    {	KEYCODE_SYM,  SDLK_OPER,	KEY88_SYS_MENU,	    },
    {	KEYCODE_SCAN,    95,		KEY88_KP_COMMA,	    },
/*  {	KEYCODE_SCAN,    102,		0,		    },*/ /* 英数     */
/*  {	KEYCODE_SCAN,    104,		0,		    },*/ /* カナ     */
    {	KEYCODE_INVALID, 0,		0,		    },
};

static const T_REMAPPING remapping_toolbox_101[] =
{
    {	KEYCODE_SYM,  SDLK_OPER,	KEY88_SYS_MENU,	    },
    {	KEYCODE_SYM,  SDLK_OPER,	KEY88_SYS_MENU,	    },
    {	KEYCODE_SYM,  SDLK_BACKQUOTE,	KEY88_YEN,	    },
    {	KEYCODE_SYM,  SDLK_EQUALS,	KEY88_CARET,	    },
    {	KEYCODE_SYM,  SDLK_BACKSLASH,	KEY88_AT,	    },
    {	KEYCODE_SYM,  SDLK_QUOTE,	KEY88_COLON,	    },
    {	KEYCODE_INVALID, 0,		0,		    },
};

static const T_REMAPPING remapping_dummy[] =
{
    {	KEYCODE_INVALID, 0,		0,		    },
};



/*----------------------------------------------------------------------
 * ソフトウェア NumLock オン時の キーコード変換情報 (デフォルト)
 *----------------------------------------------------------------------*/

static const T_BINDING binding_106[] =
{
    {	KEYCODE_SYM,	SDLK_5,		KEY88_HOME,		0,	},
    {	KEYCODE_SYM,	SDLK_6,		KEY88_HELP,		0,	},
    {	KEYCODE_SYM,	SDLK_7,		KEY88_KP_7,		0,	},
    {	KEYCODE_SYM,	SDLK_8,		KEY88_KP_8,		0,	},
    {	KEYCODE_SYM,	SDLK_9,		KEY88_KP_9,		0,	},
    {	KEYCODE_SYM,	SDLK_0,		KEY88_KP_MULTIPLY,	0,	},
    {	KEYCODE_SYM,	SDLK_MINUS,	KEY88_KP_SUB,		0,	},
    {	KEYCODE_SYM,	SDLK_CARET,	KEY88_KP_DIVIDE,	0,	},
    {	KEYCODE_SYM,	SDLK_u,		KEY88_KP_4,		0,	},
    {	KEYCODE_SYM,	SDLK_i,		KEY88_KP_5,		0,	},
    {	KEYCODE_SYM,	SDLK_o,		KEY88_KP_6,		0,	},
    {	KEYCODE_SYM,	SDLK_p,		KEY88_KP_ADD,		0,	},
    {	KEYCODE_SYM,	SDLK_j,		KEY88_KP_1,		0,	},
    {	KEYCODE_SYM,	SDLK_k,		KEY88_KP_2,		0,	},
    {	KEYCODE_SYM,	SDLK_l,		KEY88_KP_3,		0,	},
    {	KEYCODE_SYM,	SDLK_SEMICOLON,	KEY88_KP_EQUAL,		0,	},
    {	KEYCODE_SYM,	SDLK_m,		KEY88_KP_0,		0,	},
    {	KEYCODE_SYM,	SDLK_COMMA,	KEY88_KP_COMMA,		0,	},
    {	KEYCODE_SYM,	SDLK_PERIOD,	KEY88_KP_PERIOD,	0,	},
    {	KEYCODE_SYM,	SDLK_SLASH,	KEY88_RETURNR,		0,	},
    {	KEYCODE_INVALID,0,		0,			0,	},
};

static const T_BINDING binding_101[] =
{
    {	KEYCODE_SYM,	SDLK_5,		KEY88_HOME,		0,	},
    {	KEYCODE_SYM,	SDLK_6,		KEY88_HELP,		0,	},
    {	KEYCODE_SYM,	SDLK_7,		KEY88_KP_7,		0,	},
    {	KEYCODE_SYM,	SDLK_8,		KEY88_KP_8,		0,	},
    {	KEYCODE_SYM,	SDLK_9,		KEY88_KP_9,		0,	},
    {	KEYCODE_SYM,	SDLK_0,		KEY88_KP_MULTIPLY,	0,	},
    {	KEYCODE_SYM,	SDLK_MINUS,	KEY88_KP_SUB,		0,	},
    {	KEYCODE_SYM,	SDLK_EQUALS,	KEY88_KP_DIVIDE,	0,	},
    {	KEYCODE_SYM,	SDLK_u,		KEY88_KP_4,		0,	},
    {	KEYCODE_SYM,	SDLK_i,		KEY88_KP_5,		0,	},
    {	KEYCODE_SYM,	SDLK_o,		KEY88_KP_6,		0,	},
    {	KEYCODE_SYM,	SDLK_p,		KEY88_KP_ADD,		0,	},
    {	KEYCODE_SYM,	SDLK_j,		KEY88_KP_1,		0,	},
    {	KEYCODE_SYM,	SDLK_k,		KEY88_KP_2,		0,	},
    {	KEYCODE_SYM,	SDLK_l,		KEY88_KP_3,		0,	},
    {	KEYCODE_SYM,	SDLK_SEMICOLON,	KEY88_KP_EQUAL,		0,	},
    {	KEYCODE_SYM,	SDLK_m,		KEY88_KP_0,		0,	},
    {	KEYCODE_SYM,	SDLK_COMMA,	KEY88_KP_COMMA,		0,	},
    {	KEYCODE_SYM,	SDLK_PERIOD,	KEY88_KP_PERIOD,	0,	},
    {	KEYCODE_SYM,	SDLK_SLASH,	KEY88_RETURNR,		0,	},
    {	KEYCODE_INVALID,0,		0,			0,	},
};

static const T_BINDING binding_directx[] =
{
    {	KEYCODE_SYM,	SDLK_5,		KEY88_HOME,		0,	},
    {	KEYCODE_SYM,	SDLK_6,		KEY88_HELP,		0,	},
    {	KEYCODE_SYM,	SDLK_7,		KEY88_KP_7,		0,	},
    {	KEYCODE_SYM,	SDLK_8,		KEY88_KP_8,		0,	},
    {	KEYCODE_SYM,	SDLK_9,		KEY88_KP_9,		0,	},
    {	KEYCODE_SYM,	SDLK_0,		KEY88_KP_MULTIPLY,	0,	},
    {	KEYCODE_SYM,	SDLK_MINUS,	KEY88_KP_SUB,		0,	},
    {	KEYCODE_SYM,	SDLK_EQUALS,	KEY88_KP_DIVIDE,	0,	},
    {	KEYCODE_SCAN,	144,		KEY88_KP_DIVIDE,	0,	},
    {	KEYCODE_SYM,	SDLK_u,		KEY88_KP_4,		0,	},
    {	KEYCODE_SYM,	SDLK_i,		KEY88_KP_5,		0,	},
    {	KEYCODE_SYM,	SDLK_o,		KEY88_KP_6,		0,	},
    {	KEYCODE_SYM,	SDLK_p,		KEY88_KP_ADD,		0,	},
    {	KEYCODE_SYM,	SDLK_j,		KEY88_KP_1,		0,	},
    {	KEYCODE_SYM,	SDLK_k,		KEY88_KP_2,		0,	},
    {	KEYCODE_SYM,	SDLK_l,		KEY88_KP_3,		0,	},
    {	KEYCODE_SYM,	SDLK_SEMICOLON,	KEY88_KP_EQUAL,		0,	},
    {	KEYCODE_SYM,	SDLK_m,		KEY88_KP_0,		0,	},
    {	KEYCODE_SYM,	SDLK_COMMA,	KEY88_KP_COMMA,		0,	},
    {	KEYCODE_SYM,	SDLK_PERIOD,	KEY88_KP_PERIOD,	0,	},
    {	KEYCODE_SYM,	SDLK_SLASH,	KEY88_RETURNR,		0,	},
    {	KEYCODE_INVALID,0,		0,			0,	},
};



/******************************************************************************
 * イベントハンドリング
 *
 *	1/60毎に呼び出される。
 *****************************************************************************/
static	int	joystick_init(void);
static	void	joystick_exit(void);
static	int	analyze_keyconf_file(void);

static	char	video_driver[32];

/*
 * これは 起動時に1回だけ呼ばれる
 */
void	event_init(void)
{
    const T_REMAPPING *map;
    const T_BINDING   *bin;
    int i;

    /* ジョイスティック初期化 */

    if (use_joydevice) {
	if (verbose_proc) printf("Initializing Joystick System ... ");
	i = joystick_init();
	if (verbose_proc) {
	    if (i) printf("OK (found %d joystick(s))\n", i);
	    else   printf("FAILED\n");
	}
    }


    /* キーマッピング初期化 */

    //if (SDL_VideoDriverName(video_driver, sizeof(video_driver)) == NULL) {
	//memset(video_driver, 0, sizeof(video_driver));
    //}

    memset(keysym2key88, 0, sizeof(keysym2key88));
    for (i=0; i<COUNTOF(scancode2key88); i++) {
	scancode2key88[ i ] = -1;
    }
    memset(binding, 0, sizeof(binding));

	if (verbose_proc) { printf("キーマッピング初期化 %d\n",keyboard_type); }

    switch (keyboard_type) {

    case 0:					/* デフォルトキーボード */
	if (analyze_keyconf_file()) {
	    ;
	} else {
	    memcpy(keysym2key88,
		   keysym2key88_default, sizeof(keysym2key88_default));
	    memcpy(binding,
		   binding_106, sizeof(binding_106));
	}
	break;


    case 1:					/* 106日本語キーボード */
    case 2:					/* 101英語キーボード ? */
	memcpy(keysym2key88,
	       keysym2key88_default, sizeof(keysym2key88_default));

#if	defined(QUASI88_FUNIX)

	if (keyboard_type == 1) {
	    map = remapping_x11_106;
	    bin = binding_106;
	} else {
	    map = remapping_x11_101;
	    bin = binding_101;
	}

#elif	defined(QUASI88_FWIN)

	if (strcmp(video_driver, "directx") == 0) {
	    if (keyboard_type == 1) map = remapping_directx_106;
	    else                    map = remapping_directx_101;
	    bin = binding_directx;
	} else {
	    if (keyboard_type == 1) map = remapping_windib_106;
	    else                    map = remapping_windib_101;
	    bin = binding_101;
	}

#elif	defined(QUASI88_FMAC)

	if (keyboard_type == 1) {
	    map = remapping_toolbox_106;
	    bin = binding_106;
	} else {
	    map = remapping_toolbox_101;
	    bin = binding_101;
	}

	if (use_cmdkey == FALSE) {
	    map += 2;
	}

#else
	map = remapping_dummy;
	bin = binding_106;
#endif


	/*転送開始*/
	for ( ; map->type; map ++) {
		if (map->type == KEYCODE_SYM) { /*==keysym転送==*/
		 if(map->code < 0x40000000){ /*unicode対応*/
			keysym2key88[ map->code ] = map->key88;
		 }
		 else{
			keysym2key88[ map->code - 0x40000039 + 128 ] = map->key88;
		 }
		}
		if (map->type == KEYCODE_SCAN) { /*==keycode転送==*/
			scancode2key88[ map->code ] = map->key88;
		}
	}


	if (verbose_proc) { printf("MAP初期化\n"); }

	for (i=0; i<COUNTOF(binding); i++) {
	    if (bin->type == KEYCODE_INVALID) break;

	    binding[ i ].type      = bin->type;
	    binding[ i ].code      = bin->code;
	    binding[ i ].org_key88 = bin->org_key88;
	    binding[ i ].new_key88 = bin->new_key88;
	    bin ++;
	}
	break;
    }


	if (verbose_proc) { printf("key88初期化\n"); }

    /* ソフトウェアNumLock 時のキー差し替えの準備 */

    for (i=0; i<COUNTOF(binding); i++) {

	if        (binding[i].type == KEYCODE_SYM) {

	    binding[i].org_key88 = keysym2key88[ binding[i].code ];

	} else if (binding[i].type == KEYCODE_SCAN) {

	    binding[i].org_key88 = scancode2key88[ binding[i].code ];

	} else {
	    break;
	}
    }

	if (verbose_proc) { printf("キーマッピング初期化終了\n"); }

    /* test */
    if (show_fps) {
	if (display_fps_init() == FALSE) {
	    show_fps = FALSE;
	}
    }


}

/*---------------------------
 * 約 1/60 毎に呼ばれるイベント
 --------------------------*/
void	event_update(void)
{
    SDL_Event E;
    int    keysym;
    int    key88, x, y;


    SDL_PumpEvents();		/* イベントを汲み上げる */


	while (SDL_PeepEvents(&E, 1, SDL_GETEVENT,SDL_FIRSTEVENT,SDL_LASTEVENT)){

	switch (E.type) {

	case SDL_KEYDOWN:	/*------------------------------------------*/
	case SDL_KEYUP:

	    keysym  = E.key.keysym.sym;
	    if(keysym > 0x40000000){		/*SDL2はkeysymはunicode*/
	     keysym = (keysym - 0x40000039 + 128);
	    }
	    /*if (verbose_proc) printf("keysym=%08X -> %08X\n",E.key.keysym.sym,keysym);*/

		/* scancode2key88[] が定義済なら、そのキーコードを優先する */
		if (E.key.keysym.scancode < COUNTOF(scancode2key88) &&
		    scancode2key88[ E.key.keysym.scancode ] >= 0) {

		    key88 = scancode2key88[ E.key.keysym.scancode ];

		} else {
		    key88 = keysym2key88[ keysym ];
		}

	    quasi88_key(key88, (E.type == SDL_KEYDOWN));

	    break;

	case SDL_MOUSEMOTION:	/*------------------------------------------*/
	    if (sdl_mouse_rel_move) {	/* マウスがウインドウの端に届いても */
					/* 相対的な動きを検出できる場合     */
		x = E.motion.xrel;
		y = E.motion.yrel;

		quasi88_mouse_moved_rel(x, y);

	    } else {

		x = E.motion.x;
		y = E.motion.y;

		quasi88_mouse_moved_abs(x, y);
	    }
	    break;

	case SDL_MOUSEBUTTONDOWN:/*------------------------------------------*/
	case SDL_MOUSEBUTTONUP:
	    /* マウス移動イベントも同時に処理する必要があるなら、
	       quasi88_mouse_moved_abs/rel 関数をここで呼び出しておく */

	    switch (E.button.button) {
	    case SDL_BUTTON_LEFT:	key88 = KEY88_MOUSE_L;		break;
	    case SDL_BUTTON_MIDDLE:	key88 = KEY88_MOUSE_M;		break;
	    case SDL_BUTTON_RIGHT:	key88 = KEY88_MOUSE_R;		break;
	    default:			key88 = 0;			break;
	    }
	    if (key88) {
		quasi88_mouse(key88, (E.type == SDL_MOUSEBUTTONDOWN));
	    }
	    break;

	case SDL_MOUSEWHEEL:
	    /* マウス移動イベントも同時に処理する必要があるなら、
	       quasi88_mouse_moved_abs/rel 関数をここで呼び出しておく */
        if(E.wheel.y > 0) // 上へスクロール
        {
             key88 = KEY88_MOUSE_WUP;
        }
        else if(E.wheel.y < 0) // 下へスクロール
        {
             key88 = KEY88_MOUSE_WDN;
        }

        if(E.wheel.x > 0) // 右へスクロール
        {
             // ...
        }
        else if(E.wheel.x < 0) // 左へスクロール
        {
             // ...
        }

	    if (key88) {
		quasi88_mouse(key88, (E.type == SDL_MOUSEWHEEL));
	    }
	    break;

	case SDL_JOYAXISMOTION:	/*------------------------------------------*/
	    /*if (verbose_proc) printf("Joy xy %d %d %d\n",E.jaxis.which,E.jaxis.axis,E.jaxis.value);*/

	    if (E.jbutton.which < JOY_MAX &&
		joy_info[E.jbutton.which].dev != NULL) {

		int now, chg;
		T_JOY_INFO *joy = &joy_info[E.jbutton.which];
		int offset = (joy->num) * KEY88_PAD_OFFSET;

		if (E.jaxis.axis == 0) {	/* 左右方向 */

		    now = joy->axis & ~(AXIS_L|AXIS_R);

		    if      (E.jaxis.value < -0x4000) now |= AXIS_L;
		    else if (E.jaxis.value >  0x4000) now |= AXIS_R;

		    chg = joy->axis ^ now;
		    if (chg & AXIS_L) {
			quasi88_pad(KEY88_PAD1_LEFT + offset,  (now & AXIS_L));
		    }
		    if (chg & AXIS_R) {
			quasi88_pad(KEY88_PAD1_RIGHT + offset, (now & AXIS_R));
		    }

		} else {			/* 上下方向 */

		    now = joy->axis & ~(AXIS_U|AXIS_D);

		    if      (E.jaxis.value < -0x4000) now |= AXIS_U;
		    else if (E.jaxis.value >  0x4000) now |= AXIS_D;

		    chg = joy->axis ^ now;
		    if (chg & AXIS_U) {
			quasi88_pad(KEY88_PAD1_UP + offset,   (now & AXIS_U));
		    }
		    if (chg & AXIS_D) {
			quasi88_pad(KEY88_PAD1_DOWN + offset, (now & AXIS_D));
		    }
		}
		joy->axis = now;
	    }
	    break;

	case SDL_JOYBUTTONDOWN:	/*------------------------------------------*/
	case SDL_JOYBUTTONUP:
	    /*if (verbose_proc) printf("JoyButton %d %d\n",E.jbutton.which,E.jbutton.button);*/

	    if (E.jbutton.which < JOY_MAX &&
		joy_info[E.jbutton.which].dev != NULL) {

		T_JOY_INFO *joy = &joy_info[E.jbutton.which];
		int offset = (joy->num) * KEY88_PAD_OFFSET;

		if (E.jbutton.button < KEY88_PAD_BUTTON_MAX) {
		    key88 = KEY88_PAD1_A + E.jbutton.button + offset;
		    quasi88_pad(key88, (E.type == SDL_JOYBUTTONDOWN));
		}
	    }
	    break;

	case SDL_QUIT:		/*------------------------------------------*/
	    if (verbose_proc) printf("Window Closed !\n");
	    quasi88_quit();
	    break;

	case SDL_WINDOWEVENT:	/*------------------------------------------*/
	    /* -focus オプションを機能させたいなら、 
	       quasi88_focus_in / quasi88_focus_out を適宜呼び出す必要がある */
        switch (E.window.event) {
        case SDL_WINDOWEVENT_SHOWN:/*"ウィンドウ %d が見えるようになった"*/
        case SDL_WINDOWEVENT_FOCUS_GAINED:/*"ウィンドウ %d がフォーカスを得た"*/
		    quasi88_focus_in();
            break;
        case SDL_WINDOWEVENT_HIDDEN:/*"ウィンドウ %d が見えないようになった"*/
        case SDL_WINDOWEVENT_FOCUS_LOST:/*"ウィンドウ %d がキーボードフォーカスを失った"*/
		    quasi88_focus_out();
            break;
        case SDL_WINDOWEVENT_EXPOSED:/*"ウィンドウ %d が現れた"*/
		    quasi88_expose();
            break;
        case SDL_WINDOWEVENT_MOVED:/*"ウィンドウ %d が %d,%d へ移動した"*/
            break;
        default:
            break;
        }
	    break;

	case SDL_USEREVENT:	/*------------------------------------------*/
	    if (E.user.code == 1) {
		display_fps();		/* test */
	    }
	    break;
	}
    }
}



/*
 * これは 終了時に1回だけ呼ばれる
 */
void	event_exit(void)
{
	joystick_exit();
}


/***********************************************************************
 * 現在のマウス座標取得関数
 *
 *	現在のマウスの絶対座標を *x, *y にセット
 ************************************************************************/

void	event_get_mouse_pos(int *x, int *y)
{
    int win_x, win_y;

    SDL_PumpEvents();
    SDL_GetMouseState(&win_x, &win_y);

    *x = win_x;
    *y = win_y;
}



/******************************************************************************
 * ソフトウェア NumLock 有効／無効
 *
 *****************************************************************************/

static	void	numlock_setup(int enable)
{
    int i;

    for (i=0; i<COUNTOF(binding); i++) {

	if        (binding[i].type == KEYCODE_SYM) {

	    if (enable) {
		keysym2key88[ binding[i].code ] = binding[i].new_key88;
	    } else {
		keysym2key88[ binding[i].code ] = binding[i].org_key88;
	    }

	} else if (binding[i].type == KEYCODE_SCAN) {

	    if (enable) {
		scancode2key88[ binding[i].code ] = binding[i].new_key88;
	    } else {
		scancode2key88[ binding[i].code ] = binding[i].org_key88;
	    }

	} else {
	    break;
	}
    }
}


int	event_numlock_on (void){ numlock_setup(TRUE);  return TRUE; }
void	event_numlock_off(void){ numlock_setup(FALSE); }


/******************************************************************************
 * エミュレート／メニュー／ポーズ／モニターモード の 開始時の処理
 * SDL2 はkey入力はunicodeが標準
 *****************************************************************************/

void	event_switch(void)
{
	/*SDL2 はkeysym入力は常にunicode*/
}

/******************************************************************************
 * ジョイスティック 
 *****************************************************************************/

static	int	joystick_init(void)
{
    SDL_Joystick *dev;
    int i, max, nr_button;

    /* ワーク初期化 */
    joystick_num = 0;

    memset(joy_info, 0, sizeof(joy_info));
    for (i=0; i<JOY_MAX; i++) {
	joy_info[i].dev = NULL;
    }

    /* ジョイスティックサブシステム初期化 */
    if (! SDL_WasInit(SDL_INIT_JOYSTICK)) {
	if (SDL_InitSubSystem(SDL_INIT_JOYSTICK)) {
	    return 0;
	}
    }

    /* ジョイスティックの数を調べて、デバイスオープン */
    max = SDL_NumJoysticks();
    max = MIN(max, JOY_MAX);		/* ワークの数だけ、有効 */

    for (i=0; i<max; i++) {
	dev = SDL_JoystickOpen(i);	/* i番目のジョイスティックをオープン */

	if (dev) {
	    /* ボタンの数を調べる */
	    nr_button = SDL_JoystickNumButtons(dev);
	    nr_button = MIN(nr_button, BUTTON_MAX);

	    joy_info[i].dev = dev;
	    joy_info[i].num = joystick_num ++;
	    joy_info[i].nr_button = nr_button;
	}
    }

    if (joystick_num > 0) {			/* 1個以上オープンできたら  */
	SDL_JoystickEventState(SDL_ENABLE);	/* イベント処理を有効にする */
    }

    return joystick_num;			/* ジョイスティックの数を返す */
}


/* ====== joystick Close ====== */
static	void	joystick_exit(void)
{
    int i;

    if (joystick_num > 0) {

	for (i=0; i<JOY_MAX; i++) {
	    if (joy_info[i].dev) {
		SDL_JoystickClose(joy_info[i].dev);
		joy_info[i].dev = NULL;
	    }
	}

	joystick_num = 0;
    }
}


/* ======get_joystick_num====== */
int	event_get_joystick_num(void)
{
    return joystick_num;
}

/****************************************************************************
 * キー設定ファイルを読み込んで、設定する。
 *	設定ファイルが無ければ偽、あれば処理して真を返す
 *****************************************************************************/

/* SDL2 の keysym の文字列を int 値に変換するテーブル */

static const T_SYMBOL_TABLE sdlkeysym_list[] =
{
    {	"SDLK_BACKSPACE",	SDLK_BACKSPACE	}, /*	= 8,	*/
    {	"SDLK_TAB",			SDLK_TAB		}, /*	= 9,	*/
    {	"SDLK_CLEAR",		SDLK_CLEAR		}, /*	= 12,	*/
    {	"SDLK_RETURN",		SDLK_RETURN		}, /*	= 13,	*/
    {	"SDLK_PAUSE",		SDLK_PAUSE		}, /*	= 19,	*/
    {	"SDLK_ESCAPE",		SDLK_ESCAPE		}, /*	= 27,	*/
    {	"SDLK_SPACE",		SDLK_SPACE		}, /*	= 32,	*/
    {	"SDLK_EXCLAIM",		SDLK_EXCLAIM	}, /*	= 33,	*/
    {	"SDLK_QUOTEDBL",	SDLK_QUOTEDBL	}, /*	= 34,	*/
    {	"SDLK_HASH",		SDLK_HASH		}, /*	= 35,	*/
    {	"SDLK_DOLLAR",		SDLK_DOLLAR		}, /*	= 36,	*/
    {	"SDLK_AMPERSAND",	SDLK_AMPERSAND	}, /*	= 38,	*/
    {	"SDLK_QUOTE",		SDLK_QUOTE		}, /*	= 39,	*/
    {	"SDLK_LEFTPAREN",	SDLK_LEFTPAREN	}, /*	= 40,	*/
    {	"SDLK_RIGHTPAREN",	SDLK_RIGHTPAREN	}, /*	= 41,	*/
    {	"SDLK_ASTERISK",	SDLK_ASTERISK	}, /*	= 42,	*/
    {	"SDLK_PLUS",		SDLK_PLUS		}, /*	= 43,	*/
    {	"SDLK_COMMA",		SDLK_COMMA		}, /*	= 44,	*/
    {	"SDLK_MINUS",		SDLK_MINUS		}, /*	= 45,	*/
    {	"SDLK_PERIOD",		SDLK_PERIOD		}, /*	= 46,	*/
    {	"SDLK_SLASH",		SDLK_SLASH		}, /*	= 47,	*/
    {	"SDLK_0",		SDLK_0			}, /*	= 48,	*/
    {	"SDLK_1",		SDLK_1			}, /*	= 49,	*/
    {	"SDLK_2",		SDLK_2			}, /*	= 50,	*/
    {	"SDLK_3",		SDLK_3			}, /*	= 51,	*/
    {	"SDLK_4",		SDLK_4			}, /*	= 52,	*/
    {	"SDLK_5",		SDLK_5			}, /*	= 53,	*/
    {	"SDLK_6",		SDLK_6			}, /*	= 54,	*/
    {	"SDLK_7",		SDLK_7			}, /*	= 55,	*/
    {	"SDLK_8",		SDLK_8			}, /*	= 56,	*/
    {	"SDLK_9",		SDLK_9			}, /*	= 57,	*/
    {	"SDLK_COLON",		SDLK_COLON		}, /*	= 58,	*/
    {	"SDLK_SEMICOLON",	SDLK_SEMICOLON	}, /*	= 59,	*/
    {	"SDLK_LESS",		SDLK_LESS		}, /*	= 60,	*/
    {	"SDLK_EQUALS",		SDLK_EQUALS		}, /*	= 61,	*/
    {	"SDLK_GREATER",		SDLK_GREATER	}, /*	= 62,	*/
    {	"SDLK_QUESTION",	SDLK_QUESTION	}, /*	= 63,	*/
    {	"SDLK_AT",			SDLK_AT			}, /*	= 64,	*/
    {	"SDLK_LEFTBRACKET",	SDLK_LEFTBRACKET	}, /*	= 91,	*/
    {	"SDLK_BACKSLASH",	SDLK_BACKSLASH		}, /*	= 92,	*/
    {	"SDLK_RIGHTBRACKET",SDLK_RIGHTBRACKET	}, /*	= 93,	*/
    {	"SDLK_CARET",		SDLK_CARET			}, /*	= 94,	*/
    {	"SDLK_UNDERSCORE",	SDLK_UNDERSCORE		}, /*	= 95,	*/
    {	"SDLK_BACKQUOTE",	SDLK_BACKQUOTE		}, /*	= 96,	*/
    {	"SDLK_a",		SDLK_a			}, /*	= 97,	*/
    {	"SDLK_b",		SDLK_b			}, /*	= 98,	*/
    {	"SDLK_c",		SDLK_c			}, /*	= 99,	*/
    {	"SDLK_d",		SDLK_d			}, /*	= 100,	*/
    {	"SDLK_e",		SDLK_e			}, /*	= 101,	*/
    {	"SDLK_f",		SDLK_f			}, /*	= 102,	*/
    {	"SDLK_g",		SDLK_g			}, /*	= 103,	*/
    {	"SDLK_h",		SDLK_h			}, /*	= 104,	*/
    {	"SDLK_i",		SDLK_i			}, /*	= 105,	*/
    {	"SDLK_j",		SDLK_j			}, /*	= 106,	*/
    {	"SDLK_k",		SDLK_k			}, /*	= 107,	*/
    {	"SDLK_l",		SDLK_l			}, /*	= 108,	*/
    {	"SDLK_m",		SDLK_m			}, /*	= 109,	*/
    {	"SDLK_n",		SDLK_n			}, /*	= 110,	*/
    {	"SDLK_o",		SDLK_o			}, /*	= 111,	*/
    {	"SDLK_p",		SDLK_p			}, /*	= 112,	*/
    {	"SDLK_q",		SDLK_q			}, /*	= 113,	*/
    {	"SDLK_r",		SDLK_r			}, /*	= 114,	*/
    {	"SDLK_s",		SDLK_s			}, /*	= 115,	*/
    {	"SDLK_t",		SDLK_t			}, /*	= 116,	*/
    {	"SDLK_u",		SDLK_u			}, /*	= 117,	*/
    {	"SDLK_v",		SDLK_v			}, /*	= 118,	*/
    {	"SDLK_w",		SDLK_w			}, /*	= 119,	*/
    {	"SDLK_x",		SDLK_x			}, /*	= 120,	*/
    {	"SDLK_y",		SDLK_y			}, /*	= 121,	*/
    {	"SDLK_z",		SDLK_z			}, /*	= 122,	*/
    {	"SDLK_DELETE",	SDLK_DELETE		}, /*	= 127,	*/

    {	"SDLK_CAPSLOCK",	SDLK_CAPSLOCK		},
    {	"SDLK_F1",		SDLK_F1			},
    {	"SDLK_F2",		SDLK_F2			}, 
    {	"SDLK_F3",		SDLK_F3			},
    {	"SDLK_F4",		SDLK_F4			},
    {	"SDLK_F5",		SDLK_F5			},
    {	"SDLK_F6",		SDLK_F6			},
    {	"SDLK_F7",		SDLK_F7			},
    {	"SDLK_F8",		SDLK_F8			},
    {	"SDLK_F9",		SDLK_F9			},
    {	"SDLK_F10",		SDLK_F10		},
    {	"SDLK_F11",		SDLK_F11		},
    {	"SDLK_F12",		SDLK_F12		},
    {	"SDLK_PRINTSCREEN",	SDLK_PRINTSCREEN	},
    {	"SDLK_SCROLLLOCK",	SDLK_SCROLLLOCK		},
    {	"SDLK_PAUSE",	SDLK_PAUSE		},
    {	"SDLK_INSERT",	SDLK_INSERT		},
    {	"SDLK_HOME",	SDLK_HOME		},
    {	"SDLK_PAGEUP",	SDLK_PAGEUP		},
    {	"SDLK_END",		SDLK_END		},
    {	"SDLK_PAGEDOWN",	SDLK_PAGEDOWN	},
    {	"SDLK_RIGHT",	SDLK_PAGEDOWN		},
    {	"SDLK_LEFT",	SDLK_LEFT		},
    {	"SDLK_DOWN",	SDLK_DOWN		},
    {	"SDLK_UP",		SDLK_UP		},
    {	"SDLK_NUMLOCKCLEAR",	SDLK_NUMLOCKCLEAR	},
    {	"SDLK_KP_DIVIDE",	SDLK_KP_DIVIDE		},
    {	"SDLK_KP_MULTIPLY",	SDLK_KP_MULTIPLY		},
    {	"SDLK_KP_MINUS",	SDLK_KP_MINUS		},
    {	"SDLK_KP_PLUS",		SDLK_KP_PLUS	},
    {	"SDLK_KP_ENTER",	SDLK_KP_ENTER		},
    {	"SDLK_KP_1",		SDLK_KP_1		},
    {	"SDLK_KP_2",		SDLK_KP_2		},
    {	"SDLK_KP_3",		SDLK_KP_3		},
    {	"SDLK_KP_4",		SDLK_KP_4		},
    {	"SDLK_KP_5",		SDLK_KP_5		},
    {	"SDLK_KP_6",		SDLK_KP_6		},
    {	"SDLK_KP_7",		SDLK_KP_7		},
    {	"SDLK_KP_8",		SDLK_KP_8		},
    {	"SDLK_KP_9",		SDLK_KP_9		},
    {	"SDLK_KP_0",		SDLK_KP_0		},
    {	"SDLK_KP_PERIOD",	SDLK_KP_PERIOD		},
    {	"SDLK_APPLICATION",	SDLK_APPLICATION	},
    {	"SDLK_POWER",		SDLK_POWER			},
    {	"SDLK_KP_EQUALS",	SDLK_KP_EQUALS		},
    {	"SDLK_F13",		SDLK_F13		},
    {	"SDLK_F14",		SDLK_F14		},
    {	"SDLK_F15",		SDLK_F15		},
    {	"SDLK_F16",		SDLK_F16		},
    {	"SDLK_F17",		SDLK_F17		},
    {	"SDLK_F18",		SDLK_F18		},
    {	"SDLK_F19",		SDLK_F19		}, 
    {	"SDLK_F20",		SDLK_F20		},
    {	"SDLK_F21",		SDLK_F21		},
    {	"SDLK_F22",		SDLK_F22		},
    {	"SDLK_F23",		SDLK_F23		},
    {	"SDLK_F24",		SDLK_F24		},
    {	"SDLK_EXECUTE",	SDLK_EXECUTE	},
    {	"SDLK_HELP",	SDLK_HELP		},
    {	"SDLK_MENU",	SDLK_MENU		},
    {	"SDLK_SELECT",	SDLK_SELECT		},
    {	"SDLK_STOP",	SDLK_STOP		},
    {	"SDLK_AGAIN",	SDLK_AGAIN		},
    {	"SDLK_UNDO",	SDLK_UNDO		},
    {	"SDLK_CUT",		SDLK_CUT		},
    {	"SDLK_COPY",	SDLK_COPY		},
    {	"SDLK_PASTE",	SDLK_PASTE		},
    {	"SDLK_FIND",	SDLK_FIND		},
    {	"SDLK_MUTE",	SDLK_MUTE		},
    {	"SDLK_VOLUMEUP",		SDLK_VOLUMEUP		},
    {	"SDLK_VOLUMEDOWN",		SDLK_VOLUMEDOWN		},
    {	"SDLK_KP_COMMA",		SDLK_KP_COMMA		},
    {	"SDLK_KP_EQUALSAS400",	SDLK_KP_EQUALSAS400		},
    {	"SDLK_ALTERASE",		SDLK_ALTERASE		},
    {	"SDLK_SYSREQ",	SDLK_SYSREQ		},
    {	"SDLK_CANCEL",	SDLK_CANCEL		},
    {	"SDLK_CLEAR",	SDLK_CLEAR		},
    {	"SDLK_PRIOR",	SDLK_PRIOR		},
    {	"SDLK_RETURN2",		SDLK_RETURN2		},
    {	"SDLK_SEPARATOR",	SDLK_SEPARATOR		},
    {	"SDLK_OUT",			SDLK_OUT			},
    {	"SDLK_OPER",		SDLK_OPER			},
    {	"SDLK_CLEARAGAIN",	SDLK_CLEARAGAIN		},
    {	"SDLK_CRSEL",		SDLK_CRSEL			},
    {	"SDLK_EXSEL",		SDLK_EXSEL			},
    {	"SDLK_KP_00",		SDLK_KP_00			},
    {	"SDLK_KP_000",		SDLK_KP_000			},
    {	"SDLK_THOUSANDSSEPARATOR",	SDLK_THOUSANDSSEPARATOR		},
    {	"SDLK_DECIMALSEPARATOR",	SDLK_DECIMALSEPARATOR		},
    {	"SDLK_CURRENCYUNIT",		SDLK_CURRENCYUNIT		},
    {	"SDLK_CURRENCYSUBUNIT",		SDLK_CURRENCYSUBUNIT	},
    {	"SDLK_KP_LEFTPAREN",		SDLK_KP_LEFTPAREN		},
    {	"SDLK_KP_RIGHTPAREN",		SDLK_KP_RIGHTPAREN		},
    {	"SDLK_KP_LEFTBRACE",		SDLK_KP_LEFTBRACE		},
    {	"SDLK_KP_RIGHTBRACE",		SDLK_KP_RIGHTBRACE		},
    {	"SDLK_KP_TAB",			SDLK_KP_TAB			},
    {	"SDLK_KP_BACKSPACE",	SDLK_KP_BACKSPACE	},
    {	"SDLK_KP_A",		SDLK_KP_A		},
    {	"SDLK_KP_B",		SDLK_KP_B		},
    {	"SDLK_KP_C",		SDLK_KP_C		},
    {	"SDLK_KP_D",		SDLK_KP_D		},
    {	"SDLK_KP_E",		SDLK_KP_E		},
    {	"SDLK_KP_F",		SDLK_KP_F		},
    {	"SDLK_KP_XOR",		SDLK_KP_XOR		},
    {	"SDLK_KP_POWER",		SDLK_KP_POWER		},
    {	"SDLK_KP_PERCENT",		SDLK_KP_PERCENT		},
    {	"SDLK_KP_LESS",			SDLK_KP_LESS		},
    {	"SDLK_KP_GREATER",		SDLK_KP_GREATER		},
    {	"SDLK_KP_AMPERSAND",	SDLK_KP_AMPERSAND		},
    {	"SDLK_KP_DBLAMPERSAND",	SDLK_KP_DBLAMPERSAND		},
    {	"SDLK_KP_VERTICALBAR",	SDLK_KP_VERTICALBAR			},
    {	"SDLK_KP_DBLVERTICALBAR",SDLK_KP_DBLVERTICALBAR		},
    {	"SDLK_KP_COLON",		SDLK_KP_COLON		},
    {	"SDLK_KP_HASH",			SDLK_KP_HASH		},
    {	"SDLK_KP_SPACE",		SDLK_KP_SPACE		},
    {	"SDLK_KP_AT",			SDLK_KP_AT			},
    {	"SDLK_KP_EXCLAM",		SDLK_KP_EXCLAM		},
    {	"SDLK_KP_MEMSTORE",		SDLK_KP_MEMSTORE	},
    {	"SDLK_KP_MEMRECALL",	SDLK_KP_MEMRECALL	},
    {	"SDLK_KP_MEMCLEAR",		SDLK_KP_MEMCLEAR	},
    {	"SDLK_KP_MEMADD",		SDLK_KP_MEMADD		},
    {	"SDLK_KP_MEMSUBTRACT",	SDLK_KP_MEMSUBTRACT		},
    {	"SDLK_KP_MEMMULTIPLY",	SDLK_KP_MEMMULTIPLY		},
    {	"SDLK_KP_MEMDIVIDE",	SDLK_KP_MEMDIVIDE		},
    {	"SDLK_KP_PLUSMINUS",	SDLK_KP_PLUSMINUS		},
    {	"SDLK_KP_CLEAR",		SDLK_KP_CLEAR			},
    {	"SDLK_KP_CLEARENTRY",	SDLK_KP_CLEARENTRY		},
    {	"SDLK_KP_BINARY",		SDLK_KP_BINARY		},
    {	"SDLK_KP_OCTAL",		SDLK_KP_OCTAL		},
    {	"SDLK_KP_DECIMAL",		SDLK_KP_DECIMAL		},
    {	"SDLK_KP_HEXADECIMAL",	SDLK_KP_HEXADECIMAL		},
    {	"SDLK_LCTRL",			SDLK_LCTRL	},
    {	"SDLK_LSHIFT",			SDLK_LSHIFT	},
    {	"SDLK_LALT",			SDLK_LALT	},
    {	"SDLK_LGUI",			SDLK_LGUI	},
    {	"SDLK_RCTRL",			SDLK_RCTRL	},
    {	"SDLK_RSHIFT",			SDLK_RSHIFT	},
    {	"SDLK_RALT",			SDLK_RALT	},
    {	"SDLK_RGUI",			SDLK_RGUI	},
    {	"SDLK_MODE",			SDLK_MODE	},
    {	"SDLK_AUDIONEXT",		SDLK_AUDIONEXT		},
    {	"SDLK_AUDIOPREV",		SDLK_AUDIOPREV		},
    {	"SDLK_AUDIOSTOP",		SDLK_AUDIOSTOP		},
    {	"SDLK_AUDIOPLAY",		SDLK_AUDIOPLAY		},
    {	"SDLK_AUDIOMUTE",		SDLK_AUDIOMUTE		},
    {	"SDLK_MEDIASELECT",		SDLK_MEDIASELECT	},
    {	"SDLK_WWW",				SDLK_WWW			},
    {	"SDLK_MAIL",			SDLK_MAIL			},
    {	"SDLK_CALCULATOR",		SDLK_CALCULATOR		},
    {	"SDLK_COMPUTER",		SDLK_COMPUTER		},
    {	"SDLK_AC_SEARCH",		SDLK_AC_SEARCH		},
    {	"SDLK_AC_HOME",			SDLK_AC_HOME		},
    {	"SDLK_AC_BACK",			SDLK_AC_BACK		},
    {	"SDLK_AC_FORWARD",		SDLK_AC_FORWARD		},
    {	"SDLK_AC_STOP",			SDLK_AC_STOP		},
    {	"SDLK_AC_REFRESH",		SDLK_AC_REFRESH		},
    {	"SDLK_AC_BOOKMARKS",	SDLK_AC_BOOKMARKS			},
    {	"SDLK_BRIGHTNESSDOWN",	SDLK_BRIGHTNESSDOWN			},
    {	"SDLK_BRIGHTNESSUP",	SDLK_BRIGHTNESSUP			},
    {	"SDLK_DISPLAYSWITCH",	SDLK_DISPLAYSWITCH			},
    {	"SDLK_KBDILLUMTOGGLE",	SDLK_KBDILLUMTOGGLE			},
    {	"SDLK_KBDILLUMDOWN",	SDLK_KBDILLUMDOWN			},
    {	"SDLK_KBDILLUMUP",		SDLK_KBDILLUMUP		},
    {	"SDLK_EJECT",			SDLK_EJECT			},
    {	"SDLK_SLEEP",			SDLK_SLEEP			},
};
/* デバッグ用 */
static	const char *debug_sdlkeysym(int code)
{
    int i;
    for (i=0; i<COUNTOF(sdlkeysym_list); i++) {
	if (code == sdlkeysym_list[i].val)
	    return sdlkeysym_list[i].name;
    }
    return "invalid";
}

/* キー設定ファイルの、識別タグをチェックするコールバック関数 */

static const char *identify_callback(const char *parm1,
				     const char *parm2,
				     const char *parm3)
{
    if (my_strcmp(parm1, "[SDL]") == 0) {
	if (parm2 == NULL ||
	    my_strcmp(parm2, video_driver) == 0) {
	    return NULL;				/* 有効 */
	}
    }

    return "";						/* 無効 */
}

/* キー設定ファイルの、設定を処理するコールバック関数 */

static const char *setting_callback(int type,
				    int code,
				    int key88,
				    int numlock_key88)
{
    static int binding_cnt = 0;

    if (type == KEYCODE_SCAN) {
	if (code >= COUNTOF(scancode2key88)) {
	    return "scancode too large";	/* 無効 */
	}
	scancode2key88[ code ] = key88;
    } else {
	keysym2key88[ code ]   = key88;
    }

    if (numlock_key88 >= 0) {
	if (binding_cnt >= COUNTOF(binding)) {
	    return "too many NumLock-code";	/* 無効 */
	}
	binding[ binding_cnt ].type      = type;
	binding[ binding_cnt ].code      = code;
	binding[ binding_cnt ].new_key88 = numlock_key88;
	binding_cnt ++;
    }

    return NULL;				/* 有効 */
}

/* キー設定ファイルの処理関数 */

static	int	analyze_keyconf_file(void)
{
    return
	config_read_keyconf_file(file_keyboard,		  /* キー設定ファイル*/
				 identify_callback,	  /* 識別タグ行 関数 */
				 sdlkeysym_list,	  /* 変換テーブル    */
				 COUNTOF(sdlkeysym_list), /* テーブルサイズ  */
				 TRUE,			  /* 大小文字無視    */
				 setting_callback);	  /* 設定行 関数     */
}



/******************************************************************************
 * FPS
 *****************************************************************************/

/* test */

#define	FPS_INTRVAL		(1000)		/* 1000ms毎に表示する */
static	Uint32	display_fps_callback(Uint32 interval, void *dummy);

static	int	display_fps_init(void)
{
    if (show_fps == FALSE) return TRUE;

    if (! SDL_WasInit(SDL_INIT_TIMER)) {
	if (SDL_InitSubSystem(SDL_INIT_TIMER)) {
	    return FALSE;
	}
    }

    SDL_AddTimer(FPS_INTRVAL, display_fps_callback, NULL);
    return TRUE;
}

static	Uint32	display_fps_callback(Uint32 interval, void *dummy)
{
#if 0

    /* コールバック関数の内部からウインドウタイトルを変更するのは危険か ?
       「コールバック関数内ではどんな関数も呼び出すべきでない」となっている */

    display_fps();

#else

    /* SDL_PushEvent だけは呼び出しても安全となっているので、
       ユーザイベントで処理してみよう */

    SDL_Event user_event;

    user_event.type = SDL_USEREVENT;
    user_event.user.code  = 1;
    user_event.user.data1 = NULL;
    user_event.user.data2 = NULL;
    SDL_PushEvent(&user_event);		/* エラーは無視 */
#endif

    return FPS_INTRVAL;
}


static	void	display_fps(void)
{
    static int prev_drawn_count;
    static int prev_vsync_count;
    int now_drawn_count;
    int now_vsync_count;

    if (show_fps == FALSE) return;

    now_drawn_count = quasi88_info_draw_count();
    now_vsync_count = quasi88_info_vsync_count();

    if (quasi88_is_exec()) {
	char buf[32];

	sprintf(buf, "FPS: %3d (VSYNC %3d)",
		now_drawn_count - prev_drawn_count,
		now_vsync_count - prev_vsync_count);

	extern SDL_Window *sdl_display;
	//SDL_WM_SetCaption(buf, buf);
	//SDL_SetWindowTitle(sdl_display, buf);
    }

    prev_drawn_count = now_drawn_count;
    prev_vsync_count = now_vsync_count;
}
