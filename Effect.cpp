
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "AE_Effect.h"
#include "AE_EffectCB.h"
#include "AE_Macros.h"

//#include "Param_Utils.h"
//#include "version.h"
#include "util.h"


#include "define.h"

#include "upMode.h"
#include "downMode.h"
#include "8link.h"
#include "Lack.h"

//#include "Effect.h"

//#include <string.h>
#include <math.h>

#include <string>
#include <iostream>

// the one OFX header we need, it includes the others necessary
#include "ofxImageEffect.h"

#if defined __APPLE__ || defined linux
#  define EXPORT __attribute__((visibility("default")))
#elif defined _WIN32
#  define EXPORT OfxExport
#else
#  error Not building on your operating system quite yet
#endif

////////////////////////////////////////////////////////////////////////////////
// macro to write a labelled message to stderr with
#ifdef _WIN32
  #define DUMP(LABEL, MSG, ...)                                           \
  {                                                                       \
    fprintf(stderr, "%s%s:%d in %s ", LABEL, __FILE__, __LINE__, __FUNCTION__); \
    fprintf(stderr, MSG, ##__VA_ARGS__);                                  \
    fprintf(stderr, "\n");                                                \
  }
#else
  #define DUMP(LABEL, MSG, ...)                                           \
  {                                                                       \
    fprintf(stderr, "%s%s:%d in %s ", LABEL, __FILE__, __LINE__, __PRETTY_FUNCTION__); \
    fprintf(stderr, MSG, ##__VA_ARGS__);                                  \
    fprintf(stderr, "\n");                                                \
  }
#endif

// macro to write a simple message, only works if 'VERBOSE' is #defined
//#define VERBOSE
#ifdef VERBOSE
#  define MESSAGE(MSG, ...) DUMP("", MSG, ##__VA_ARGS__)
#else
#  define MESSAGE(MSG, ...)
#endif

// macro to dump errors to stderr if the given condition is true
#define ERROR_IF(CONDITION, MSG, ...) if(CONDITION) { DUMP("ERROR : ", MSG, ##__VA_ARGS__);}

// macro to dump errors to stderr and abort if the given condition is true
#define ERROR_ABORT_IF(CONDITION, MSG, ...)     \
{                                               \
  if(CONDITION) {                               \
    DUMP("FATAL ERROR : ", MSG, ##__VA_ARGS__); \
    abort();                                    \
  }                                             \
}
// name of our two params
//#define GAIN_PARAM_NAME "gain"
//#define APPLY_TO_ALPHA_PARAM_NAME "applyToAlpha"
#define WHITE_OPTION_PARAM_NAME "whiteOption"
#define RANGE_PARAM_NAME "range"
#define LINE_WEIGHT_PARAM_NAME "lineWeight"

////////////////////////////////////////////////////////////////////////////////
// set of suite pointers provided by the host
OfxHost               *gHost;
OfxPropertySuiteV1    *gPropertySuite    = 0;
OfxImageEffectSuiteV1 *gImageEffectSuite = 0;
OfxParameterSuiteV1   *gParameterSuite   = 0;

////////////////////////////////////////////////////////////////////////////////
// our instance data, where we are caching away clip and param handles
struct MyInstanceData {
  // handles to the clips we deal with
  OfxImageClipHandle sourceClip;
  OfxImageClipHandle outputClip;

  // handles to a our parameters
  OfxParamHandle whiteOptionParam;
  OfxParamHandle rangeParam;
  OfxParamHandle lineWeightParam;
};

////////////////////////////////////////////////////////////////////////////////
// get my instance data from a property set handle
MyInstanceData *FetchInstanceData(OfxPropertySetHandle effectProps)
{
  MyInstanceData *myData = 0;
  gPropertySuite->propGetPointer(effectProps,
                                 kOfxPropInstanceData,
                                 0,
                                 (void **) &myData);
  return myData;
}

////////////////////////////////////////////////////////////////////////////////
// get my instance data
MyInstanceData *FetchInstanceData(OfxImageEffectHandle effect)
{
  // get the property handle for the plugin
  OfxPropertySetHandle effectProps;
  gImageEffectSuite->getPropertySet(effect, &effectProps);

  // and get the instance data out of that
  return FetchInstanceData(effectProps);
}

////////////////////////////////////////////////////////////////////////////////
// get the named suite and put it in the given pointer, with error checking
template <class SUITE>
void FetchSuite(SUITE *& suite, const char *suiteName, int suiteVersion)
{
  suite = (SUITE *) gHost->fetchSuite(gHost->host, suiteName, suiteVersion);
  if(!suite) {
    ERROR_ABORT_IF(suite == NULL,
                   "Failed to fetch %s verison %d from the host.",
                   suiteName,
                   suiteVersion);
  }
}

//---------------------------------------------------------------------------//
// 定義
enum 
{
    PARAM_INPUT = 0,
    PARAM_WHITE_OPTION,
    PARAM_RANGE,
    PARAM_LINE_WEIGHT,
    PARAM_NUM,
};

//---------------------------------------------------------------------------//
/*
// プロトタイプ
static PF_Err About (   PF_InData       *in_data,
                        PF_OutData      *out_data,
                        PF_ParamDef     *params[],
                        PF_LayerDef     *output );

static PF_Err GlobalSetup ( PF_InData       *in_data,
                            PF_OutData      *out_data,
                            PF_ParamDef     *params[],
                            PF_LayerDef     *output );

static PF_Err GlobalSetdown( PF_InData      *in_data,
                            PF_OutData      *out_data,
                            PF_ParamDef     *params[],
                            PF_LayerDef     *output );

static PF_Err ParamsSetup ( PF_InData       *in_data,
                            PF_OutData      *out_data,
                            PF_ParamDef     *params[],
                            PF_LayerDef     *output);

static PF_Err Render (  PF_InData       *in_data,
                        PF_OutData      *out_data,
                        PF_ParamDef     *params[],
                        PF_LayerDef     *output );

static PF_Err PopDialog (PF_InData		*in_data,
						 PF_OutData		*out_data,
						 PF_ParamDef		*params[],
						 PF_LayerDef		*output );
*/

//---------------------------------------------------------------------------//
// util funcs
//---------------------------------------------------------------------------//
static inline void getWhitePixel(PF_Pixel16 *white)	
{ 
	PF_Pixel16 color = { 0x8000, 0x8000, 0x8000, 0x8000 };
	*white = color;
}

static inline void getWhitePixel(PF_Pixel8 *white )
{ 
	PF_Pixel8	color = { 0xFF, 0xFF, 0xFF, 0xFF };
	*white = color;
}

static inline void getNullPixel(PF_Pixel16 *null_pixel )
{ 
	PF_Pixel16	color = { 0x0, 0x0, 0x0, 0x0 };
	*null_pixel = color;
}

static inline void getNullPixel(PF_Pixel8 *null_pixel )
{ 
	PF_Pixel8	color = { 0x0, 0x0, 0x0, 0x0 };
	*null_pixel = color;
}






#if 0
template<typename PackedPixelType > 
static inline void ColorKey( PackedPixelType *in_ptr, int row_bytes, int height )
{
    int         limit, t=0;
    PackedPixelType	key;
	getWhitePixel( &key );	// 0xff or 0xffff

    limit = (row_bytes / sizeof(PackedPixelType)) * height;

    for( t=0; t<limit; t++)
    {
        if( key == in_ptr[t] )
        {
			in_ptr[t] = 0;
        }
    }
}
#endif


template<typename PixelType > 
static inline void preProcess( PixelType *in_ptr, int row_bytes, int height, PF_Rect *rect, bool is_white_trans )
{
    PixelType key;
	PixelType null_pixel;
	getWhitePixel( &key );	// 0xff or 0x8000
	getNullPixel( &null_pixel );

	int width = (row_bytes / sizeof(PixelType));
	
	int		top=0, left=width, right=0, bottom=0;
	bool	top_found=false, left_found=false;


	if( is_white_trans )
	{
		// 白を抜く
		// Alphaチャンネルは無視して、色が白だったら抜く
		int t=0;
		for(int j=0; j<height; j++)
		{
			if( !top_found )
			{
				top = j;
			}

			for(int i=0; i<width; i++)
			{
				if( key.red == in_ptr[t].red &&
					key.green == in_ptr[t].green &&
					key.blue == in_ptr[t].blue )
				{
					// 抜き色
					in_ptr[t] = null_pixel;
				}
				else if( in_ptr[t].alpha == 0 )
				{
					// すでに抜かれている
				}
				else
				{
					top_found = true;
					left_found = true;

					if( left > i )
					{
						left = i;
					}

					if( right < i )
					{
						right = i;
					}

					if( bottom < j )
					{
						bottom = j;
					}
				}
				t++;
			}
		}
	}
	else
	{
		// 白を抜かずに、領域情報だけ取得
		int t=0;
		for(int j=0; j<height; j++)
		{
			if( !top_found )
			{
				top = j;
			}

			for(int i=0; i<width; i++)
			{
				if(!(key.red == in_ptr[t].red && key.green == in_ptr[t].green && key.blue == in_ptr[t].blue ) &&
					 in_ptr[t].alpha != 0 )
				{
					top_found = true;
					left_found = true;

					if( left > i )
					{
						left = i;
					}

					if( right < i )
					{
						right = i;
					}

					if( bottom < j )
					{
						bottom = j;
					}
				}
				t++;
			}
		}
	}

	if( top_found )
		rect->top = top;
	else
		rect->top = 0;

	if( left_found )
		rect->left = left;
	else
		rect->left = 0;

	rect->right = right+1;
	rect->bottom = bottom+1;


}

/*
//---------------------------------------------------------------------------//
// 概要   : Effectメイン
// 関数名 : EffectPluginMain
// 引数   : 
// 返り値 : 
//---------------------------------------------------------------------------//
DllExport
PF_Err EntryPointFunc(    PF_Cmd          cmd,
                            PF_InData       *in_data,
                            PF_OutData      *out_data,
                            PF_ParamDef     *params[],
                            PF_LayerDef     *output )
{
    PF_Err      err = PF_Err_NONE;
    
    try
    {
        switch (cmd)
        {
            case PF_Cmd_ABOUT:              // Aboutボタンを押した
                err = About(in_data, 
                            out_data, 
                            params, 
                            output);
                break;


            case PF_Cmd_GLOBAL_SETUP:       // Global setup 読み込まれた時1度だけ呼ばれるはず
                err = GlobalSetup(  in_data, 
                                    out_data,
                                    params, 
                                    output);
                break;

            case PF_Cmd_GLOBAL_SETDOWN:     // Global setdown 終了時1度だけ呼ばれるはず
                err = GlobalSetdown(in_data, 
                                    out_data,
                                    params, 
                                    output);
                break;

            case PF_Cmd_PARAMS_SETUP:       // パラメータの設定
                err = ParamsSetup(  in_data, 
                                    out_data, 
                                    params, 
                                    output);
                break;


            case PF_Cmd_RENDER:             // レンダリング
                err = Render(   in_data, 
                                out_data, 
                                params, 
                                output);
                break;

			case PF_Cmd_DO_DIALOG:
				err = PopDialog(in_data,
								out_data,
								params,
								output);
				break;

        }
    }
    catch( APIErr   api_err )
    {   // APIがエラーを返した
        
        PrintAPIErr( &api_err ); // プリント

        err = PF_Err_INTERNAL_STRUCT_DAMAGED;
    }
    catch(...)
    {
        err = PF_Err_INTERNAL_STRUCT_DAMAGED;
    }


    return err;
}
*/
// The first _action_ called after the binary is loaded (three boot strapper functions will be howeever)
OfxStatus LoadAction(void)
{
  // fetch our three suites
  FetchSuite(gPropertySuite,    kOfxPropertySuite,    1);
  FetchSuite(gImageEffectSuite, kOfxImageEffectSuite, 1);
  FetchSuite(gParameterSuite,   kOfxParameterSuite,   1);

  return kOfxStatOK;
}

////////////////////////////////////////////////////////////////////////////////
// the plugin's basic description routine
OfxStatus DescribeAction(OfxImageEffectHandle descriptor)
{
  // get the property set handle for the plugin
  OfxPropertySetHandle effectProps;
  gImageEffectSuite->getPropertySet(descriptor, &effectProps);

  // set some labels and the group it belongs to
  gPropertySuite->propSetString(effectProps,
                                kOfxPropLabel,
                                0,
                                "smooth");
  gPropertySuite->propSetString(effectProps,
                                kOfxImageEffectPluginPropGrouping,
                                0,
                                "Anime Cell FX");
  // define the image effects contexts we can be used in, in this case a simple filter
  gPropertySuite->propSetString(effectProps,
                                kOfxImageEffectPropSupportedContexts,
                                0,
                                kOfxImageEffectContextFilter);

  // set the bit depths the plugin can handle
  gPropertySuite->propSetString(effectProps,
                                kOfxImageEffectPropSupportedPixelDepths,
                                0,
                                kOfxBitDepthFloat);
  gPropertySuite->propSetString(effectProps,
                                kOfxImageEffectPropSupportedPixelDepths,
                                1,
                                kOfxBitDepthShort);
  gPropertySuite->propSetString(effectProps,
                                kOfxImageEffectPropSupportedPixelDepths,
                                2,
                                kOfxBitDepthByte);

  // say that a single instance of this plugin can be rendered in multiple threads
  gPropertySuite->propSetString(effectProps,
                                kOfxImageEffectPluginRenderThreadSafety,
                                0,
                                kOfxImageEffectRenderFullySafe);

  // say that the host should manage SMP threading over a single frame
  gPropertySuite->propSetInt(effectProps,
                             kOfxImageEffectPluginPropHostFrameThreading,
                             0,
                             1);


  return kOfxStatOK;
}

////////////////////////////////////////////////////////////////////////////////
//  describe the plugin in context
OfxStatus
DescribeInContextAction(OfxImageEffectHandle descriptor,
                        OfxPropertySetHandle inArgs)
{
  OfxPropertySetHandle props;
  // define the mandated single output clip
  gImageEffectSuite->clipDefine(descriptor, "Output", &props);

  // set the component types we can handle on out output
  gPropertySuite->propSetString(props,
                                kOfxImageEffectPropSupportedComponents,
                                0,
                                kOfxImageComponentRGBA);
  gPropertySuite->propSetString(props,
                                kOfxImageEffectPropSupportedComponents,
                                1,
                                kOfxImageComponentAlpha);
  gPropertySuite->propSetString(props,
                                kOfxImageEffectPropSupportedComponents,
                                2,
                                kOfxImageComponentRGB);

  // define the mandated single source clip
  gImageEffectSuite->clipDefine(descriptor, "Source", &props);

  // set the component types we can handle on our main input
  gPropertySuite->propSetString(props,
                                kOfxImageEffectPropSupportedComponents,
                                0,
                                kOfxImageComponentRGBA);
  gPropertySuite->propSetString(props,
                                kOfxImageEffectPropSupportedComponents,
                                1,
                                kOfxImageComponentAlpha);
  gPropertySuite->propSetString(props,
                                kOfxImageEffectPropSupportedComponents,
                                2,
                                kOfxImageComponentRGB);

  // first get the handle to the parameter set
  OfxParamSetHandle paramSet;
  gImageEffectSuite->getParamSet(descriptor, &paramSet);

  // properties on our parameter
  OfxPropertySetHandle paramProps;

  // now define a 'gain' parameter and set its properties
  gParameterSuite->paramDefine(paramSet,
                               kOfxParamTypeDouble,
                               RANGE_PARAM_NAME,
                               &paramProps);
  gPropertySuite->propSetString(paramProps,
                                kOfxParamPropDoubleType,
                                0,
                                kOfxParamDoubleTypeScale);
  gPropertySuite->propSetDouble(paramProps,
                                kOfxParamPropDefault,
                                0,
                                1.0);
  gPropertySuite->propSetDouble(paramProps,
                                kOfxParamPropMin,
                                0,
                                0.0);
  gPropertySuite->propSetDouble(paramProps,
                                kOfxParamPropDisplayMin,
                                0,
                                0.0);
  gPropertySuite->propSetDouble(paramProps,
                                kOfxParamPropDisplayMax,
                                0,
                                10.0);
  gPropertySuite->propSetString(paramProps,
                                kOfxPropLabel,
                                0,
                                "Range");
  gPropertySuite->propSetString(paramProps,
                                kOfxParamPropHint,
                                0,
                                "Set range");
  // now define a 'gain' parameter and set its properties
  gParameterSuite->paramDefine(paramSet,
                               kOfxParamTypeDouble,
                               LINE_WEIGHT_PARAM_NAME,
                               &paramProps);
  gPropertySuite->propSetString(paramProps,
                                kOfxParamPropDoubleType,
                                0,
                                kOfxParamDoubleTypeScale);
  gPropertySuite->propSetDouble(paramProps,
                                kOfxParamPropDefault,
                                0,
                                0.0);
  gPropertySuite->propSetDouble(paramProps,
                                kOfxParamPropMin,
                                0,
                                0.0);
  gPropertySuite->propSetDouble(paramProps,
                                kOfxParamPropDisplayMin,
                                0,
                                0.0);
  gPropertySuite->propSetDouble(paramProps,
                                kOfxParamPropDisplayMax,
                                0,
                                10.0);
  gPropertySuite->propSetString(paramProps,
                                kOfxPropLabel,
                                0,
                                "Line Weight");
  gPropertySuite->propSetString(paramProps,
                                kOfxParamPropHint,
                                0,
                                "Set line weight");
  // and define the 'applyToAlpha' parameters and set its properties
  gParameterSuite->paramDefine(paramSet,
                               kOfxParamTypeBoolean,
                               WHITE_OPTION_PARAM_NAME,
                               &paramProps);
  gPropertySuite->propSetInt(paramProps,
                             kOfxParamPropDefault,
                             0,
                             0);
  gPropertySuite->propSetString(paramProps,
                                kOfxParamPropHint,
                                0,
                                "White Option");
  gPropertySuite->propSetString(paramProps,
                                kOfxPropLabel,
                                0,
                                "Transparent");

  return kOfxStatOK;
}

////////////////////////////////////////////////////////////////////////////////
/// instance construction
OfxStatus CreateInstanceAction( OfxImageEffectHandle instance)
{
  OfxPropertySetHandle effectProps;
  gImageEffectSuite->getPropertySet(instance, &effectProps);

  // To avoid continual lookup, put our handles into our instance
  // data, those handles are guaranteed to be valid for the duration
  // of the instance.
  MyInstanceData *myData = new MyInstanceData;

  // Set my private instance data
  gPropertySuite->propSetPointer(effectProps, kOfxPropInstanceData, 0, (void *) myData);

  // Cache the source and output clip handles
  gImageEffectSuite->clipGetHandle(instance, "Source", &myData->sourceClip, 0);
  gImageEffectSuite->clipGetHandle(instance, "Output", &myData->outputClip, 0);

  // Cache away the param handles
  OfxParamSetHandle paramSet;
  gImageEffectSuite->getParamSet(instance, &paramSet);
  gParameterSuite->paramGetHandle(paramSet,
                                  RANGE_PARAM_NAME,
                                  &myData->rangeParam,
                                  0);
  gParameterSuite->paramGetHandle(paramSet,
                                  LINE_WEIGHT_PARAM_NAME,
                                  &myData->lineWeightParam,
                                  0);
  gParameterSuite->paramGetHandle(paramSet,
                                  WHITE_OPTION_PARAM_NAME,
                                  &myData->whiteOptionParam,
                                  0);

  return kOfxStatOK;
}

////////////////////////////////////////////////////////////////////////////////
// instance destruction
OfxStatus DestroyInstanceAction( OfxImageEffectHandle instance)
{
  // get my instance data
  MyInstanceData *myData = FetchInstanceData(instance);
  delete myData;

  return kOfxStatOK;
}
/*
//---------------------------------------------------------------------------//
// 概要   : Aboutボタンを押したときに呼ばれる関数
// 関数名 : About
// 引数   : 
// 返り値 : 
//---------------------------------------------------------------------------//
static PF_Err About (   PF_InData       *in_data,
                        PF_OutData      *out_data,
                        PF_ParamDef     *params[],
                        PF_LayerDef     *output )
{
#if SK_STAGE_DEVELOP
    const char *stage_str= "Debug";
#elif SK_STAGE_RELEASE
    const char *stage_str= "";
#endif

    char str[256];
    memset( str, 0, 256 );

    sprintf(    out_data->return_msg, 
                 "%s, v%d.%d.%d %s\n%s\n",
                NAME, 
                MAJOR_VERSION, 
                MINOR_VERSION,
                BUILD_VERSION,
                stage_str,
                str );

    return PF_Err_NONE;
}



//---------------------------------------------------------------------------//
// 概要   : プラグインが読み込まれた時に呼ばれる関数
// 関数名 : GlobalSetup
// 引数   : 
// 返り値 : 
//---------------------------------------------------------------------------//
static PF_Err GlobalSetup ( PF_InData       *in_data,
                            PF_OutData      *out_data,
                            PF_ParamDef     *params[],
                            PF_LayerDef     *output )
{
    // versionをpiplとあわせないといけないの&&PiPlは直値のみ
    // 使いづらいから使わないので0固定
    out_data->my_version    = PF_VERSION(2,0,0,0,0);    

	// input buffer を加工します
    out_data->out_flags  |= PF_OutFlag_I_WRITE_INPUT_BUFFER | PF_OutFlag_DEEP_COLOR_AWARE;
    out_data->out_flags2 |= PF_OutFlag2_I_AM_THREADSAFE;

    return PF_Err_NONE;
}



static PF_Err GlobalSetdown(PF_InData       *in_data,
                            PF_OutData      *out_data,
                            PF_ParamDef     *params[],
                            PF_LayerDef     *output )
{	
    return PF_Err_NONE;
}
*/

//---------------------------------------------------------------------------//
// 概要   : パラメータの設定
// 関数名 : ParamsSetup
// 引数   : 
// 返り値 : 
//---------------------------------------------------------------------------//
/*static PF_Err ParamsSetup(  PF_InData       *in_data,
                            PF_OutData      *out_data,
                            PF_ParamDef     *params[],
                            PF_LayerDef     *output)
{
    
    PF_ParamDef def;
    AEFX_CLR_STRUCT(def);   // defを初期化 //

    def.param_type = PF_Param_CHECKBOX;
    def.flags = PF_ParamFlag_START_COLLAPSED;
    PF_STRCPY(def.name, "white option");
    def.u.bd.value = def.u.bd.dephault = FALSE;
    def.u.bd.u.nameptr = "transparent"; 
    
    PF_ADD_PARAM(in_data, -1, &def);

    AEFX_CLR_STRUCT(def);


    PF_ADD_FLOAT_SLIDER("range",
                        0.0f,           //VALID_MIN,
                        100.0f,         //VALID_MAX,
                        0.0f,           //SLIDER_MIN,
                        10.0f,          //SLIDER_MAX,
                        1.00f,          //CURVE_TOLERANCE,  よくわかんない
                        1.0f,           //DFLT, デフォルト
                        1,              //DISP  会いたいをそのまま表示
                        0,              //PREC, パーセント表示？,
                        FALSE,          //WANT_PHASE,
                        PARAM_RANGE);   // ID

    PF_ADD_FLOAT_SLIDER("line weight",
                        0.0f,           //VALID_MIN,
                        1.0f,           //VALID_MAX,
                        0.0f,           //SLIDER_MIN,
                        1.0f,           //SLIDER_MAX,
                        1.0f,           //CURVE_TOLERANCE,  よくわかんない
                        0.0f,           //DFLT, デフォルト
                        1,              //DISP  会いたいをそのまま表示
                        0,              //PREC, パーセント表示？,
                        FALSE,          //WANT_PHASE,
                        PARAM_LINE_WEIGHT ); // ID

    // パラメータ数をセット //
    out_data->num_params = PARAM_NUM;

    
    return PF_Err_NONE;
}
*/




//---------------------------------------------------------------------------//
// smoothing実行関数 
// PixelType		PF_Pixel8, PF_Pixel16
// PackedPixelType	KP_PIXEL32,	KP_PIXEL64
//---------------------------------------------------------------------------//
template<typename PixelType, typename PackedPixelType>
static OfxStatus smoothing(OfxImageEffectHandle instance,
						PF_LayerDef *input,
						PF_LayerDef *output,
						PixelType	*in_ptr,
						PixelType	*out_ptr,
            OfxPropertySetHandle sourceImg,
            OfxPropertySetHandle outputImg,
            OfxRectI renderWindow,
            int nComps)
{
  // fetch output image info from the property handle
  int dstRowBytes;
  OfxRectI dstBounds;
  void *dstPtr = NULL;
  gPropertySuite->propGetInt(outputImg, kOfxImagePropRowBytes, 0, &dstRowBytes);
  gPropertySuite->propGetIntN(outputImg, kOfxImagePropBounds, 4, &dstBounds.x1);
  gPropertySuite->propGetPointer(outputImg, kOfxImagePropData, 0, &dstPtr);

  if(dstPtr == NULL) {
    throw "Bad destination pointer";
  }

  // fetch input image info from the property handle
  int srcRowBytes;
  OfxRectI srcBounds;
  void *srcPtr = NULL;
  gPropertySuite->propGetInt(sourceImg, kOfxImagePropRowBytes, 0, &srcRowBytes);
  gPropertySuite->propGetIntN(sourceImg, kOfxImagePropBounds, 4, &srcBounds.x1);
  gPropertySuite->propGetPointer(sourceImg, kOfxImagePropData, 0, &srcPtr);

  if(srcPtr == NULL) {
    throw "Bad source pointer";
  }

	OfxTime time;
    MyInstanceData *myData = FetchInstanceData(instance);
    double range = 1.0;
    double lineWeight = 1.0;
    double weight;
    int whiteOption = 0;
    gParameterSuite->paramGetValueAtTime(myData->rangeParam, time, &range);
    gParameterSuite->paramGetValueAtTime(myData->lineWeightParam, time, &lineWeight);
    gParameterSuite->paramGetValueAtTime(myData->whiteOptionParam, time, &whiteOption);
    //PF_Err	err;
	PF_Rect extent_hint;
    BEGIN_PROFILE();
	

	// 白抜き & 領域情報取得
	preProcess<PixelType>(	in_ptr,
							input->rowbytes, input->height,
							&extent_hint,
							myData->whiteOptionParam );
	
    //err = PF_COPY(input, output, NULL, NULL);

    
    int     in_width,in_height, out_width, out_height, i,j;
    long    in_target, out_target;
    range = (range * (getMaxValue<PixelType>() * 4)) / 100; 
    lineWeight = (lineWeight / 2.0 + 0.5),
                weight;
    bool        lack_flg;

    in_width    = GET_WIDTH(input);
    in_height   = GET_HEIGHT(input);
    out_width   = GET_WIDTH(output);
    out_height  = GET_HEIGHT(output);


    BlendingInfo<PixelType>    blend_info, *info;

    info = &blend_info;

    // 共通部分を初期化
    blend_info.input        = input;
    blend_info.output       = output;
    blend_info.in_ptr       = in_ptr;
    blend_info.out_ptr      = out_ptr;
    blend_info.range        = range;
    blend_info.LineWeight   = lineWeight;
    
	// 領域情報を加工
	if( extent_hint.top == 0 )			extent_hint.top = 1;
	if( extent_hint.left == 0 )			extent_hint.left = 1;
	if( extent_hint.right == in_width )	extent_hint.right -= 1;
	if( extent_hint.bottom == in_height )	extent_hint.bottom -= 1;


    ///////////////// アンチ処理 //////////////////////////////////////////////////
    for( j=extent_hint.top; j<extent_hint.bottom; j++)
    {
        lack_flg = false;

        in_target   = j*in_width+extent_hint.left;
        out_target  = j*out_width+extent_hint.left;

        for( i=extent_hint.left; i<extent_hint.right; i++, in_target++, out_target++)
        {
            // 欠けである可能性あり
            if( lack_flg )
            {
                // フラグ落とす
                lack_flg = false;

                // 設定
                blend_info.i            = i;
                blend_info.j            = j;
                blend_info.in_target    = in_target;
                blend_info.out_target   = out_target;
                blend_info.flag         = 0;

                // 処理
                LackMode0304Execute( &blend_info );
            }

            // 可能性のある角のみに限定する //
			if( FAST_COMPARE_PIXEL(in_target, in_target+1))
            {
                unsigned char   mode_flg = 0;
                
                // 初期化 //

                blend_info.i            = i;
                blend_info.j            = j;
                blend_info.in_target    = in_target;
                blend_info.out_target   = out_target;
                blend_info.flag         = 0;

                memset( &blend_info.core, 0, sizeof(Cinfo)*4 ); // 0フィル

                // ここでアンチを処理 //
                if( ComparePixel(in_target, in_target+1))           (mode_flg |= 1<<0);
                if( ComparePixel(in_target, in_target-in_width))    (mode_flg |= 1<<1);
                if( ComparePixel(in_target, in_target+in_width))    (mode_flg |= 1<<2);
                if( ComparePixel(in_target, in_target-1))           (mode_flg |= 1<<3);


                if( mode_flg != 0 )
                {
                    // 次のピクセルがlackである可能性あり
                    if( i < input->width-2 && (mode_flg & 1<<0))
					{
                        lack_flg = true;
					}
                    
                    switch(mode_flg)
                    {
                    
                        case 3: ////// 上向きの角 /////////////////////////////
                            

                            // 8連結系モードとのバッティングを避ける
                            if( ComparePixelEqual(in_target-in_width,   in_target+1) &&     // 対角が同じで
                                ComparePixel     (in_target-in_width+1, in_target-in_width) &&  // 対角の角がそれぞれ違ういろ
                                ComparePixel     (in_target-in_width+1, in_target+1))
                            {
                                // 処理しない
                                break;
                            }
                    
                    
                            // カウント //
                            upMode_LeftCountLength<PixelType>( &blend_info);
                            
                            upMode_RightCountLength<PixelType>( &blend_info);
                            
                            upMode_TopCountLength<PixelType>( &blend_info);
                            
                            upMode_BottomCountLength<PixelType>( &blend_info);
                            
                    
                            ///// 補正処理 //////////
                            
                            /////// 水平方向 /////////////////////////
                            // start座標 (leftが長い場合しか行わない) //
                            if(blend_info.core[0].length - blend_info.core[1].length == 1)
                            {
                                blend_info.core[0].start -= 0.5f;
                                blend_info.core[1].start -= 0.5f;
                            }
                            
                            if( (blend_info.core[0].flg & CR_FLG_FILL ) || 
                                (blend_info.core[1].flg & CR_FLG_FILL ) )
                            {
                                weight = 0.5f;
                            }
                            else
                            {
                                weight = blend_info.LineWeight;
                            }
                            
                            // end を計算しなおす(Countが出力するend値はLenと同じで0.5がかけられていない)(補正時の再計算で沸けわからなくなるため) //
                            blend_info.core[0].end = blend_info.core[0].start - (blend_info.core[0].start - blend_info.core[0].end) * weight;
                            blend_info.core[1].end = blend_info.core[1].start + (blend_info.core[1].end   - blend_info.core[1].start) * weight;
                            
                            
                            /////// 垂直方向 /////////////////////////
                            // start座標 (bottomが長い場合しか行わない) //
                            if(blend_info.core[3].length - blend_info.core[2].length == 1)
                            {
                                blend_info.core[2].start += 0.5f;
                                blend_info.core[3].start += 0.5f;
                            }
                            
                            if( (blend_info.core[2].flg & CR_FLG_FILL ) || 
                                (blend_info.core[3].flg & CR_FLG_FILL ) )
                            {
                                weight = 0.5f;
                            }
                            else
                            {
                                weight = blend_info.LineWeight;
                            }
                            
                            // end を計算しなおす(Countが出力するend値はLenと同じで0.5がかけられていない)(補正時の再計算で沸けわからなくなるため) //
                            blend_info.core[2].end = blend_info.core[2].start - (blend_info.core[2].start - blend_info.core[2].end) * weight;
                            blend_info.core[3].end = blend_info.core[3].start + (blend_info.core[3].end   - blend_info.core[3].start) * weight;
                            
#if 0                       // 補正処理なし
                            {
                                blend_info.core[0].start    = (float)(i+1); // 画像の座標は左上が(0,0), でも論理線は右からだから+1
                                blend_info.core[0].end      = blend_info.core[0].start - (float)blend_info.core[0].length * 0.5f;
                                
                                blend_info.core[1].start    = (float)(i+1); // 画像の座標は左上が(0,0), でも論理線は右からだから+1
                                blend_info.core[1].end      = blend_info.core[1].start + (float)blend_info.core[1].length * 0.5f;
                                    
                                blend_info.core[2].start    = (float)(j);
                                blend_info.core[2].end      = blend_info.core[2].start - (float)blend_info.core[2].length * 0.5f;
                                
                                blend_info.core[3].start    = (float)(j);
                                blend_info.core[3].end      = blend_info.core[3].start + (float)blend_info.core[3].length * 0.5f;
                            }
#endif              
                    
                            if( blend_info.core[0].length >= 2 && // left >= 2 && bottom >= 2
                                blend_info.core[3].length >= 2)
                            {
                                // 欠けモード2
                                LackMode02Execute( &blend_info );
                            }
                            else if(blend_info.core[1].length > 0)  // Left > 0 && Right > 0
                            {   ////////// 水平方向のブレンド /////////////
                            
                                blend_info.mode = BLEND_MODE_UP_H;  // モード設定
                    
                                // ブレンド //
                                upMode_LeftBlending<PixelType>( &blend_info);
                               
                                upMode_RightBlending<PixelType>( &blend_info);
                    
                                if(blend_info.core[2].length > 1)
                                {   //// 同時に垂直方向も存在 //////
                                    upMode_TopBlending<PixelType>( &blend_info);
                                
                                    upMode_BottomBlending<PixelType>( &blend_info);
                                }
                                
                            }
                            else if(blend_info.core[2].length > 0)  // bottom > 0 && top > 0
                            {   //////////// 垂直方向のブレンド /////////////
                            
                                blend_info.mode = BLEND_MODE_UP_V;  // モード設定
                                
                            
                                upMode_TopBlending<PixelType>( &blend_info);
                                
                                upMode_BottomBlending<PixelType>( &blend_info);
                            }
                    
                            break;
                    
                    
                    
                    
                        case 5: //// 下向きの角 ///////////////////////////////////
                            // 8連結系モードとのバッティングを避ける
                            if( ComparePixelEqual(in_target+in_width,   in_target+1) &&     // 対角が同じで
                                ComparePixel     (in_target+in_width+1, in_target+in_width) &&  // 対角の角がそれぞれ違ういろ
                                ComparePixel     (in_target+in_width+1, in_target+1))
                            {
                                // 処理しない
                                break;
                            }
                    
                    
                            ////// カウント //////////
                            downMode_LeftCountLength<PixelType>( &blend_info);
                            
                            downMode_RightCountLength<PixelType>( &blend_info);
                            
                            downMode_TopCountLength<PixelType>( &blend_info);
                            
                            downMode_BottomCountLength<PixelType>( &blend_info);
                            
                            
                            
                            ///// 補正処理 //////////
                            /////// 水平方向 /////////////////////////
                            // start座標 (leftが長い場合しか行わない) //
                            if(blend_info.core[0].length - blend_info.core[1].length == 1)
                            {
                                blend_info.core[0].start -= 0.5f;
                                blend_info.core[1].start -= 0.5f;
                            }
                            
                            if( (blend_info.core[0].flg & CR_FLG_FILL ) || 
                                (blend_info.core[1].flg & CR_FLG_FILL ) )
                            {
                                weight = 0.5f;
                            }
                            else
                            {
                                weight = blend_info.LineWeight;
                            }
                            
                            // end を計算しなおす(Countが出力するend値はLenと同じで0.5がかけられていない)(補正時の再計算で沸けわからなくなるため) //
                            blend_info.core[0].end = blend_info.core[0].start - (blend_info.core[0].start - blend_info.core[0].end)  * weight;
                            blend_info.core[1].end = blend_info.core[1].start + (blend_info.core[1].end   - blend_info.core[1].start)  * weight;
                            
                            
                            /////// 垂直方向 /////////////////////////
                            // start座標 (bottomが長い場合しか行わない) //
                            if(blend_info.core[3].length - blend_info.core[2].length == 1)
                            {
                                blend_info.core[2].start += 0.5f;
                                blend_info.core[3].start += 0.5f;
                            }
                            
                            if( (blend_info.core[2].flg & CR_FLG_FILL ) || 
                                (blend_info.core[3].flg & CR_FLG_FILL ) )
                            {
                                weight = 0.5f;
                            }
                            else
                            {
                                weight = blend_info.LineWeight;
                            }
                    
                            // end を計算しなおす(Countが出力するend値はLenと同じで0.5がかけられていない)(補正時の再計算で沸けわからなくなるため) //
                            blend_info.core[2].end = blend_info.core[2].start - (blend_info.core[2].start - blend_info.core[2].end)  * weight;
                            blend_info.core[3].end = blend_info.core[3].start + (blend_info.core[3].end   - blend_info.core[3].start)  * weight;
                            
                            
#if 0                       // 補正処理なし                     
                            {
                                blend_info.core[0].start    = (float)(i+1); // 画像の座標は左上が(0,0), でも論理線は右からだから+1
                                blend_info.core[0].end      = blend_info.core[0].start - (float)blend_info.core[0].length * 0.5f;
                                
                                blend_info.core[1].start    = (float)(i+1); // 画像の座標は左上が(0,0), でも論理線は右からだから+1
                                blend_info.core[1].end      = blend_info.core[1].start + (float)blend_info.core[1].length * 0.5f;
                                    
                                blend_info.core[2].start    = (float)(j);
                                blend_info.core[2].end      = blend_info.core[2].start - (float)blend_info.core[2].length * 0.5f;
                                
                                blend_info.core[3].start    = (float)(j);
                                blend_info.core[3].end      = blend_info.core[3].start + (float)blend_info.core[3].length * 0.5f;
                            }
#endif              
                    
                            ////// ブレンド ///////
                            if( blend_info.core[0].length >= 2 && // left >= 2 && top >= 2
                                blend_info.core[2].length >= 2)
                            {
                                // 欠けモード1
                                LackMode01Execute( &blend_info );
                            }
                            else if(blend_info.core[1].length > 0)  // Left > 0 && Right > 0
                            {   ////////// 水平方向のブレンド /////////////
                            
                                blend_info.mode = BLEND_MODE_UP_H;  // モード設定
                    
                                // ブレンド //
                                downMode_LeftBlending<PixelType>( &blend_info);
                               
                                downMode_RightBlending<PixelType>( &blend_info);
                    
                                if(blend_info.core[3].length > 1)
                                {   //// 同時に垂直方向も存在 //////
                                    downMode_TopBlending<PixelType>( &blend_info);
                                
                                    downMode_BottomBlending<PixelType>( &blend_info);
                                }
                                
                            }
                            else if(blend_info.core[3].length > 0)  // bottom > 0 && top > 0
                            {   //////////// 垂直方向のブレンド /////////////
                            
                                blend_info.mode = BLEND_MODE_UP_V;  // モード設定
                                
                            
                                downMode_TopBlending<PixelType>( &blend_info);
                                
                                downMode_BottomBlending<PixelType>( &blend_info);
                            }

                            break;
                        //// 突起1 コ  2 п  3 ヒ  4 ш
                        case 7: ////// 突起1
                            Link8Mode01Execute(&blend_info);
                            break;
                    
                        case 11: ////// 突起2
                            Link8Mode02Execute(&blend_info);
                            break;
                        
                        case 13: ////// 突起4
                            Link8Mode04Execute(&blend_info);
                            break;
                    
                    
                    
                        case 15: ////// 四角のピクセル
                            Link8SquareExecute(&blend_info);
                            break;
                    
                        default:
                            break;
                    }
                    
					// 突起mode3
                    if(i < input->width-2)
                    {
                        // 初期化 //
                        blend_info.i            = i+1;
                        blend_info.j            = j;
                        blend_info.in_target    = in_target+1;
                        blend_info.out_target   = out_target+1;
                        blend_info.flag         = 0;
                        
                        mode_flg = 0;
                        if( ComparePixel(blend_info.in_target, blend_info.in_target-in_width))  (mode_flg |= 1<<0);
                        if( ComparePixel(blend_info.in_target, blend_info.in_target+in_width))  (mode_flg |= 1<<1);
                        if( ComparePixel(blend_info.in_target, blend_info.in_target+1))         (mode_flg |= 1<<2);
                    
                        if(3==mode_flg)
                        {
                            // 突起 3 ヒ
                            Link8Mode03Execute(&blend_info);
                        }
                    }
                }

            }

        }
    }
	
	DEBUG_PIXEL( out_ptr, output, extent_hint.left, extent_hint.top );
	DEBUG_PIXEL( out_ptr, output, extent_hint.left, extent_hint.bottom );
	DEBUG_PIXEL( out_ptr, output, extent_hint.right, extent_hint.top );
	DEBUG_PIXEL( out_ptr, output, extent_hint.right, extent_hint.bottom );


    END_PROFILE();

	//return err;
}


//---------------------------------------------------------------------------//
// 概要   : レンダリング
// 関数名 : Render
// 引数   : 
// 返り値 : 
//---------------------------------------------------------------------------//
/*
static OfxStatus Render(OfxImageEffectHandle instance,
                        PF_InData       *in_data,
                        PF_OutData      *out_data,
                        PF_LayerDef *output )
{
  PF_Err err = PF_Err_NONE;
	PF_LayerDef *input  = 0;

	PF_Pixel16	*in_ptr16, *out_ptr16;
	PF_GET_PIXEL_DATA16(output, NULL, &out_ptr16 );
	PF_GET_PIXEL_DATA16(input, NULL, &in_ptr16 );

	if( out_ptr16 != NULL && in_ptr16 != NULL )
	{
		// 16bpc or 32bpc
		err = smoothing<PF_Pixel16, KP_PIXEL64>(instance, 
												input, output, in_ptr16, out_ptr16 );
	}
	else
	{
		// 8bpc
		PF_Pixel8	*in_ptr8, *out_ptr8;
		PF_GET_PIXEL_DATA8(output, NULL, &out_ptr8 );
		PF_GET_PIXEL_DATA8(input, NULL, &in_ptr8 );
		
		err = smoothing<PF_Pixel8, KP_PIXEL32>(instance,
												input, output, in_ptr8, out_ptr8 );
	}

	return err;


}

// Look up a pixel in the image. returns null if the pixel was not
// in the bounds of the image
template <class T>
static inline T * pixelAddress(int x, int y,
                               void *baseAddress,
                               OfxRectI bounds,
                               int rowBytes,
                               int nCompsPerPixel)
{
  // Inside the bounds of this image?
  if(x < bounds.x1 || x >= bounds.x2 || y < bounds.y1 || y >= bounds.y2)
    return NULL;

  // turn image plane coordinates into offsets from the bottom left
  int yOffset = y - bounds.y1;
  int xOffset = x - bounds.x1;

  // Find the start of our row, using byte arithmetic
  void *rowStartAsVoid = reinterpret_cast<char *>(baseAddress) + yOffset * rowBytes;

  // turn the row start into a pointer to our data type
  T *rowStart = reinterpret_cast<T *>(rowStartAsVoid);

  // finally find the position of the first component of column
  return rowStart + (xOffset * nCompsPerPixel);
}

// iterate over our pixels and process them
template <class T, int MAX>
void PixelProcessing(OfxImageEffectHandle instance,
                     OfxPropertySetHandle sourceImg,
                     OfxPropertySetHandle outputImg,
                     OfxRectI renderWindow,
                     int nComps)
{
  // fetch output image info from the property handle
  int dstRowBytes;
  OfxRectI dstBounds;
  void *dstPtr = NULL;
  gPropertySuite->propGetInt(outputImg, kOfxImagePropRowBytes, 0, &dstRowBytes);
  gPropertySuite->propGetIntN(outputImg, kOfxImagePropBounds, 4, &dstBounds.x1);
  gPropertySuite->propGetPointer(outputImg, kOfxImagePropData, 0, &dstPtr);

  if(dstPtr == NULL) {
    throw "Bad destination pointer";
  }

  // fetch input image info from the property handle
  int srcRowBytes;
  OfxRectI srcBounds;
  void *srcPtr = NULL;
  gPropertySuite->propGetInt(sourceImg, kOfxImagePropRowBytes, 0, &srcRowBytes);
  gPropertySuite->propGetIntN(sourceImg, kOfxImagePropBounds, 4, &srcBounds.x1);
  gPropertySuite->propGetPointer(sourceImg, kOfxImagePropData, 0, &srcPtr);

  if(srcPtr == NULL) {
    throw "Bad source pointer";
  }

  // and do some inverting
  for(int y = renderWindow.y1; y < renderWindow.y2; y++) {
    if(y % 20 == 0 && gImageEffectSuite->abort(instance)) break;

    // get the row start for the output image
    T *dstPix = pixelAddress<T>(renderWindow.x1, y, dstPtr, dstBounds, dstRowBytes, nComps);

    for(int x = renderWindow.x1; x < renderWindow.x2; x++) {

      // get the source pixel
      T *srcPix = pixelAddress<T>(x, y, srcPtr, srcBounds, srcRowBytes, nComps);

      if(srcPix) {
        // we have one, iterate each component in the pixels
        for(int i = 0; i < nComps; ++i) {
          if(i != 3) { // We don't invert alpha.
            *dstPix = MAX - *srcPix; // invert
          }
          else {
            *dstPix = *srcPix;
          }
          ++dstPix; ++srcPix;
        }
      }
      else {
        // we don't have a pixel in the source image, set output to black
        for(int i = 0; i < nComps; ++i) {
          *dstPix = 0;
          ++dstPix;
        }
      }
    }
  }
}
*/

////////////////////////////////////////////////////////////////////////////////
// Render an output image
OfxStatus RenderAction( OfxImageEffectHandle instance,
                        OfxPropertySetHandle inArgs,
                        OfxPropertySetHandle outArgs,PF_LayerDef *output)
{
  // get the render window and the time from the inArgs
  OfxTime time;
  OfxRectI renderWindow;
  OfxStatus status = kOfxStatOK;

  gPropertySuite->propGetDouble(inArgs, kOfxPropTime, 0, &time);
  gPropertySuite->propGetIntN(inArgs, kOfxImageEffectPropRenderWindow, 4, &renderWindow.x1);

  // fetch output clip
  OfxImageClipHandle outputClip;
  gImageEffectSuite->clipGetHandle(instance, "Output", &outputClip, NULL);

  // fetch main input clip
  OfxImageClipHandle sourceClip;
  gImageEffectSuite->clipGetHandle(instance, "Source", &sourceClip, NULL);

  // the property sets holding our images
  OfxPropertySetHandle outputImg = NULL, sourceImg = NULL;
  try {
    // fetch image to render into from that clip
    OfxPropertySetHandle outputImg;
    if(gImageEffectSuite->clipGetImage(outputClip, time, NULL, &outputImg) != kOfxStatOK) {
      throw " no output image!";
    }

    // fetch image at render time from that clip
    if (gImageEffectSuite->clipGetImage(sourceClip, time, NULL, &sourceImg) != kOfxStatOK) {
      throw " no source image!";
    }

    // figure out the data types
    char *cstr;
    gPropertySuite->propGetString(outputImg, kOfxImageEffectPropComponents, 0, &cstr);
    std::string components = cstr;

    // how many components per pixel?
    int nComps = 0;
    if(components == kOfxImageComponentRGBA) {
      nComps = 4;
    }
    else if(components == kOfxImageComponentRGB) {
      nComps = 3;
    }
    else if(components == kOfxImageComponentAlpha) {
      nComps = 1;
    }
    else {
      throw " bad pixel type!";
    }

    // now do our render depending on the data type
    gPropertySuite->propGetString(outputImg, kOfxImageEffectPropPixelDepth, 0, &cstr);
    std::string dataType = cstr;

    //PF_Err err = PF_Err_NONE;
	  PF_LayerDef *input  = 0;
    //PF_InData       *in_data;
    //PF_LayerDef *output;
	  PF_Pixel16	*in_ptr16, *out_ptr16;
	  //PF_GET_PIXEL_DATA16(output, NULL, &out_ptr16 );
	  //PF_GET_PIXEL_DATA16(input, NULL, &in_ptr16 );

	  if(dataType == kOfxBitDepthByte)
	  {
	  	// 8bpc
	  	PF_Pixel8	*in_ptr8, *out_ptr8;
	  	//PF_GET_PIXEL_DATA8(output, NULL, &out_ptr8 );
	  	//PF_GET_PIXEL_DATA8(input, NULL, &in_ptr8 );
  
	  	smoothing<PF_Pixel8, KP_PIXEL32>(instance,
	  											input, output, in_ptr8, out_ptr8, sourceImg, outputImg, renderWindow, nComps);
	  }
	  else 
    {
	  	// 16bpc or 32bpc
	  	smoothing<PF_Pixel16, KP_PIXEL64>(instance, 
	  											input, output, in_ptr16, out_ptr16, sourceImg, outputImg, renderWindow, nComps);
	  }
    /*if(dataType == kOfxBitDepthByte) {
      PixelProcessing<unsigned char, 255>(instance, sourceImg, outputImg, renderWindow, nComps);
    }
    else if(dataType == kOfxBitDepthShort) {
      PixelProcessing<unsigned short, 65535>(instance, sourceImg, outputImg, renderWindow, nComps);
    }
    else if (dataType == kOfxBitDepthFloat) {
      PixelProcessing<float, 1>(instance, sourceImg, outputImg, renderWindow, nComps);
    }
    else {
      throw " bad data type!";
      throw 1;
    }*/

  }
  catch(const char *errStr ) {
    bool isAborting = gImageEffectSuite->abort(instance);

    // if we were interrupted, the failed fetch is fine, just return kOfxStatOK
    // otherwise, something wierd happened
    if(!isAborting) {
      status = kOfxStatFailed;
    }
    ERROR_IF(!isAborting, " Rendering failed because %s", errStr);

  }

  if(sourceImg)
    gImageEffectSuite->clipReleaseImage(sourceImg);
  if(outputImg)
    gImageEffectSuite->clipReleaseImage(outputImg);

  // all was well
  return status;
}





/*
//---------------------------------------------------------------------------//
// ダイアログ作成
//---------------------------------------------------------------------------//
static PF_Err 
PopDialog (	
	PF_InData		*in_data,
	PF_OutData		*out_data,
	PF_ParamDef		*params[],
	PF_LayerDef		*output )
{
	PF_Err err = PF_Err_NONE;
 
	char str[256];
    memset( str, 0, 256 );

    sprintf(    out_data->return_msg, 
                 "%s, v%d.%d.%d\n%s\n",
                NAME, 
                MAJOR_VERSION, 
                MINOR_VERSION,
                BUILD_VERSION,
                str );

	return err;
}
*/
// are the settings of the effect making it redundant and so not do anything to the image data
OfxStatus IsIdentityAction(OfxImageEffectHandle instance,
                           OfxPropertySetHandle inArgs,
                           OfxPropertySetHandle outArgs)
{
  // we set the name of the input clip to pull data from
  gPropertySuite->propSetString(outArgs, kOfxPropName, 0, "Source");
  return kOfxStatOK;
}
////////////////////////////////////////////////////////////////////////////////
// Call back passed to the host in the OfxPlugin struct to set our host pointer
//
// This must be called AFTER both OfxGetNumberOfPlugins and OfxGetPlugin, but
// before the pluginMain entry point is ever touched.
void SetHostFunc(OfxHost *hostStruct)
{
  gHost = hostStruct;
}

////////////////////////////////////////////////////////////////////////////////
// The main entry point function, the host calls this to get the plugin to do things.
OfxStatus MainEntryPoint(const char *action, const void *handle, OfxPropertySetHandle inArgs,  OfxPropertySetHandle outArgs)
{
  MESSAGE(": START action is : %s \n", action );
  // cast to appropriate type
  OfxImageEffectHandle effect = (OfxImageEffectHandle) handle;
  PF_Err      err = PF_Err_NONE;
  PF_LayerDef *output;
  OfxStatus returnStatus = kOfxStatReplyDefault;
  if(strcmp(action, kOfxActionLoad) == 0) {
    // The very first action called on a plugin.
    returnStatus = LoadAction();
  }
  else if(strcmp(action, kOfxActionDescribe) == 0) {
    // the first action called to describe what the plugin does
    returnStatus = DescribeAction(effect);
  }
  else if(strcmp(action, kOfxImageEffectActionDescribeInContext) == 0) {
    // the second action called to describe what the plugin does in a specific context
    returnStatus = DescribeInContextAction(effect, inArgs);
  }
  else if(strcmp(action, kOfxActionCreateInstance) == 0) {
    // the action called when an instance of a plugin is created
    returnStatus = CreateInstanceAction(effect);
  }
  else if(strcmp(action, kOfxActionDestroyInstance) == 0) {
    // the action called when an instance of a plugin is destroyed
    returnStatus = DestroyInstanceAction(effect);
  }
  else if(strcmp(action, kOfxImageEffectActionIsIdentity) == 0) {
    // Check to see if our param settings cause nothing to happen
    returnStatus = IsIdentityAction(effect, inArgs, outArgs);
  }
  else if(strcmp(action, kOfxImageEffectActionRender) == 0) {
    // action called to render a frame
    returnStatus = RenderAction(effect, inArgs, outArgs,output);
  }

  MESSAGE(": END action is : %s \n", action );
  /// other actions to take the default value
  return returnStatus;
}
////////////////////////////////////////////////////////////////////////////////
// The plugin struct passed back to the host application to initiate bootstrapping
// of plugin communications
static OfxPlugin effectPluginStruct =
{
  kOfxImageEffectPluginApi,                // The API this plugin satisfies.
  1,                                       // The version of the API it satisifes.
  "net.studio-mizutama:smooth",     // The unique ID of this plugin.
  1,                                       // The major version number of this plugin.
  0,                                       // The minor version number of this plugin.
  SetHostFunc,                             // Function used to pass back to the plugin the OFXHost struct.
  MainEntryPoint                           // The main entry point to the plugin where all actions are passed to.
};

////////////////////////////////////////////////////////////////////////////////
// The first of the two functions that a host application will look for
// after loading the binary, this function returns the number of plugins within
// this binary.
//
// This will be the first function called by the host.
EXPORT int OfxGetNumberOfPlugins(void)
{
  return 1;
}

////////////////////////////////////////////////////////////////////////////////
// The second of the two functions that a host application will look for
// after loading the binary, this function returns the 'nth' plugin declared in
// this binary.
//
// This will be called multiple times by the host, once for each plugin present.
EXPORT OfxPlugin * OfxGetPlugin(int nth)
{
  if(nth == 0)
    return &effectPluginStruct;
  return 0;
}