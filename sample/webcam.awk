#!gawk-4.0.1 -f

BEGIN {
    extension("./OpenCV-AWK.so", "dlload")

    switch (ARGV[1]) {
    case "qvga":
	w = 320
	h = 240
	break
    case "vga":
	w = 640
	h = 480
	break
    case "sxga":
	w = 1280
	h = 1024
	break
    case "svga":
    default:
	w = 1024
	h = 768
	break
    }

    #capture = cvCreateCameraCapture()
    capture = cvCaptureFromCAM()

    #// この設定は，利用するカメラに依存する
    #// キャプチャサイズを設定する．
    cvSetCaptureProperty(capture, "CV_CAP_PROP_FRAME_WIDTH", w)
    cvSetCaptureProperty(capture, "CV_CAP_PROP_FRAME_HEIGHT", h)

    vw = cvCreateVideoWriter("vvvvv.mov", "PIM1", 30, w, h, 1)

    cvNamedWindow("Capture")
    cvMoveWindow("Cature", 100, 100)

    cvSetMouseCallback("Capture", "onmouse")

    mon = cvCreateImage(w, h, "IPL_DEPTH_8U", 1)
    gray = cvCreateImage(w, h, "IPL_DEPTH_8U", 1)
    bin = cvCreateImage(w, h, "IPL_DEPTH_8U", 1)

    #// カメラから画像をキャプチャする
    while (1) {
	frame = cvQueryFrame(capture)
	cvCvtColor(frame, gray, "RGB2GRAY")
#    print gray
#    acvGetImgHead(gray, img_hd)
#    print img_hd["depth"]
#    print img_hd["nChannels"]
	switch (conv) {
	case "b":
	    cvThreshold(gray, bin, "OTSU", 255, "BINARY")
	    show = bin
	    break
	case "t":
	    cvThreshold(gray, bin, "OTSU", 255, "TRUNC")
	    show = bin
	    break
	case "z":
	    cvThreshold(gray, bin, "OTSU", 255, "TOZERO")
	    show = bin
	    break
	case "g":
	    show = gray
	    break
	default:
	    show = frame
	    break
	}
	#print show
	if (circle)
	    cvCircle(show, cx, cy, 80, "#ff0000", 3, "AA", 0)
	cvShowImage("Capture", show)
	cvWriteFrame(vw, image)
	c = cvWaitKey(2)
	if (c == 0x1b) {
	    break
	} else if (c == ascii("q")) {
	    break
	} else if ( c == ascii("g") || c == ascii("b") || c == ascii("t") || c == ascii("z") || c == ascii("c")) {
	    conv = substr(char(c), 1, 1)
	} else if (c == ascii("\n")) {
	    cvSaveImage("_cam_on_awk_.jpeg", frame, 0);
	} else if (c == ascii("\t")) {
	    cvSaveImage("_cam_on_awk_.jpeg", frame, 0);
	} else if (c == ascii(" ")) {
	    cvSaveImage("_cam_on_awk_"rand()".jpeg", show, 0);
	    cvNamedWindow("")
	    cvShowImage("", show);
	}
    }

    cvReleaseCapture(capture)
    cvReleaseVideoWriter(vw)
    cvDestroyWindow("Capture")
    cvReleaseImage(bin)
}


function onmouse(event, x, y, flaggs, paam)
{
    if (event == "LBUTTONDOWN") {
	cx = x; cy = y
	circle = 1
    } else if (event == "RBUTTONDOWN") {
	circle = 0
    }
    return
}


