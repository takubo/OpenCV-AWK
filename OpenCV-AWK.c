#include <cv.h>
#include <highgui.h>
#include <stdio.h>
#include <string.h>
#include "awk.h"

int plugin_is_GPL_compatible;



IplImage *img;
//CvCapture *capture;
//IplImage *frame;


/* do_acvDetectObjects内で使用するstorage */
/* 毎回メモリを確保したくないので、静的に確保しておく。 */
/* その代り、non-reentrant */
CvMemStorage *detect_obj_storage;


#define IMG_NUM 64
#define CPT_NUM 8
#define VWR_NUM 8
#define FNT_NUM 64
typedef struct ICV {
//	union {
//		IplImage *img;
//		CvCapture *capture;
//		CvVideoWriter *vw;
//		CvFont *font;
//	} ptr;
	void *ptr;
	int capture;
	char filename[32];
} ICV;
struct ICV ImgPtr[IMG_NUM] = { { 0 } };
struct ICV CapPtr[CPT_NUM] = { { 0 } };
struct ICV AvwPtr[VWR_NUM] = { { 0 } };
struct ICV FntPtr[FNT_NUM] = { { 0 } };

#if 0
#define resist_image(fn, objptr)	resist_objptr((fn), (objptr), ImgPtr, IMG_NUM)
#define resist_capture(fn, objptr)	resist_objptr((fn), (objptr), CapPtr, CPT_NUM)
#define resist_videowriter(fn, objptr)	resist_objptr((fn), (objptr), AvwPtr, VWR_NUM)
static int
resist_objptr(const char *fn, void *videowriter, ICV *table, int tbl_size)
#else
#define resist_image(objptr)		resist_objptr((objptr), ImgPtr, IMG_NUM, "Image", FALSE)
#define resist_capture_image(objptr)	resist_objptr((objptr), ImgPtr, IMG_NUM, "Image", TRUE)
#define resist_capture(objptr)		resist_objptr((objptr), CapPtr, CPT_NUM, "Capture", FALSE)
#define resist_videowriter(objptr)	resist_objptr((objptr), AvwPtr, VWR_NUM, "VideoWriter", FALSE)
#define resist_font(objptr)		resist_objptr((objptr), FntPtr, FNT_NUM, "Font", FALSE)
static const char *
resist_objptr(void *objptr, ICV *table, int tbl_size, const char * prefix, int capture)
#endif
{
	int i;

	for (i = 0; i < tbl_size; i++) {
		if (table[i].ptr == NULL) {
			sprintf(table[i].filename, "%s%d", prefix, i);
			table[i].ptr = objptr;
			if (capture)
				table[i].capture = TRUE;
			return table[i].filename;
		}
	}

	return NULL;
}

#define lookup_image(fn)		lookup_objptr((fn), ImgPtr, IMG_NUM)
#define lookup_capture(fn)		lookup_objptr((fn), CapPtr, CPT_NUM)
#define lookup_videowriter(fn)		lookup_objptr((fn), AvwPtr, VWR_NUM)
#define lookup_font(fn)			lookup_objptr((fn), FntPtr, FNT_NUM)
static void *
lookup_objptr(const char *fn, ICV *table, int tbl_size)
{
	int i;

	for (i = 0; i < tbl_size; i++) {
		if (strcmp(table[i].filename, fn) == 0) {
			return table[i].ptr;
		}
	}

	return NULL;
}
static char *
lookup_image_image(IplImage *img)
{
	int i;

	for (i = 0; i < IMG_NUM; i++) {
		if (ImgPtr[i].ptr == img) {
			return ImgPtr[i].filename;
		}
	}

	return NULL;
}

#define delete_image(fn)	delete_objptr((fn), ImgPtr, IMG_NUM)
#define delete_capture(fn)	delete_objptr((fn), CapPtr, CPT_NUM)
#define delete_videowriter(fn)	delete_objptr((fn), AvwPtr, VWR_NUM)
#define delete_font(fn)		delete_objptr((fn), FntPtr, FNT_NUM)
static void
delete_objptr(const char *fn, ICV *table, int tbl_size)
{
	int i;

	for (i = 0; i < tbl_size; i++) {
		if (strcmp(table[i].filename, fn) == 0) {
			table[i].ptr = NULL;
		}
	}

	return;
}




/************/
/* High GUI */
/************/


/***** User Interface ******/

static NODE *
do_cvNamedWindow(int nags)
{
	NODE *tmp;
	char *name;

	tmp = (NODE*) get_scalar_argument(0, FALSE);
	force_string(tmp);
	name = tmp->stptr;

	cvNamedWindow(name, CV_WINDOW_AUTOSIZE);
	return make_number((AWKNUM) 0);
}

static NODE *
do_cvResizeWindow(int nags)
{
	NODE *tmp;
	char *name;
	int width, height;

	tmp = (NODE*) get_actual_argument(0, FALSE, FALSE);
	force_string(tmp);
	name = tmp->stptr;

	tmp = (NODE*) get_actual_argument(1, FALSE, FALSE);
	width = (int) force_number(tmp);

	tmp = (NODE*) get_actual_argument(2, FALSE, FALSE);
	height = (int) force_number(tmp);

	cvResizeWindow(name, width, height);
	return make_number((AWKNUM) 0);
}

static NODE *
do_cvMoveWindow(int nags)
{
	NODE *tmp;
	char *name;
	int x, y;

	tmp = (NODE*) get_actual_argument(0, FALSE, FALSE);
	force_string(tmp);
	name = tmp->stptr;

	tmp = (NODE*) get_actual_argument(1, FALSE, FALSE);
	x = (int) force_number(tmp);

	tmp = (NODE*) get_actual_argument(2, FALSE, FALSE);
	y = (int) force_number(tmp);

	cvMoveWindow(name, x, y);
	return make_number((AWKNUM) 0);
}

static NODE *
do_cvDestroyWindow(int nags)
{
	NODE *tmp;
	char *name;

	tmp = (NODE*) get_actual_argument(0, FALSE, FALSE);
	force_string(tmp);
	name = tmp->stptr;

	cvDestroyWindow(name);
	return make_number((AWKNUM) 0);
}

static NODE *
do_cvShowImage(int nags)
{
	NODE *tmp;
	char *window_name;
	IplImage *img;

	tmp = (NODE*) get_actual_argument(0, FALSE, FALSE);
	force_string(tmp);
	window_name = tmp->stptr;

	tmp = (NODE*) get_actual_argument(1, FALSE, FALSE);
	force_string(tmp);
	img = lookup_image(tmp->stptr);

	cvShowImage(window_name, img);
	return make_number((AWKNUM) 0);
}

NODE *mouse_callback_user_func = NULL;

// CV_EVENT_MOUSEMOVE マウスが移動した
// CV_EVENT_LBUTTONDOWN 左ボタンが押された
// CV_EVENT_RBUTTONDOWN 右ボタンが押された
// CV_EVENT_MBUTTONDOWN 中ボタンが押された
// CV_EVENT_LBUTTONUP 左ボタンが離された
// CV_EVENT_RBUTTONUP 右ボタンが離された
// CV_EVENT_MBUTTONUP 中ボタンが離された
// CV_EVENT_LBUTTONDBLCLK 左ボタンがダブルクリックされた
// CV_EVENT_RBUTTONDBLCLK 右ボタンがダブルクリックされた
// CV_EVENT_MBUTTONDBLCLK 中ボタンがダブルクリックされた
static const char *
event2str(int event)
{
	switch (event) {
	case CV_EVENT_MOUSEMOVE:
		return "MOUSEMOVE";
		break;
	case CV_EVENT_LBUTTONDOWN:
		return "LBUTTONDOWN";
		break;
	case CV_EVENT_RBUTTONDOWN:
		return "RBUTTONDOWN";
		break;
	case CV_EVENT_MBUTTONDOWN:
		return "MBUTTONDOWN";
		break;
	case CV_EVENT_LBUTTONUP:
		return "LBUTTONUP";
		break;
	case CV_EVENT_RBUTTONUP:
		return "RBUTTONUP";
		break;
	case CV_EVENT_MBUTTONUP:
		return "MBUTTONUP";
		break;
	case CV_EVENT_LBUTTONDBLCLK:
		return "LBUTTONDBLCLK";
		break;
	case CV_EVENT_RBUTTONDBLCLK:
		return "RBUTTONDBLCLK";
		break;
	case CV_EVENT_MBUTTONDBLCLK:
		return "MBUTTONDBLCLK";
		break;
	}
}

// CV_EVENT_FLAG_LBUTTON 左ボタンが押されている
// CV_EVENT_FLAG_RBUTTON 右ボタンが押されている
// CV_EVENT_FLAG_MBUTTON 中ボタンが押されている
// CV_EVENT_FLAG_CTRLKEY Ctrl キーが押されている
// CV_EVENT_FLAG_SHIFTKEY Shift キーが押されている
// CV_EVENT_FLAG_ALTKEY Alt キーが押されている
static void
flag2str(int flags, char * str)
{
	if (flags & CV_EVENT_FLAG_LBUTTON) { *str++ = 'L'; }
	if (flags & CV_EVENT_FLAG_RBUTTON) { *str++ = 'R'; }
	if (flags & CV_EVENT_FLAG_MBUTTON) { *str++ = 'M'; }
	if (flags & CV_EVENT_FLAG_CTRLKEY) { *str++ = 'C'; }
	if (flags & CV_EVENT_FLAG_SHIFTKEY){ *str++ = 'S'; }
	if (flags & CV_EVENT_FLAG_ALTKEY)  { *str++ = 'A'; }

	*str = '\0';
	return;
}

static void
onMouse(int event, int x, int y, int flags, void * prm)
{
	static INSTRUCTION *code = NULL;
	extern int currule;
	extern int exiting;
	char *event_str;
	char flags_str[8];

	if (code == NULL) {
		/* make function call instructions */
		code = bcalloc(Op_func_call, 2, 0);
		code->func_name = NULL;		/* not needed, func_body will assign */
		code->nexti = bcalloc(Op_stop, 1, 0);
	}

	code->func_body = mouse_callback_user_func;
	(code + 1)->expr_count = 5;	/* function takes num of 5 arguments */

	(code + 1)->inrule = currule;	/* save current rule */
	currule = 0;

	event_str = event2str(event);
	PUSH(make_string(event_str, strlen(event_str)));
	PUSH(make_number((AWKNUM) x));
	PUSH(make_number((AWKNUM) y));
	flag2str(flags, flags_str);
	PUSH(make_string(flags_str, strlen(flags_str)));
	PUSH(make_number((AWKNUM) 0)); // TODO

	interpret(code);
	if (exiting)	/* do not assume anything about the user-defined function! */
		gawk_exit(exit_val);

	currule = (code + 1)->inrule;   /* restore current rule */ 
	return;
}

static NODE *
do_cvSetMouseCallback(int nargs)
{
	NODE *tmp;
	char *window_name;
	NODE *func_ptr;
	void *prm = NULL;

	tmp = (NODE*) get_scalar_argument(0, FALSE);
	force_string(tmp);
	window_name = tmp->stptr;

	tmp = (NODE*) get_scalar_argument(1, FALSE);
	force_string(tmp);
	func_ptr = lookup(tmp->stptr);
	if (func_ptr == NULL || func_ptr->type != Node_func)
		fatal(_("Callback function `%s' is not defined"), tmp->stptr);

	if ((tmp = (NODE*) get_scalar_argument(2, TRUE)) != NULL) {
		force_string(tmp);
		window_name = tmp->stptr;
		//TODO
	}

	mouse_callback_user_func = func_ptr;
	cvSetMouseCallback(window_name, onMouse, prm);
	return make_number((AWKNUM) 0);
}

static NODE *
do_cvWaitKey(int nags)
{
	NODE *tmp;
	int delay;

	if ((tmp = (NODE*) get_scalar_argument(0, FALSE)) != NULL) {
		delay = (int) force_number(tmp);
	} else {
		delay = 0;
	}

	//TODO Return a key-code
	return make_number((AWKNUM) cvWaitKey(delay));
}

static NODE *
do_ascii(int nags)
{
	NODE *tmp;

	if  (do_lint && get_curfunc_arg_count() > 1)
		lintwarn("ascii: called with too many arguments");

	if((tmp = get_scalar_argument(0, FALSE)) == NULL) {
		lintwarn("ascii: called with no arguments");
		return make_number((AWKNUM) -1);
	}

	force_string(tmp);
	return make_number((AWKNUM) tmp->stptr[0]);
}

static NODE *
do_char(int nags)
{
	NODE *tmp;
	int num;
	char buf[2];

	if  (do_lint && get_curfunc_arg_count() > 1)
		lintwarn("char: called with too many arguments");

	if((tmp = get_scalar_argument(0, FALSE)) == NULL) {
		lintwarn("char: called with no arguments");
		return make_number((AWKNUM) -1);
	}

	num = (int) force_number(tmp);
	sprintf(buf, "%c", num);

	force_string(tmp);
	return make_string(buf, 2);
}


/***** Loading and Saving Images *****/
// CV_LOAD_IMAGE_COLOR     画像は，強制的に3チャンネルカラー画像として読み込まれます．
// CV_LOAD_IMAGE_GRAYSCALE 画像は，強制的にグレースケール画像として読み込まれます．
// CV_LOAD_IMAGE_UNCHANGED 画像は，そのままの画像として読み込まれます．
static int
str2iscolor(const char * str)
{
	int iscolor;

	// 3 is strlen("CV_")
	if (strncmp(str, "CV_", 3) == 0) { str += 3; }

	// 5 is strlen("LOAD_")
	if (strncmp(str, "LOAD_", 5) == 0) { str += 5; }

	// 6 is strlen("IMAGE_")
	if (strncmp(str, "IMAGE_", 6) == 0) { str += 6; }

	if (strcmp(str, "COLOR") == 0) {
		iscolor = CV_LOAD_IMAGE_COLOR;
	} else if (strcmp(str, "GRAYSCALE") == 0) {
		iscolor = CV_LOAD_IMAGE_GRAYSCALE;
	} else if (strcmp(str, "ANYCOLOR") == 0) {
		iscolor = CV_LOAD_IMAGE_ANYCOLOR;
	} else if ((strcmp(str, "") == 0) || (strcmp(str, "0") == 0)) {
		iscolor = CV_LOAD_IMAGE_ANYCOLOR;
	} else {
		_fatal(); //TODO
	}
	// TODO ANY_DEPTH

	return iscolor;
}

static NODE *
do_cvLoadImage(int nags)
{
	NODE *tmp;
	char *ret;
	IplImage *img;
	char *filename;
	int iscolor;

	tmp = (NODE*) get_scalar_argument(0, FALSE);
	force_string(tmp);
	filename = tmp->stptr;

	if ((tmp = (NODE*) get_scalar_argument(1, TRUE)) != NULL) {
		force_string(tmp);
		iscolor = str2iscolor(tmp->stptr);
	} else {
		iscolor = str2iscolor("");
	}

	img = cvLoadImage(filename, iscolor);
	if (img == NULL){
		fprintf(stderr, "no such file or directory\n");
		return make_number((AWKNUM) -1);
	}

	ret = resist_image(/*tmp->stptr,*/ img);
	return make_string(ret, strlen(ret));
}

// windows bitmaps		bmp, dib
// jpeg files			jpeg, jpg, jpe
// portable network graphics	png
// portable image format	pbm, pgm, ppm
// sun rasters			sr, ras
// tiff files			tiff, tif
// openexr hdr images		exr
// jpeg 2000 images		jp2
static NODE *
do_cvSaveImage(int nags)
{
	NODE *tmp;
	char *filename;
	IplImage *img;

	tmp = (NODE*) get_scalar_argument(0, FALSE);
	force_string(tmp);
	filename = tmp->stptr;

	tmp = (NODE*) get_scalar_argument(1, FALSE);
	force_string(tmp);
	img = lookup_image(tmp->stptr);

	return make_number((AWKNUM) cvSaveImage(filename, img, NULL));
}


/***** Video I/O *****/

static NODE *
do_cvCaptureFromFile(int nargs)
{
	NODE *tmp;
	char *ret;
	CvCapture *capture;

	tmp = (NODE*) get_scalar_argument(0, FALSE);
	force_string(tmp);

	capture = cvCaptureFromFile(tmp->stptr);

	ret = resist_capture(/*tmp->stptr,*/ capture);
	return make_string(ret, strlen(ret));
}

static NODE *
do_cvCaptureFromCAM(int nargs)
{
	NODE *tmp;
	char *ret;
	int num;
	CvCapture *capture;

	if ((tmp = (NODE*) get_scalar_argument(0, TRUE)) != NULL) {
		num = (int) force_number(tmp);
	} else {
		num = 0;
	}

	capture = cvCaptureFromCAM(num);

	ret = resist_capture(/*tmp->stptr,*/ capture);
	return make_string(ret, strlen(ret));
}

static NODE *
do_cvReleaseCapture(int nargs)
{
	NODE *tmp;
	CvCapture *capture;

	tmp = (NODE*) get_scalar_argument(0, FALSE);
	force_string(tmp);

	capture = lookup_capture(tmp->stptr);
	cvReleaseCapture(&capture);
	delete_capture(tmp->stptr);

	return make_number((AWKNUM) 0);
}

static NODE *
do_cvGrabFrame(int nargs)
{
	NODE *tmp;
	char *ret;
	IplImage *img;
	CvCapture *capture;

	tmp = (NODE*) get_scalar_argument(0, FALSE);
	force_string(tmp);

	capture = lookup_capture(tmp->stptr);

	return make_number((AWKNUM) cvRetrieve(capture));
}

static NODE *
do_cvRetrieveFrame(int nargs)
{
	NODE *tmp;
	char *ret;
	IplImage *img;
	CvCapture *capture;

	tmp = (NODE*) get_scalar_argument(0, FALSE);
	force_string(tmp);
	capture = lookup_capture(tmp->stptr);

	img = cvRetrieve(capture);
	if ((ret = lookup_image_image(img)) == NULL) {
		ret = resist_image(/*tmp->stptr,*/ img);
	}
	return make_string(ret, strlen(ret));
}

static NODE *
do_cvQueryFrame(int nargs)
{
	NODE *tmp;
	char *ret;
	IplImage *img;
	CvCapture *capture;

	tmp = (NODE*) get_scalar_argument(0, FALSE);
	force_string(tmp);
	capture = lookup_capture(tmp->stptr);

	img = cvQueryFrame(capture);
	if ((ret = lookup_image_image(img)) == NULL) {
		ret = resist_capture_image(/*tmp->stptr,*/ img);
	}
	return make_string(ret, strlen(ret));
}

static int
str2PropertyId(const char* str, int *property_id, int get )
{
	if (strcmp(str, "CV_CAP_PROP_POS_MSEC") == 0) {
		// - ファイル中の現在の位置（ミリ秒単位），あるいはビデオキャプチャのタイムスタンプ値
		*property_id = CV_CAP_PROP_POS_MSEC;
	} else if (strcmp(str, "CV_CAP_PROP_POS_FRAMES") == 0) {
		// - 次にデコード/キャプチャされるフレームのインデックス．0 から始まる
		*property_id = CV_CAP_PROP_POS_FRAMES;
	} else if (strcmp(str, "CV_CAP_PROP_POS_AVI_RATIO") == 0) {
		// - ビデオファイル内の相対的な位置（0 - ファイルの開始位置， 1 - ファイルの終了位置）
		*property_id = CV_CAP_PROP_POS_AVI_RATIO;
	} else if (strcmp(str, "CV_CAP_PROP_FRAME_WIDTH") == 0) {
		// - ビデオストリーム中のフレームの幅
		*property_id = CV_CAP_PROP_FRAME_WIDTH;
	} else if (strcmp(str, "CV_CAP_PROP_FRAME_HEIGHT") == 0) {
		// - ビデオストリーム中のフレームの高さ
		*property_id = CV_CAP_PROP_FRAME_HEIGHT;
	} else if (strcmp(str, "CV_CAP_PROP_FPS") == 0) {
		// - フレームレート
		*property_id = CV_CAP_PROP_FPS;
	} else if (strcmp(str, "CV_CAP_PROP_FOURCC") == 0) {
		// - コーデックを表す 4 文字
		*property_id = CV_CAP_PROP_FOURCC;
	} else if ((strcmp(str, "CV_CAP_PROP_FRAME_COUNT") == 0) && get) {
		// - ビデオファイル中のフレーム数 (GetPropertyのみ可能)
		*property_id = CV_CAP_PROP_FRAME_COUNT;
	} else {
		return FALSE;
	}

	return TRUE;
}

static NODE *
do_cvGetCaptureProperty(int nargs)
{
	NODE *tmp;
	CvCapture *capture;
	int property_id;

	tmp = (NODE*) get_scalar_argument(0, FALSE);
	force_string(tmp);
	if (!(capture = lookup_capture(tmp->stptr))){
		//TODO
	}

	tmp = (NODE*) get_scalar_argument(1, FALSE);
	force_string(tmp);
	if (!str2PropertyId(tmp->stptr, &property_id, TRUE)) {
		//TODO
	}

	return make_number((AWKNUM) cvGetCaptureProperty(capture, property_id));
}

static NODE *
do_cvSetCaptureProperty(int nargs)
{
	NODE *tmp;
	CvCapture *capture;
	int property_id;
	double value;

	tmp = (NODE*) get_scalar_argument(0, FALSE);
	force_string(tmp);
	if (!(capture = lookup_capture(tmp->stptr))){
		//TODO
	}

	tmp = (NODE*) get_scalar_argument(1, FALSE);
	force_string(tmp);
	if (!str2PropertyId(tmp->stptr, &property_id, FALSE)) {
		//TODO
	}

	tmp = (NODE*) get_scalar_argument(2, FALSE);
	value = (double) force_number(tmp);

	return make_number((AWKNUM) cvSetCaptureProperty(capture, property_id, value));
}

static NODE *
do_cvCreateVideoWriter(int nargs)
{
	NODE *tmp;
	char *str;
	char *ret;
	char *filename;
	int fourcc;
	double fps;
	int width, height;
	CvSize frame_size;
	int is_color;
	CvVideoWriter *vw;

	tmp = (NODE*) get_scalar_argument(0, FALSE);
	force_string(tmp);
	filename = tmp->stptr;

	tmp = (NODE*) get_scalar_argument(1, FALSE);
	force_string(tmp);
	str = tmp->stptr;
	if (strlen(str) == 4) {
		fourcc = (int) CV_FOURCC(str[1], str[2], str[3], str[4]);
	} else {
		fourcc = -1;
	}

	tmp = (NODE*) get_scalar_argument(2, FALSE);
	fps = (double) force_number(tmp);

	tmp = (NODE*) get_scalar_argument(3, FALSE);
	width = (int) force_number(tmp);

	tmp = (NODE*) get_scalar_argument(4, FALSE);
	height = (int) force_number(tmp);

	frame_size = (CvSize){width,  height};

	if ((tmp = (NODE*) get_scalar_argument(5, TRUE)) != NULL) {
		is_color = (int) force_number(tmp);
	} else {
		// default is color
		is_color = 1;
	}

	vw = cvCreateVideoWriter(filename, fourcc, fps, frame_size, is_color);
	printf("%x\n",vw); // TODO morio

	ret = resist_videowriter(/*filename,*/ vw);
	return make_string(ret, strlen(ret));
}
//フレームを圧縮するためのコーデックを表す 4 文字．
//例えば， CV_FOURCC('P','I','M','1') は，MPEG-1 コーデック，
//CV_FOURCC('M','J','P','G') は，motion-jpeg コーデックである．
//Win32 環境下では，-1 を渡すとダイアログから圧縮方法と圧縮のパラメータを選択できるようになる．

static NODE *
do_cvReleaseVideoWriter(int nargs)
{
	NODE *tmp;
	CvVideoWriter *vw;

	tmp = (NODE*) get_scalar_argument(0, FALSE);
	force_string(tmp);

	vw = lookup_videowriter(tmp->stptr);
	cvReleaseVideoWriter(&vw);
	delete_videowriter(tmp->stptr);

	return make_number((AWKNUM) 0);
}

static NODE *
do_cvWriteFrame(int nargs)
{
	NODE *tmp;
	CvVideoWriter *vw;
	IplImage *frame;

	tmp = (NODE*) get_scalar_argument(0, FALSE);
	force_string(tmp);
	vw = lookup_videowriter(tmp->stptr);

	tmp = (NODE*) get_scalar_argument(1, FALSE);
	force_string(tmp);
	frame = lookup_image(tmp->stptr);

	return make_number((AWKNUM) cvWriteFrame(vw, frame));
}



/******/
/* CV */
/******/


/***** Image Processing *****/

// 補間方法．
// CV_INTER_NN - 最近隣接補間
// CV_INTER_LINEAR - バイリニア補間 (デフォルト)
// CV_INTER_AREA - ピクセル領域の関係を用いてリサンプリングする．
//                 画像縮小の際は，モアレの無い処理結果を得ることができる手法である．
//                 拡大の際は，CV_INTER_NN と同様
// CV_INTER_CUBIC - バイキュービック補間
static int
str2interpolation(const char *str)
{
	int interpolation;

	// 3 is strlen("CV_")
	if (strncmp(str, "CV_", 3) == 0) { str += 3; }

	// 6 is strlen("INTER_")
	if (strncmp(str, "INTER_", 6) == 0) { str += 6; }

	if (strcmp(str, "NN") == 0) {
		interpolation = CV_INTER_NN;
	} else if (strcmp(str, "LINEAR") == 0) {
		interpolation = CV_INTER_LINEAR;
	} else if (strcmp(str, "AREA") == 0) {
		interpolation = CV_INTER_AREA;
	} else if (strcmp(str, "CUBIC") == 0) {
		interpolation = CV_INTER_CUBIC;
	} else if ((strcmp(str, "") == 0) || (strcmp(str, "0") == 0)) {
		// OpenCV default
		interpolation = CV_INTER_LINEAR;
	} else {
		interpolation = -1;
	}

	return interpolation;
}

static NODE *
do_cvResize(int nargs)
{
	NODE *tmp;
	IplImage *src;
	IplImage *dst;
	int interpolation;

	tmp = (NODE*) get_scalar_argument(0, FALSE);
	force_string(tmp);
	src = lookup_image(tmp->stptr);

	tmp = (NODE*) get_scalar_argument(1, FALSE);
	force_string(tmp);
	dst = lookup_image(tmp->stptr);

	if ((tmp = (NODE*) get_scalar_argument(2, TRUE)) != NULL) {
		force_string(tmp);
		interpolation = str2interpolation(tmp->stptr);
	} else {
		interpolation = str2interpolation("");
	}

	cvResize(src, dst, interpolation);
	return make_number((AWKNUM) 0);
}

#define str2code_help(src,  dst)	\
	if (strcmp(str, #src "2" #dst) == 0) { code = CV_##src##2##dst; }	\
	if (strcmp(str, #dst "2" #src) == 0) { code = CV_##dst##2##src; }
static int
str2code(const char *str)
{
	int code;

	// 3 is strlen("CV_")
	if (strncmp(str, "CV_", 3) == 0) { str += 3; }

	str2code_help(RGB, BGR)
	str2code_help(RGB, GRAY)
	str2code_help(BGR, GRAY)
	// TODO

	return code;
}

static NODE *
do_cvCvtColor(int nargs)
{
	NODE *tmp;
	IplImage *src;
	IplImage *dst;
	int code;

	tmp = (NODE*) get_scalar_argument(0, FALSE);
	force_string(tmp);
	src = lookup_image(tmp->stptr);

	tmp = (NODE*) get_scalar_argument(1, FALSE);
	force_string(tmp);
	dst = lookup_image(tmp->stptr);

	tmp = (NODE*) get_scalar_argument(2, TRUE);
	force_string(tmp);
	code = str2code(tmp->stptr);

	cvCvtColor(src, dst, code);
	return make_number((AWKNUM) 0);
}

static int
otsu_chk(const char *str)
{
	int thresholdType;

	// 3 is strlen("CV_")
	if (strncmp(str, "CV_", 3) == 0) { str += 3; }

	// 7 is strlen("THRESH_")
	if (strncmp(str, "THRESH_", 7) == 0) { str += 7; }

	if (strcmp(str, "OTSU") == 0) {
		thresholdType = CV_THRESH_OTSU;
	}

	return thresholdType;
}

// CV_THRESH_BINARY
// CV_THRESH_BINARY_INV
// CV_THRESH_TRUNC
// CV_THRESH_TOZERO
// CV_THRESH_TOZERO_INV
static int
str2thresholdType(const char *str)
{
	int thresholdType;

	// 3 is strlen("CV_")
	if (strncmp(str, "CV_", 3) == 0) { str += 3; }

	// 7 is strlen("THRESH_")
	if (strncmp(str, "THRESH_", 7) == 0) { str += 7; }

	if (strcmp(str, "BINARY") == 0) {
		thresholdType = CV_THRESH_BINARY;
	} else if (strcmp(str, "BINARY_INV") == 0) {
		thresholdType = CV_THRESH_BINARY_INV;
	} else if (strcmp(str, "TRUNC") == 0) {
		thresholdType = CV_THRESH_TRUNC;
	} else if (strcmp(str, "TOZERO") == 0) {
		thresholdType = CV_THRESH_TOZERO;
	} else if (strcmp(str, "TOZERO_INV") == 0) {
		thresholdType = CV_THRESH_TOZERO_INV;
	}

	return thresholdType;
}

static NODE *
do_cvThreshold(int nargs)
{
	NODE *tmp;
	IplImage *src;
	IplImage *dst;
	double threshold;
	double maxValue;
	int thresholdType;
	int interpolation;

	tmp = (NODE*) get_scalar_argument(0, FALSE);
	force_string(tmp);
	src = lookup_image(tmp->stptr);

	tmp = (NODE*) get_scalar_argument(1, FALSE);
	force_string(tmp);
	dst = lookup_image(tmp->stptr);

	tmp = (NODE*) get_scalar_argument(2, FALSE);
	force_string(tmp);
	if ((thresholdType = otsu_chk(tmp->stptr)) == 0) {
		threshold = (double) force_number(tmp);
	} else {
		threshold = 0;
	}

	tmp = (NODE*) get_scalar_argument(3, FALSE);
	maxValue = (int) force_number(tmp);

	tmp = (NODE*) get_scalar_argument(4, FALSE);
	force_string(tmp);
	thresholdType |= str2thresholdType(tmp->stptr);

	return make_number((AWKNUM) cvThreshold((CvArr*)src, (CvArr*)dst, threshold, maxValue,  thresholdType));
}
// 配列の要素に対して，ある定数での閾値処理を行います．
// パラメタ:
// src ? 入力配列（シングルチャンネル，8ビット，あるいは32ビット浮動小数点型）．
// dst ? 出力配列． src と同じデータ型，あるいは8ビットでなければいけません．
// threshold ? 閾値．
// maxValue ? CV_THRESH_BINARY と CV_THRESH_BINARY_INV の場合に利用される，最大値の値．
// thresholdType ? 閾値処理の種類（以下の説明を参照してください）．
//
// この関数は，シングルチャンネルの配列に対して，ある定数での閾値処理を行います．
// これは，グレースケールからの2値画像生成（関数 CmpS も，この目的に利用できます）や，
// ノイズ除去（つまり，小さすぎたり大きすぎたりする値を除きます）などによく利用されます．
// この関数がサポートする閾値処理にはいくつかの種類があり，それは引数 thresholdType によって決定されます：
//
// CV_THRESH_BINARY
// CV_THRESH_BINARY_INV
// CV_THRESH_TRUNC
// CV_THRESH_TOZERO
// CV_THRESH_TOZERO_INV
//
// また，特殊な値 CV_THRESH_OTSU を上述のものと組み合わせて使うこともできます．
// この場合，関数は大津のアルゴリズムを用いて最適な閾値を決定し，それを引数 thresh で指定された値の代わりに利用します．
// また，この関数は，計算された閾値を返します．
// 現在のところ，大津の手法は8ビット画像に対してのみ実装されています．



/**********/
/* CXCORE */
/**********/


/***** Operations on Arrays *****/


//IPL_DEPTH_8U - 符号無し 8 ビット整数
//IPL_DEPTH_8S - 符号有り 8 ビット整数
//IPL_DEPTH_16U - 符号無し 16 ビット整数
//IPL_DEPTH_16S - 符号有り 16 ビット整数
//IPL_DEPTH_32S - 符号有り 32 ビット整数
//IPL_DEPTH_32F - 単精度浮動小数点数
//IPL_DEPTH_64F - 倍精度浮動小数点数
static int
str2depth(const char * str)
{
	int depth;

	// 4 is strlen("IPL_")
	if (strncmp(str, "IPL_", 4) == 0) { str += 4; }

	// 6 is strlen("DEPTH_")
	if (strncmp(str, "DEPTH_", 6) == 0) { str += 6; }

	if (strcmp(str, "8U") == 0) {
		depth = IPL_DEPTH_8U;
	} else if (strcmp(str, "8S") == 0) {
		depth = IPL_DEPTH_8S;
	} else if (strcmp(str, "16U") == 0) {
		depth = IPL_DEPTH_16U;
	} else if (strcmp(str, "16S") == 0) {
		depth = IPL_DEPTH_16S;
	} else if (strcmp(str, "32S") == 0) {
		depth = IPL_DEPTH_32S;
	} else if (strcmp(str, "32F") == 0) {
		depth = IPL_DEPTH_32F;
	} else if (strcmp(str, "64F") == 0) {
		depth = IPL_DEPTH_64F;
	} else {
		_fatal(); //TODO
	}

	return depth;
}

static NODE *
do_cvCreateImage(int nags)
{
	NODE *tmp;
	int width, height;
	int depth;
	int channels;
	CvSize size;
	IplImage *img;
	char *ret;

	tmp = (NODE*) get_actual_argument(0, FALSE, FALSE);
	width = (int) force_number(tmp);

	tmp = (NODE*) get_actual_argument(1, FALSE, FALSE);
	height = (int) force_number(tmp);

	size = (CvSize){width, height};

	tmp = (NODE*) get_actual_argument(2, FALSE, FALSE);
	force_string(tmp);
	depth = str2depth(tmp->stptr);

	tmp = (NODE*) get_actual_argument(3, FALSE, FALSE);
	channels = (int) force_number(tmp);

	img = cvCreateImage(size, depth, channels);
	ret = resist_image(/*"",*/ img);
	return make_string(ret, strlen(ret));
}
#if 0
channels
要素（ピクセル）毎のチャンネル数．1, 2, 3, 4 のいずれか．
#endif

static NODE *
do_cvReleaseImage(int nags)
{
	NODE *tmp;
	IplImage *img;

	tmp = (NODE*) get_scalar_argument(0, FALSE);
	force_string(tmp);

	img = lookup_image(tmp->stptr);
	cvReleaseImage(&img);
	delete_image(tmp->stptr);

	return make_number((AWKNUM) 0);
}


/***** Drawing Fucntions *****/

#define DEFAULT_LINE_TYPE CV_AA

#define is_hex_char(c) \
	(('0' <= c && c <= '9' || 'a' <= c && c <= 'f' || 'A' <= c && c <= 'F') ? \
		TRUE : FALSE)
static int
str2color(const char *str, CvScalar *color)
{
	int i, j;
	int tmp[3];

	if (strlen(str) != 7)
		return FALSE;

	if (str[0] != '#')
		return FALSE;

	for (i = 1; i <= 6; i++) {
		if (!is_hex_char(str[i]))
			return FALSE;
	}

	/* color sequence is B, G, R */
	sscanf(&str[1], "%02x%02x%02x", &tmp[2], &tmp[1], &tmp[0]);
	color->val[2] = (double) tmp[2];
	color->val[1] = (double) tmp[1];
	color->val[0] = (double) tmp[0];

	return TRUE;
}

static NODE *
do_cvCircle(int nargs)
{
	NODE *tmp;
	IplImage *img;
	CvPoint center;
	int radius;
	CvScalar color;
	int thickness;
	int lineType;
	int shift;

	tmp = (NODE*) get_scalar_argument(0, FALSE);
	force_string(tmp);
	img = lookup_image(tmp->stptr);

	tmp = (NODE*) get_scalar_argument(1, FALSE);
	center.x = (int) force_number(tmp);

	tmp = (NODE*) get_scalar_argument(2, FALSE);
	center.y = (int) force_number(tmp);

	tmp = (NODE*) get_scalar_argument(3, FALSE);
	radius = (int) force_number(tmp);

	tmp = (NODE*) get_scalar_argument(4, FALSE);
	force_string(tmp);
	if (str2color(tmp->stptr, &color) == FALSE) {
		_fatal("CvCircle: Invalid color");
	}

	if ((tmp = (NODE*) get_scalar_argument(5, TRUE)) != NULL ) {
		thickness = (int) force_number(tmp);
	} else {
		thickness = 1;
	}

	if ((tmp = (NODE*) get_scalar_argument(6, TRUE)) != NULL ) {
		force_string(tmp);
		if ((strcmp(tmp->stptr, "CV_AA") == 0) || (strcmp(tmp->stptr, "AA") == 0)) {
			lineType = CV_AA;
		} else {
			lineType = (int) force_number(tmp);
		}
	} else {
		lineType = DEFAULT_LINE_TYPE;
	}

	if ((tmp = (NODE*) get_scalar_argument(7, TRUE)) != NULL) {
		shift = (int) force_number(tmp);
	} else {
		shift = 0;
	}

	cvCircle(img, center, radius, color, thickness, lineType, shift);
	return make_number((AWKNUM) 0);
}

static NODE *
do_cvLine(int nargs)
{
	NODE *tmp;
	int i;
	IplImage *img;
	CvPoint pt[2];
	CvScalar color;
	int thickness;
	int lineType;
	int shift;

	tmp = (NODE*) get_scalar_argument(0, FALSE);
	force_string(tmp);
	img = lookup_image(tmp->stptr);

	for ( i = 0; i < 2; i++ ) {
		tmp = (NODE*) get_scalar_argument(i * 2 + 1, FALSE);
		pt[i].x = (int) force_number(tmp);
		tmp = (NODE*) get_scalar_argument(i * 2 + 2, FALSE);
		pt[i].y = (int) force_number(tmp);
	}

	tmp = (NODE*) get_scalar_argument(5, FALSE);
	force_string(tmp);
	if (str2color(tmp->stptr, &color) == FALSE) {
		_fatal("Cvline: Invalid color");
	}

	if ((tmp = (NODE*) get_scalar_argument(6, TRUE)) != NULL ) {
		thickness = (int) force_number(tmp);
	} else {
		thickness = 1;
	}

	if ((tmp = (NODE*) get_scalar_argument(7, TRUE)) != NULL ) {
		force_string(tmp);
		if ((strcmp(tmp->stptr, "CV_AA") == 0) || (strcmp(tmp->stptr, "AA") == 0)) {
			lineType = CV_AA;
		} else {
			lineType = (int) force_number(tmp);
		}
	} else {
		lineType = DEFAULT_LINE_TYPE;
	}

	if ((tmp = (NODE*) get_scalar_argument(8, TRUE)) != NULL) {
		shift = (int) force_number(tmp);
	} else {
		shift = 0;
	}

	cvLine(img, pt[0], pt[1], color, thickness, lineType, shift);
	return make_number((AWKNUM) 0);
}

static NODE *
do_cvRectangle(int nargs)
{
	NODE *tmp;
	int i;
	IplImage *img;
	CvPoint pt[2];
	CvScalar color;
	int thickness;
	int lineType;
	int shift;

	tmp = (NODE*) get_scalar_argument(0, FALSE);
	force_string(tmp);
	img = lookup_image(tmp->stptr);

	for ( i = 0; i < 2; i++ ) {
		tmp = (NODE*) get_scalar_argument(i * 2 + 1, FALSE);
		pt[i].x = (int) force_number(tmp);
		tmp = (NODE*) get_scalar_argument(i * 2 + 2, FALSE);
		pt[i].y = (int) force_number(tmp);
	}

	tmp = (NODE*) get_scalar_argument(5, FALSE);
	force_string(tmp);
	if (str2color(tmp->stptr, &color) == FALSE) {
		_fatal("CvRectangle: Invalid color");
	}

	if ((tmp = (NODE*) get_scalar_argument(6, TRUE)) != NULL ) {
		thickness = (int) force_number(tmp);
	} else {
		thickness = 1;
	}

	if ((tmp = (NODE*) get_scalar_argument(7, TRUE)) != NULL ) {
		force_string(tmp);
		if ((strcmp(tmp->stptr, "CV_AA") == 0) || (strcmp(tmp->stptr, "AA") == 0)) {
			lineType = CV_AA;
		} else {
			lineType = (int) force_number(tmp);
		}
	} else {
		lineType = DEFAULT_LINE_TYPE;
	}

	if ((tmp = (NODE*) get_scalar_argument(8, TRUE)) != NULL) {
		shift = (int) force_number(tmp);
	} else {
		shift = 0;
	}

	cvRectangle(img, pt[0], pt[1], color, thickness, lineType, shift);
	return make_number((AWKNUM) 0);
}

// CV_FONT_HERSHEY_SIMPLEX 普通サイズの sans-serif フォント
// CV_FONT_HERSHEY_PLAIN 小さいサイズの sans-serif フォント
// CV_FONT_HERSHEY_DUPLEX 普通サイズの sans-serif フォント（ CV_FONT_HERSHEY_SIMPLEX よりも複雑）
// CV_FONT_HERSHEY_COMPLEX 普通サイズの serif フォント
// CV_FONT_HERSHEY_TRIPLEX 普通サイズの serif フォント（ CV_FONT_HERSHEY_COMPLEX よりも複雑）
// CV_FONT_HERSHEY_COMPLEX_SMALL CV_FONT_HERSHEY_COMPLEX の小さいサイズ版
// CV_FONT_HERSHEY_SCRIPT_SIMPLEX 手書きスタイルのフォント
// CV_FONT_HERSHEY_SCRIPT_COMPLEX CV_FONT_HERSHEY_SCRIPT_SIMPLEX の複雑版
static int
str2fontFace(const char *str)
{
	int fontFace;

	// 3 is strlen("CV_")
	if (strncmp(str, "CV_", 3) == 0) { str += 3; }

	// 5 is strlen("FONT_")
	if (strncmp(str, "FONT_", 5) == 0) { str += 5; }

	// 8 is strlen("HERSHEY_")
	if (strncmp(str, "HERSHEY_", 8) == 0) { str += 8; }

	if (strcmp(str, "SIMPLEX") ) {
		fontFace = CV_FONT_HERSHEY_SIMPLEX;
	} else if (strcmp(str, "PLAIN") ) {
		fontFace = CV_FONT_HERSHEY_PLAIN;
	} else if (strcmp(str, "DUPLEX") ) {
		fontFace = CV_FONT_HERSHEY_DUPLEX;
	} else if (strcmp(str, "COMPLEX") ) {
		fontFace = CV_FONT_HERSHEY_COMPLEX;
	} else if (strcmp(str, "TRIPLEX") ) {
		fontFace = CV_FONT_HERSHEY_TRIPLEX;
	} else if (strcmp(str, "COMPLEX_SMALL") ) {
		fontFace = CV_FONT_HERSHEY_COMPLEX_SMALL;
	} else if (strcmp(str, "SCRIPT_SIMPLEX") ) {
		fontFace = CV_FONT_HERSHEY_SCRIPT_SIMPLEX;
	} else if (strcmp(str, "SCRIPT_COMPLEX") ) {
		fontFace = CV_FONT_HERSHEY_SCRIPT_COMPLEX;
	}

	return fontFace;
}

static NODE *
do_cvInitFont(int nargs)
{
	NODE *tmp;
	char *ret;
	CvFont *font;
	int fontFace;
	double hscale;
	double vscale;
	double shear;
	int thickness;
	int lineType;

	tmp = (NODE*) get_scalar_argument(0, FALSE);
	force_string(tmp);
	fontFace = str2fontFace(tmp->stptr);

	if ((tmp = (NODE*) get_scalar_argument(1, TRUE)) != NULL) {
		hscale = (double) force_number(tmp);
	} else {
		hscale = 1.0;
	}

	if ((tmp = (NODE*) get_scalar_argument(2, TRUE)) != NULL) {
		vscale = (double) force_number(tmp);
	} else {
		vscale = hscale;
	}

	if ((tmp = (NODE*) get_scalar_argument(3, TRUE)) != NULL ) {
		shear = (double) force_number(tmp);
	} else {
		shear = 0;
	}

	if ((tmp = (NODE*) get_scalar_argument(4, TRUE)) != NULL ) {
		thickness = (int) force_number(tmp);
	} else {
		thickness = 1;
	}

	if ((tmp = (NODE*) get_scalar_argument(5, TRUE)) != NULL ) {
		force_string(tmp);
		if ((strcmp(tmp->stptr, "CV_AA") == 0) || (strcmp(tmp->stptr, "AA") == 0)) {
			lineType = CV_AA;
		} else {
			lineType = (int) force_number(tmp);
		}
	} else {
		lineType = DEFAULT_LINE_TYPE;
	}

	cvInitFont(font, fontFace, hscale, vscale, shear, thickness, lineType);
	ret = resist_font(/*filename,*/ font);
	return make_string(ret, strlen(ret));
}

static NODE *
do_cvPutText(int nargs)
{
	NODE *tmp;
	IplImage *img;
	char *text;
	int x, y;
	CvPoint org;
	CvFont *font;
	CvScalar color;

	tmp = (NODE*) get_scalar_argument(0, FALSE);
	force_string(tmp);
	img = lookup_image(tmp->stptr);

	tmp = (NODE*) get_scalar_argument(1, FALSE);
	force_string(tmp);
	text = lookup_image(tmp->stptr);

	tmp = (NODE*) get_scalar_argument(2, FALSE);
	org.x = (int) force_number(tmp);

	tmp = (NODE*) get_scalar_argument(3, FALSE);
	org.y = (int) force_number(tmp);

	tmp = (NODE*) get_scalar_argument(4, FALSE);
	force_string(tmp);
	font = lookup_font(tmp->stptr);

	tmp = (NODE*) get_scalar_argument(5, FALSE);
	force_string(tmp);
	if (str2color(tmp->stptr, &color) == FALSE) {
		_fatal("CvPutText: Invalid color");
	}

	cvPutText(img, text, org, font, color);
	return make_number((AWKNUM) 0);
}

static NODE *
do_CV_RGB(int nags)
{
	NODE *tmp;
	int i;
	int color;
	char colors[8];
	char *ptr;

	colors[0] = '#';

	ptr = &colors[1];
	for (i = 0; i < 3; i++) {
		tmp = (NODE*) get_scalar_argument(i, FALSE);
		color = (int) force_number(tmp);
		color = color < 0   ?   0 : color;
		color = color > 255 ? 255 : color;
		sprintf(ptr, "%02x", color);
		ptr += 2;
	}

	return make_string(colors, 7);
}



/*********/
/* AwkCV */
/*********/


/***** IplImage *****/

#define acvGetImgHead_help_num(index)	\
	elem = assoc_lookup(array, make_string(#index, strlen(#index)), 0);	\
	*elem = make_number((AWKNUM) (img->index));
static NODE *
do_acvGetImgHead(int nargs)
{
	NODE *tmp;
	NODE **elem;
	char *str;
	IplImage *img;
	NODE *array;

	tmp = (NODE*) get_scalar_argument(0, FALSE);
	force_string(tmp);
	img = lookup_image(tmp->stptr);

	array = get_array_argument(1, FALSE);

	acvGetImgHead_help_num(nChannels)

	elem = assoc_lookup(array, make_string("depth", strlen("depth")), 0);
	switch (img->depth) {
	case IPL_DEPTH_8U:
		str = "IPL_DEPTH_8U";
	case IPL_DEPTH_8S:
		str = "IPL_DEPTH_8S";
	case IPL_DEPTH_16U:
		str = "IPL_DEPTH_16U";
	case IPL_DEPTH_16S:
		str = "IPL_DEPTH_16S";
	case IPL_DEPTH_32S:
		str = "IPL_DEPTH_32S";
	case IPL_DEPTH_32F:
		str = "IPL_DEPTH_32F";
	case IPL_DEPTH_64F:
		str = "IPL_DEPTH_64F";
	}
	*elem = make_string(str, strlen(str));

	acvGetImgHead_help_num(dataOrder)

	acvGetImgHead_help_num(origin)

	acvGetImgHead_help_num(width)

	acvGetImgHead_help_num(height)

	//acvGetImgHead_help_num(roi)

	acvGetImgHead_help_num(imageSize)

	acvGetImgHead_help_num(widthStep)

	return make_number((AWKNUM) 0);
}

static NODE *
do_acvGetImgWidth(int nags)
{
	NODE *tmp;
	IplImage *img;

	tmp = (NODE*) get_scalar_argument(0, FALSE);
	force_string(tmp);
	img = lookup_image(tmp->stptr);

	return make_number((AWKNUM) img->width);
}

static NODE *
do_acvGetImgHeight(int nags)
{
	NODE *tmp;
	IplImage *img;

	tmp = (NODE*) get_scalar_argument(0, FALSE);
	force_string(tmp);
	img = lookup_image(tmp->stptr);

	return make_number((AWKNUM) img->height);
}

static NODE *
do_acvGetImgDataPointer(int nags)
{
	NODE *tmp;
	IplImage *img;

	tmp = (NODE *) get_scalar_argument(0, FALSE);
	force_string(tmp);
	img = lookup_image(tmp->stptr);

	return make_number((AWKNUM) img->imageData);
}


/***** Detect Object *****/

static NODE *
do_acvDetectObjects(int nargs)
{
	NODE *tmp, *array;
	char *full_index;
	size_t full_len;
	int i;
	IplImage *src_img;
	IplImage *src_gray;
	const char *cascade_name;
	CvHaarClassifierCascade *cascade;
	// CvMemStorage *storage; // TODO reentrant
	CvSeq *objs;
	double scale_factor;
	int min_neighbors;
	int flags;
	int size_min_x, size_min_y, size_max_x, size_max_y;

	// (1)画像を得る
	tmp = (NODE *) get_scalar_argument(0, FALSE);
	force_string(tmp);
	src_img = lookup_image(tmp->stptr);
	if( src_img == NULL ){
		// TODO
		fprintf(stderr, "no such file or directory\n");
		exit(-1);
	}
	src_gray = cvCreateImage(cvGetSize(src_img), IPL_DEPTH_8U, 1);

	// (2)ブーストされた分類器のカスケードを読み込む
	tmp = (NODE *) get_scalar_argument(1, FALSE);
	force_string(tmp);
	cascade = (CvHaarClassifierCascade *) cvLoad(tmp->stptr, 0, 0, 0);

	// (3)読み込んだ画像のグレースケール化，ヒストグラムの均一化を行う
	// storage = cvCreateMemStorage(0); // TODO reentrant
	// cvClearMemStorage(detect_obj_storage);
	cvCvtColor(src_img, src_gray, CV_BGR2GRAY);
	cvEqualizeHist(src_gray, src_gray);

	// (4)物体（顔）検出
	tmp           = (NODE *) get_scalar_argument(2, FALSE);
	scale_factor  = (int) force_number(tmp);
	tmp           = (NODE *) get_scalar_argument(3, FALSE);
	min_neighbors = (int) force_number(tmp);
	tmp           = (NODE *) get_scalar_argument(4, FALSE);
	flags         = (int) force_number(tmp);
	tmp           = (NODE *) get_scalar_argument(5, FALSE);
	size_min_x    = (int) force_number(tmp);
	tmp           = (NODE *) get_scalar_argument(6, FALSE);
	size_min_y    = (int) force_number(tmp);
	tmp           = (NODE *) get_scalar_argument(7, FALSE);
	size_max_x    = (int) force_number(tmp);
	tmp           = (NODE *) get_scalar_argument(8, FALSE);
	size_max_y    = (int) force_number(tmp);

	objs = cvHaarDetectObjects(src_gray, cascade, detect_obj_storage,
			scale_factor, min_neighbors, flags;,
			cvSize(size_min_x, size_min_y), cvSize(size_max_x, size_max_y));

	// (5)検出された全ての位置をAWKの配列にコピーする
	array = (NODE *) get_array_argument(9, FALSE);

	full_len = 10 /* strlen(2^32) */
		  + SUBSEP_node->var_value->stlen
		  + 6  /* strlen("height") longer than x, y, width */
		  + 1; /* string terminator */
	emalloc(full_index, char *, full_len, "do_acvDetectObject");

	for (i = 0; i < (objs ? objs->total : 0); i++) {
		CvRect *r = (CvRect *) cvGetSeqElem(objs, i);
		NODE **elemval;

		sprintf(full_index, "%d%.*sx", i,
			(int) SUBSEP_node->var_value->stlen,
			SUBSEP_node->var_value->stptr);
		elemval  = assoc_lookup(array, make_string(full_index, strlen(full_index)), 0);
		*elemval = make_number((AWKNUM) r->x);

		sprintf(full_index, "%d%.*sy", i,
			(int) SUBSEP_node->var_value->stlen,
			SUBSEP_node->var_value->stptr);
		elemval  = assoc_lookup(array, make_string(full_index, strlen(full_index)), 0);
		*elemval = make_number((AWKNUM) r->y);

		sprintf(full_index, "%d%.*swidth", i,
			(int) SUBSEP_node->var_value->stlen,
			SUBSEP_node->var_value->stptr);
		elemval  = assoc_lookup(array, make_string(full_index, strlen(full_index)), 0);
		*elemval = make_number((AWKNUM) r->width);

		sprintf(full_index, "%d%.*sheight", i,
			(int) SUBSEP_node->var_value->stlen,
			SUBSEP_node->var_value->stptr);
		elemval  = assoc_lookup(array, make_string(full_index, strlen(full_index)), 0);
		*elemval = make_number((AWKNUM) r->height);
	}

	efree(full_index);

	cvReleaseImage(&src_gray);
	cvClearMemStorage(detect_obj_storage);
	// cvReleaseMemStorage (&storage);	// reentrant

	return make_number((AWKNUM) obj->total);
}



/***********/
/* Utility */
/***********/


static NODE *
do_round(int nargs)
{
	NODE *tmp;

	tmp = (NODE*) get_scalar_argument(0, FALSE);
	return make_number((AWKNUM) cvRoud((double) force_number(tmp)) 0);
}



/**********/
/* dlload */
/**********/


NODE *
dlload(NODE *tree, void *dl)
{
	//////// High GUI
	// User Interface
	make_builtin("cvNamedWindow", do_cvNamedWindow, 2);
	make_builtin("cvResizeWindow", do_cvResizeWindow, 3);
	make_builtin("cvMoveWindow", do_cvMoveWindow, 3);
	make_builtin("cvDestroyWindow", do_cvDestroyWindow, 1);
	make_builtin("cvShowImage", do_cvShowImage, 2);
	make_builtin("cvSetMouseCallback", do_cvSetMouseCallback, 3);
	make_builtin("cvWaitKey", do_cvWaitKey, 1);
	make_builtin("ascii", do_ascii, 1);
	make_builtin("char", do_char, 1);
	// Loading and Saving Images
	make_builtin("cvLoadImage", do_cvLoadImage, 2);
	make_builtin("cvSaveImage", do_cvSaveImage, 3);
	// Video I/O
	make_builtin("cvCaptureFromFile", do_cvCaptureFromFile, 1);
	make_builtin("cvCaptureFromCAM", do_cvCaptureFromCAM, 1);
	make_builtin("cvReleaseCapture", do_cvReleaseCapture, 1);
	make_builtin("cvGrabFrame", do_cvGrabFrame, 1);
	make_builtin("cvRetrieveFrame", do_cvRetrieveFrame, 1);
	make_builtin("cvQueryFrame", do_cvQueryFrame, 1);
	make_builtin("cvGetCaptureProperty", do_cvGetCaptureProperty, 2);
	make_builtin("cvSetCaptureProperty", do_cvSetCaptureProperty, 3);
	make_builtin("cvCreateVideoWriter", do_cvCreateVideoWriter, 6);
	make_builtin("cvReleaseVideoWriter", do_cvReleaseVideoWriter, 1);
	make_builtin("cvWriteFrame", do_cvWriteFrame, 2);
	//////// CV
	// Image Processing
	make_builtin("cvResize", do_cvResize, 3);
	make_builtin("cvCvtColor", do_cvCvtColor, 3);
	make_builtin("cvThreshold", do_cvThreshold, 5);
	//////// CXCORE
	// Operations on Arrays
	make_builtin("cvCreateImage", do_cvCreateImage, 4); //?
	make_builtin("cvReleaseImage", do_cvReleaseImage, 1);
	// Drawing Functions
	make_builtin("cvCircle", do_cvCircle, 8);
	make_builtin("cvLine", do_cvLine, 9);
	make_builtin("cvRectangle", do_cvRectangle, 9);
	/*make_builtin("cvGetTextSize", do_cvGetTextSize, 4);*/
	make_builtin("cvInitFont", do_cvInitFont, 6);
	make_builtin("cvPutText", do_cvPutText, 6);
	make_builtin("CV_RGB", do_CV_RGB, 3);
	//////// AwkCV
	// IplImage
	make_builtin("acvGetImgHead", do_acvGetImgHead, 2);
	make_builtin("acvGetImgWidth", do_acvGetImgWidth, 1);
	make_builtin("acvGetImgHeight", do_acvGetImgHeight, 1);
	make_builtin("acvGetImgDataPointer", do_acvGetImgDataPointer, 1);
	//make_builtin("acvDeleteFont", do_acvDeleteFont, 1);
	// Detect Object
	make_builtin("acvDetectObjects", do_acvDetectObjects, 10);
	//////// Utility
	make_builtin("round", do_round, 1);


	/* TODO be reentrant */
	detect_obj_storage = cvCreateMemStorage(0);


	return make_number((AWKNUM) 0);
}

// endl
// TODO
//
// 物体認識
// 動体検知
// 顔認識
// キーコード
//
