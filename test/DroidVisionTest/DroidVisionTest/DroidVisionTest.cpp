// Couldn't get JNI debugging working with Android Studio, so I'm using this file to debug image_processing.cpp

#include <opencv2/core/core.hpp>
#include <opencv2/highgui/highgui.hpp>
#include <opencv2/imgproc/imgproc.hpp>
#include <iostream>

#include "image_processor.h"


using namespace std;

int main()
{
	cv::VideoCapture captureStream(1);   // 0 is the id of video device.  0 if you have only one camera.

	if (!captureStream.isOpened())		//check if video device has been initialised
	{ 
		cout << "cannot open camera";
		return 0;
	}

	int mode = 3;	// Targets Plus
	int w = 640;
	int h = 480;
	int h_min = 0;
	int h_max = 255;
	int s_min = 0;
	int s_max = 255;
	int v_min = 130;
	int v_max = 255;

	while (true)
	{
		cv::Mat cameraFrame;
		captureStream.read(cameraFrame);

		processFrame(cameraFrame, mode, h_min, h_max, s_min, s_max, v_min, v_max);

		bool stop = false;
		int key = cv::waitKey(30);
		printf("Key = %d\n", key);
		switch (key)
		{ 
		case 56:	// NumPad up
			if (mode < 6)
				mode++;
			break;
		case 50:	// NumPad down
			if (mode > 0)
				mode--;
			break;
		case 27:
			stop = true;
			break;
		}

		if (stop)
			break;
	}

	// the camera will be closed automaticaly upon exit
	// captureStream.close();

	return 0;
}
