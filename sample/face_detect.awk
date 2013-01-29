
BEGIN {
    extension("./OpenCV-AWK.so", "dlload")

    cascade_name = "/usr/local/share/OpenCV/haarcascades/haarcascade_frontalface_default.xml"
    cascade_name = "haarcascade_frontalface_default.xml"

    for ( ; ; ) {
	for (i = 1; i < ARGC; i++) {
	    print ARGV[i]
	    img = cvLoadImage(ARGV[i], CV_LOAD_IMAGE_ANYCOLOR);

	    num = acvDetectObjects(img, cascade_name, 1.11, 3,  "DO_CANNY_PRUNING", 0, 0, 00, 00, faces)

	    for (j = 0; j < num; j++) {
		center_x = round(faces[j, "x"] + faces[j, "width"] * 0.5);
		center_y = round(faces[j, "y"] + faces[j, "height"] * 0.5);
		radius = round((faces[j, "width"] + faces[j, "height"]) * 0.25);
		cvCircle(img, center_x, center_y, radius, "#ff0000", 3, 8, 0);
	    }

	    cvNamedWindow("Image");
	    cvShowImage("Image", img);
	    c = cvWaitKey(1300);
	    switch (c) {
	    case 0x0a:
	    case 0x0d:
		cvSaveImage("face_detect_by_awk.jpeg", img, 0);
		cvNamedWindow("")
		cvShowImage("", img);
		break
	    case 0x71:
		exit
	    }
	    cvReleaseImage(img);
	}
    }
}
