/***********************************************************************
 * グラフィック処理 (システム依存)
 *
 *	詳細は、 graph.h 参照
 ************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <SDL2/SDL.h>

#include "quasi88.h"
#include "graph.h"
#include "device.h"

/************************************************************************/

/*#define	DEBUG_PRINTF*/


/* 以下は static な変数。オプションで変更できるのでグローバルにしてある */

    int	use_hwsurface	= TRUE;		/* HW SURFACE を使うかどうか	*/
    int	use_doublebuf	= FALSE;	/* ダブルバッファを使うかどうか	*/


/* 以下は、 event.c などで使用する、 OSD なグローバル変数 */

    int	sdl_mouse_rel_move;		/* マウス相対移動量検知可能か	*/



/************************************************************************/

static	T_GRAPH_SPEC	graph_spec;		/* 基本情報		*/

static	int		graph_exist = FALSE;			/* 真で、画面生成済み	*/
static	T_GRAPH_INFO	graph_info;		/* その時の、画面情報	*/


/************************************************************************
 *	SDLの初期化
 *	SDLの終了
 ************************************************************************/

int	sdl_init(void)
{
	SDL_version compiled;
	SDL_version linked;

	SDL_VERSION(&compiled);
	SDL_GetVersion(&linked);

	if (verbose_proc) {
	printf("We compiled against SDL version %d.%d.%d ...\n",
		compiled.major, compiled.minor, compiled.patch);
	printf("But we are linking against SDL version %d.%d.%d.\n",
		linked.major, linked.minor, linked.patch);
	}


	if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO) < 0) {
		if (verbose_proc) printf("Failed\n");
		fprintf(stderr, "SDL Error: %s\n", SDL_GetError());

		return FALSE;

	} else {
		if (verbose_proc) printf(" SDL init END.\n");

		return TRUE;

	}
}

/************************************************************************
 *	グラフィック処理の初期化
 *	グラフィック処理の動作
 *	グラフィック処理の終了
 ************************************************************************/

static	char	sdl_vname[16]={"SDL2 Display"};
static	int		sdl_depth = 0;
static	int		sdl_byte_per_pixel = 0;

static	SDL_Rect **sdl_mode;
static	Uint32	sdl_mode_flags;


const T_GRAPH_SPEC	*graph_init(void)
{
    int display_index;
    SDL_DisplayMode mode = { SDL_PIXELFORMAT_UNKNOWN, 0, 0, 0, 0 };

	/*==マルチ・ディスプレイ　プライマリ(0)の現在の解像度を調査==*/
	display_index = 0;
	// display_count = SDL_GetNumVideoDisplays();
	// modes_count = SDL_GetNumDisplayModes(display_index);


        if (SDL_GetCurrentDisplayMode(display_index, &mode) == 0)
        {
         graph_spec.window_max_width      = mode.w;/*現在の解像度*/
         graph_spec.window_max_height     = mode.h;
         graph_spec.fullscreen_max_width  = mode.w;/*ダミーセット*/
         graph_spec.fullscreen_max_height = mode.h;
         graph_spec.forbid_status         = FALSE;/*DOUBLE禁止？*/
         graph_spec.forbid_half           = FALSE;/*HURF禁止？*/
         sdl_byte_per_pixel = SDL_BYTESPERPIXEL(mode.format);
         sdl_depth          = sdl_byte_per_pixel * 8 ; /*24bitの場合がある！*/
        }
        else
		{
		 if (verbose_proc) printf("Err. Display mode.\n");
		 return FALSE;
		}

/*モード条件判定*/
#if	defined(SUPPORT_32BPP) && (sdl_byte_per_pixel == 4)
	sdl_depth          = 32;
	sdl_byte_per_pixel = 4;
#elif	defined(SUPPORT_16BPP) &&  (sdl_byte_per_pixel == 2)
	sdl_depth          = 16;
	sdl_byte_per_pixel = 2;
#elif	defined(SUPPORT_16BPP) &&  (sdl_byte_per_pixel == 4)
	sdl_depth          = 16;
	sdl_byte_per_pixel = 2;
#endif

/*エラー処理*/
	if(sdl_byte_per_pixel == 0){
		 if (verbose_proc) printf("Err. byte_per_pixel.\n");
		 return FALSE;
	}

	if (verbose_proc){
	printf("  VideoINFO:Maxsize=window(%d,%d),fullscreen(%d,%d) %dbpp(%dbyte)\n",
		graph_spec.window_max_width,graph_spec.window_max_height,
		graph_spec.fullscreen_max_width,graph_spec.fullscreen_max_height,
		sdl_depth, sdl_byte_per_pixel);
	}

	return &graph_spec;
}


/************************************************************************
 *  メイン画面をwindow  renderer,surface,texture 生成
 ************************************************************************/

static SDL_Window *sdl_display;
static SDL_Renderer *sdl_render;
static SDL_Surface *sdl_offscreen;
static SDL_Texture *sdl_texture;

const T_GRAPH_INFO	*graph_setup(int width, int height,
				     int fullscreen, double aspect)
{
    Uint32 flags;

    if (graph_exist) { /*---すでに初期化済みならば一旦終了---*/
	if (verbose_proc) printf("Re-Init Graphic System (%s) ...\n",sdl_vname);
	    SDL_QuitSubSystem(SDL_INIT_VIDEO);
	    graph_exist = FALSE;
    }

	/* VIDEOの再初期化 */
	if (! SDL_WasInit(SDL_INIT_VIDEO)) {
	 if (SDL_InitSubSystem(SDL_INIT_VIDEO) != 0) {
	    if (verbose_proc) printf("Video INIT FAILED\n");
	    return NULL;
	 }
	if (verbose_proc) printf("Video INIT OK\n");
    }

    /* 全画面モードの場合、SDL2の機能で実現する！ なのでsearch_modeは不要。*/
    if (fullscreen) {
      flags = SDL_WINDOW_FULLSCREEN_DESKTOP;
      if (verbose_proc) printf("  Trying full screen mode ... ");
    }
    else{
      flags = 0;
      if (verbose_proc) printf("  Opening window ... ");
    }

    /*ウィンドウ&レンダラー生成*/
	if(SDL_CreateWindowAndRenderer(width, height, flags, &sdl_display, &sdl_render)){
		if (verbose_proc) printf("Window Error %s\n",SDL_GetError());
		return NULL;
	}
	if (verbose_proc)	printf("OK\n");


    /*サーフェース生成*/
    if (verbose_proc) printf("  Allocating surface buffer ... ");
    sdl_offscreen = SDL_CreateRGBSurface(0, width, height, sdl_depth, 0, 0, 0, 0);
    if (verbose_proc) printf("%s\n", (sdl_offscreen ? "OK" : "FAILED"));
    if (sdl_offscreen == NULL) return NULL;

    /*テクスチャー生成*/
    if (verbose_proc) printf("  Allocating texture buffer ... ");
    sdl_texture = SDL_CreateTexture(sdl_render,SDL_PIXELFORMAT_BGRA32,
        SDL_TEXTUREACCESS_STREAMING, width, height);
    if (verbose_proc) printf("%s\n", (sdl_texture ? "OK" : "FAILED"));
    if (sdl_texture == NULL) return NULL;


	/*全画面時論理解像度を変更*/
	if (fullscreen) {
	 if(SDL_RenderSetLogicalSize( sdl_render ,width, height)){/*実際の解像度と論理解像度を合わせる*/
		if (verbose_proc) printf("Render Error %s\n",SDL_GetError());
		return NULL;
	 }
	}

	SDL_RenderClear(sdl_render);/*画面バッファクリア*/


    /* Surfaceの画面情報をセットして、返す */

    graph_info.fullscreen	= fullscreen;
    graph_info.width		= sdl_offscreen->w;
    graph_info.height		= sdl_offscreen->h;
    graph_info.byte_per_pixel	= sdl_byte_per_pixel;
    graph_info.byte_per_line	= sdl_offscreen->pitch;
    graph_info.buffer		= sdl_offscreen->pixels;
    graph_info.nr_color		= 255;
    graph_info.write_only	= FALSE;
    graph_info.broken_mouse	= FALSE;
    graph_info.draw_start	= NULL;
    graph_info.draw_finish	= NULL;
    graph_info.dont_frameskip	= FALSE;

    graph_exist = TRUE; /*初期化完了flag*/

    return &graph_info;
}

/************************************************************************
 *  SDL全終了処理
 ************************************************************************/
void	sdl_exit(void)
{
	SDL_Quit();
}



/************************************************************************
 *  SDL VIDEOのみ終了処理
 ************************************************************************/
void	graph_exit(void)
{
	SDL_QuitSubSystem(SDL_INIT_VIDEO);
}


/************************************************************************
 *	色の確保
 *	色の解放
 ************************************************************************/

void	graph_add_color(const PC88_PALETTE_T color[],
			int nr_color, unsigned long pixel[])
{
    int i;
    for (i=0; i<nr_color; i++) {
	pixel[i] = SDL_MapRGB(sdl_offscreen->format,
			      color[i].red, color[i].green, color[i].blue);
    }
}

/************************************************************************/

void	graph_remove_color(int nr_pixel, unsigned long pixel[])
{
    /* 色に関しては何も管理しないので、ここでもなにもしない */
}



/************************************************************************
 *	SDL2 グラフィックの更新
 ************************************************************************/

void	graph_update(int nr_rect, T_GRAPH_RECT rect[])
{

	/*surface → texture へ転送*/
	/*sdl_texture = SDL_CreateTextureFromSurface(sdl_render, sdl_offscreen);  遅い？*/
	SDL_UpdateTexture(sdl_texture, NULL, sdl_offscreen->pixels, sdl_offscreen->pitch);

	/*texture → render へ転送*/
	SDL_RenderCopy(sdl_render, sdl_texture, NULL, NULL);

	/*画面更新*/
	SDL_RenderPresent(sdl_render);

}


/************************************************************************
 *	タイトルの設定
 *	属性の設定
 ************************************************************************/
void	graph_set_window_title(const char *title)
{
	SDL_SetWindowTitle(sdl_display, title);
}

/******************************** TEST中 *********************************/

void	graph_set_attribute(int mouse_show, int grab, int keyrepeat_on)
{
    if (mouse_show) SDL_ShowCursor(SDL_ENABLE);
    else            SDL_ShowCursor(SDL_DISABLE);

	/*Grab許可or不許可の設定。あとはSDLに任せる*/
	if (grab) SDL_SetWindowGrab(sdl_display, SDL_TRUE);
	else      SDL_SetWindowGrab(sdl_display, SDL_FALSE);

    //if (keyrepeat_on) SDL_EnableKeyRepeat(SDL_DEFAULT_REPEAT_DELAY,
	//				  SDL_DEFAULT_REPEAT_INTERVAL);
    //else              SDL_EnableKeyRepeat(0, 0);

    sdl_mouse_rel_move = (mouse_show == FALSE && grab) ? TRUE : FALSE;

    /* SDL は、グラブ中かつマウスオフなら、ウインドウの端にマウスが
       ひっかかっても、マウス移動の相対量を検知できる。

       なので、この条件を sdl_mouse_rel_move にセットしておき、
       真なら、マウス移動は相対量、偽なら絶対位置とする (event.c)

       メニューでは、かならずグラブなし (マウスはあり or なし) なので、
       この条件にはかからず、常にウインドウの端でマウスは停止する。
    */
}

/*
  -videodrv directx について

  グラブあり、マウスありの場合、グラブされない・・・

  全画面で、グラブなし、マウスなしにすると、
  マウスが画面の端で停止してしまう。あたりまえだが…
  全画面の場合、グラブなしは意味があるのか？ マルチディスプレイで検証



  -videodrv dga について

  ウインドウでも全画面でも、全画面フラグが立っている。
  デフォルトで -hwsurface になっている。 -swsurface の指定は可能。
  -doublebuf を指定すると、 -hwsurface もセットで有効になる。

  全画面←→ウインドウを繰り返すとコアを吐く。

  -hwsurface では、マウスの表示に時々残骸が残る。
  -swsurface は問題さなげ。

  -doublebuf を指定すると、マウスは表示されなくなる。
*/
