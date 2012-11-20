#!gawk-4.0.1 -f
BEGIN {
    extension("./OpenCV-AWK.so", "dlload")

    for (i = 1; i < ARGC; i++) {
	img = cvLoadImage(ARGV[i], CV_LOAD_IMAGE_ANYCOLOR);
	cvNamedWindow("Image");
	cvShowImage("Image", img);
	print c = cvWaitKey(000);
	switch (c) {
	case 0x71:
	    exit
	}
	cvReleaseImage(img);
    }
}
