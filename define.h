//---------------------------------------------------------------------------------------//
// 概要: smooth内でいろいろ使う関数など
// 更新: 2003 4/21
// 
//---------------------------------------------------------------------------------------//

#ifndef __DEFINE_H
#define __DEFINE_H

// 型 //
typedef uint16_t		A_u_short;
typedef uint8_t			A_u_char;
typedef double			A_FpLong;
typedef float			A_FpShort;
typedef int32_t			A_long;
typedef uint32_t		A_u_long;
typedef A_long PF_WorldFlags;
typedef unsigned char   u_char;
typedef unsigned int    u_int;
typedef unsigned short  u_short;
typedef unsigned long   u_long;
#ifndef A_INTERNAL
	// Basic pixel defn's
	typedef struct {
		A_u_char	alpha, red, green, blue;
	} PF_Pixel;

	typedef PF_Pixel		PF_Pixel8;
	typedef PF_Pixel		PF_UnionablePixel;

	typedef struct {
		#ifdef PF_PIXEL16_RENAME_COMPONENTS
			// this style is useful for debugging code converted from 8 bit
			A_u_short		alphaSu, redSu, greenSu, blueSu;
		#else
			A_u_short		alpha, red, green, blue;
		#endif
	} PF_Pixel16;
	
	typedef A_FpShort			PF_FpShort;
	typedef A_FpLong			PF_FpLong;

	typedef struct {
		PF_FpShort				alpha, red, green, blue;
	} PF_PixelFloat, PF_Pixel32;
	
	typedef struct {
		PF_FpLong				mat[3][3];
	} PF_FloatMatrix;

#endif
typedef A_u_long	PF_PixLong;

typedef struct _PF_PixelOpaque	*PF_PixelOpaquePtr;

#ifdef PF_DEEP_COLOR_AWARE
	typedef PF_PixelOpaquePtr		PF_PixelPtr;
#else
	typedef PF_Pixel				*PF_PixelPtr;
#endif
typedef struct {
	A_long left, top, right, bottom;
} PF_LRect;

typedef PF_LRect	PF_Rect;
typedef PF_Rect		PF_UnionableRect;
typedef struct {
	A_long		num;	/* numerator */
	A_u_long	den;	/* denominator */
} PF_RationalScale;

typedef struct PF_LayerDef {
	/* PARAMETER VALUE */
	
	void				*reserved0;
	void				*reserved1;
	
	PF_WorldFlags		world_flags;

	PF_PixelPtr			data;

	A_long				rowbytes;
	A_long				width;
	A_long				height;
	PF_UnionableRect	extent_hint;
	/* For source, extent_hint is the smallest rect encompassing all opaque
	 * (non-zero alpha) areas of the layer.  For output, this encompasses
	 * the area that needs to be rendered (i.e. not covered by other layers,
	 * needs refreshing, etc.). if your plug-in varies based on extent (like
	 * a diffusion dither, for example) you should ignore this param and
	 * render the full frame each time.
	 */
	void				*platform_ref;		/* unused since CS5 */

	A_long				reserved_long1;

	void				*reserved_long4;

	PF_RationalScale	pix_aspect_ratio;	/* pixel aspect ratio of checked out layer */

	void				*reserved_long2;

	A_long				origin_x;		/* origin of buffer in layer coords; smart effect checkouts only */ 
	A_long				origin_y;

	A_long				reserved_long3;
	
	/* PARAMETER DESCRIPTION */
	A_long				dephault;		/* use a PF_LayerDefault constant defined above */

} PF_LayerDef;


// AfterEffectsはARGB


struct Cinfo
{
    long    length;     // カウントした画素数
    float   start, end; // 実際にブレンドする座標値(その該当する方向のものしか有効でない)
    u_int   flg;        // 特殊処理用のフラグ

    Cinfo()  {length = 0; flg = 0;} // コンストラクタ
};

// ↑のフラグ //
#define CR_FLG_FILL         (1<<0) // べた塗りモード


// 合成関数用の情報用構造体 //
template <typename PixelType>
struct BlendingInfo
{
    PF_LayerDef *input, *output;    // 入出力画像
	PixelType			*in_ptr, *out_ptr;	// 画像データへのポインタ
    int                 i,j;                // 現在処理中の座標値
    long                in_target,          // 処理中の座標値の1次配列インデックス  (inputに対する)
                        out_target;         //                                      (outputに対する)
    Cinfo               core[4];            // 処理すべき長さ、フラグなどのコアの情報 0:left 1:right 2:top 3:bottom
    int                 flag;               // 制御用のフラグ(補正用のカウントなど)
    unsigned int        range;              // 同じ色とみなす範囲
    int                 mode;               // 処理するモード

    float               LineWeight;         // ラインの太さ
};

// ↑のフラグ
#define SECOND_COUNT    (1<<0)      // 補正用の2度目のカウント

// ↑のMode //
enum
{
    BLEND_MODE_UP_H = 0,            // 上向きの角の横方向
    BLEND_MODE_UP_V,                // 上向きの角の縦方向
    BLEND_MODE_DOWN_H,              // 下向きの角の横方向
    BLEND_MODE_DOWN_V,              // 下向きの角の縦方向
};


#endif
