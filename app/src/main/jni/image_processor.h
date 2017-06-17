#pragma once

#include <vector>

#include <opencv2/core.hpp>
#include <opencv2/imgproc.hpp>
#include <opencv2/core/ocl.hpp>

using namespace std;



enum DisplayMode
{
	DISP_MODE_RAW = 0,
	DISP_MODE_THRESH = 1,
	DISP_MODE_CONTOURS = 2,
	DISP_MODE_CONVEX_CONTOURS = 3,
	DISP_MODE_POLY = 4,
	DISP_MODE_TARGETS = 5,
	DISP_MODE_TARGETS_PLUS = 6
};

struct TargetInfo
{
	double centroid_x;
	double centroid_y;
	double width;
	double height;
	vector<cv::Point> points;
};


struct ContourInfo
{
	vector<cv::Point> contour;
	double area;
};

struct byArea
{
	bool operator() (ContourInfo const &a, ContourInfo const &b)
	{
		return a.area > b.area;
	}
};



#ifdef ANDROID
#include <jni.h>

#ifdef __cplusplus
extern "C" {
#endif

	void processFrame(JNIEnv* env,
					int tex1,
					int tex2,
					int w,
					int h,
					int mode,
					int h_min,
					int h_max,
					int s_min,
					int s_max,
					int v_min,
					int v_max,
					jobject destTargetInfo);

#ifdef __cplusplus
}
#endif
#endif


#ifdef _MSC_VER
#include <opencv2/core/core.hpp>

void processFrame(cv::Mat input, int mode,
					int h_min, int h_max, 
					int s_min, int s_max, 
					int v_min, int v_max);

#endif

