#include <vector>
#include <algorithm>
#include <limits>

#ifdef ANDROID
	#include <GLES2/gl2.h>
	#include <EGL/egl.h>
#endif

#ifdef _MSC_VER
#include <Windows.h>
#undef min	// stop windows.h from messing up numeric_limits<T>::min()
#undef max	// stop windows.h from messing up numeric_limits<T>::max()
#include <GL.h>
#include <opencv2/core/core.hpp>
#include <opencv2/highgui/highgui.hpp>
#include <opencv2/imgproc/imgproc.hpp>
using std::min;
using std::max;
#endif

#include "image_processor.h"
#include "common.hpp"


using namespace std;




void processImpl(cv::Mat input,  cv::Mat &thresh,
				 vector<vector<cv::Point>> &contours, vector<vector<cv::Point>> &convex_contours, vector<vector<cv::Point>> &polys,
				 vector<TargetInfo> &targets, vector<TargetInfo> &rejected_targets,
				 int h_min, int h_max, int s_min, int s_max, int v_min, int v_max)
{
	contours.clear();
	convex_contours.clear();
	polys.clear();
	targets.clear();
	rejected_targets.clear();

	//-----------------------------------
	// Convert to HSV for filtering
	//-----------------------------------
	int64_t t = getTimeMs();
	static cv::Mat hsv;
	cv::cvtColor(input, hsv, CV_RGBA2RGB);
	cv::cvtColor(hsv, hsv, CV_RGB2HSV);
	LOGD("cvtColor() costs %d ms", getTimeInterval(t));

	//-----------------------------------
	// Keep pixels inside HSV thresholds
	//-----------------------------------
	t = getTimeMs();
	cv::inRange(hsv, cv::Scalar(h_min, s_min, v_min), cv::Scalar(h_max, s_max, v_max), thresh);
	LOGD("inRange() costs %d ms", getTimeInterval(t));


	//----------------------------------------
	// Find contours around thresholded image
	//----------------------------------------
	t = getTimeMs();
	static cv::Mat contour_input;
	contour_input = thresh.clone();
	cv::findContours(contour_input, contours, cv::RETR_EXTERNAL, cv::CHAIN_APPROX_TC89_KCOS);

	// need at least 2 halves
	if (contours.size() >= 2)
	{
		//----------------------------------------
		// Sort contours by area
		//----------------------------------------
		vector<ContourInfo> sortedContour;
		sortedContour.resize(contours.size());
		for (int k = 0; k < contours.size(); k++)
		{
			sortedContour[k].contour = contours[k];
			sortedContour[k].area = cv::contourArea(contours[k]);
			LOGD("Area %d: %f", k, sortedContour[k].area);
		}
		sort(sortedContour.begin(), sortedContour.end(), byArea());
		for (int k = 0; k < sortedContour.size(); k++)
		{
			LOGD("Sorted Area %d: %f", k, sortedContour[k].area);
		}

		bool rectangular[2];

		convex_contours.resize(2);
		polys.resize(2);

		for (int k = 0; k < 2; k++)
		{
			//----------------------------------------
			// Find a convex hull around the contour
			//----------------------------------------
			cv::convexHull(sortedContour[k].contour, convex_contours[k], false);

			//------------------------------------------------
			// Approximate convex hull lines with polynomials
			//------------------------------------------------
			cv::approxPolyDP(convex_contours[k], polys[k], 20, true);

			//------------------------------------------------
			// Determine if contours are rectangular
			//------------------------------------------------
			rectangular[k] = (polys[k].size() == 4 && cv::isContourConvex(polys[k]));
		}
		
		
		if (!rectangular[0] || !rectangular[1])
		{
			//            LOGD("Rejecting target due to rectangular-ness: # sides = %d, %d", poly[0].size(), poly[1].size());
			LOGD("Rejecting target due to rectangular-ness: # sides = %d, %d", polys[0].size(), polys[1].size());
			TargetInfo target;
			target.points = polys[0];
			rejected_targets.push_back(move(target));
			target.points = polys[1];
			rejected_targets.push_back(move(target));
		}
		else
		{
			//----------------------------------------------------------------
			// The true peg target should consist of the two largest contours
			//----------------------------------------------------------------
			vector<cv::Point> goal;
			goal.insert(goal.end(), polys[0].begin(), polys[0].end());
			goal.insert(goal.end(), polys[1].begin(), polys[1].end());

			vector<cv::Point> convex_goal;
			vector<cv::Point> poly_goal;

			//----------------------------------------
			// Find a convex hull around the goal
			//----------------------------------------
			cv::convexHull(goal, convex_goal, false);

			//------------------------------------------------
			// Approximate convex hull lines with polynomials
			//------------------------------------------------
			cv::approxPolyDP(convex_goal, poly_goal, 20, true);

			//------------------------------------------------
			// Calculate centroid, height, width of contour
			//------------------------------------------------
			TargetInfo target;
			int min_x = numeric_limits<int>::max();
			int max_x = numeric_limits<int>::min();
			int min_y = numeric_limits<int>::max();
			int max_y = numeric_limits<int>::min();
			target.centroid_x = 0;
			target.centroid_y = 0;
			for (auto point : poly_goal)
			{
				if (point.x < min_x)
					min_x = point.x;
				if (point.x > max_x)
					max_x = point.x;
				if (point.y < min_y)
					min_y = point.y;
				if (point.y > max_y)
					max_y = point.y;

				target.centroid_x += point.x;
				target.centroid_y += point.y;
			}
			target.centroid_x /= 4;
			target.centroid_y /= 4;
			target.width = max_x - min_x;
			target.height = max_y - min_y;
			target.points = poly_goal;

			//------------------------------------------------
			// Filter based on min/max size
			//------------------------------------------------
			// Keep in mind width/height are in imager terms...
			const double kMinTargetWidth = 20;
			const double kMaxTargetWidth = 600;
			const double kMinTargetHeight = 10;
			const double kMaxTargetHeight = 400;
			if (target.width < kMinTargetWidth || target.height < kMinTargetHeight ||
				target.width > kMaxTargetWidth || target.height > kMaxTargetHeight)
			{
				LOGD("Rejecting target due to size: W:%.1lf, H:%.1lf -- limits: W:%.1lf-%.1lf, H:%.1lf-%.1lf",
					target.width, target.height, kMinTargetWidth, kMaxTargetWidth, kMinTargetHeight, kMaxTargetHeight);
				rejected_targets.push_back(move(target));
			}
			else
			{
				//------------------------------------------------
				// Filter based on shape (angles of sides)
				//------------------------------------------------
				const double kNearlyHorizontalSlope = 1 / 1.25;
				const double kNearlyVerticalSlope = 1.25;
				int num_nearly_horizontal_slope = 0;
				int num_nearly_vertical_slope = 0;
				bool last_edge_vertical = false;
				for (size_t i = 0; i < 4; ++i)
				{
					double dy = target.points[i].y - target.points[(i + 1) % 4].y;
					double dx = target.points[i].x - target.points[(i + 1) % 4].x;
					double slope = numeric_limits<double>::max();
					if (dx != 0)
						slope = dy / dx;

					if (abs(slope) <= kNearlyHorizontalSlope && (i == 0 || last_edge_vertical))
					{
						last_edge_vertical = false;
						num_nearly_horizontal_slope++;
					}
					else if (abs(slope) >= kNearlyVerticalSlope && (i == 0 || !last_edge_vertical))
					{
						last_edge_vertical = true;
						num_nearly_vertical_slope++;
					}
					else
					{
						return;
					}
				}
				if (num_nearly_horizontal_slope != 2 && num_nearly_vertical_slope != 2)
				{
					LOGD("Rejecting target due to shape");
					rejected_targets.push_back(move(target));
				}
				else
				{
					//------------------------------------------------
					// Filter based on fullness
					// (percentage of area filled by reflection)
					//------------------------------------------------
					const double kMinFullness = .39 - .20;
					const double kMaxFullness = .39 + .20;
					//                    double filled_area = cv::contourArea(poly[0]) + cv::contourArea(poly[1]);
					double filled_area = cv::contourArea(polys[0]) + cv::contourArea(polys[1]);
					double total_area = cv::contourArea(poly_goal);
					double fullness = filled_area / total_area;
					if (fullness < kMinFullness || fullness > kMaxFullness)
					{
						LOGD("Rejected target due to fullness: Filled: %.1lf, Total: %.1lf, Fullness: %.3lf", filled_area, total_area, fullness);
						rejected_targets.push_back(move(target));
					}
					else
					{
						//------------------------------------------------
						// We found a target
						//------------------------------------------------
						LOGD("Found target at (%.2lf, %.2lf).  W:%.2lf, H:%.2lf", target.centroid_x, target.centroid_y, target.width, target.height);
						targets.push_back(move(target));
					}
				}
			}
		}
	}
	LOGD("Contour analysis costs %d ms", getTimeInterval(t));
}


void displayImpl(DisplayMode mode, cv::Mat &input, cv::Mat &thresh, 
	vector<vector<cv::Point>> &contours, vector<vector<cv::Point>> &convex_contours, vector<vector<cv::Point>> &polys, 
	vector<TargetInfo> &targets, vector<TargetInfo> &rejected_targets, cv::Mat &display)
{
	int64_t t = getTimeMs();

	if (mode == DISP_MODE_RAW)
	{
		display = input;
	}
	else if (mode == DISP_MODE_THRESH)
	{
		cv::cvtColor(thresh, display, CV_GRAY2RGBA);
	}
	else if (mode == DISP_MODE_CONTOURS)
	{
		cv::cvtColor(thresh, display, CV_GRAY2RGBA);
		for (auto &points : contours)
		{
			cv::polylines(display, points, true, cv::Scalar(0, 112, 255), 3);
		}
	}
	else if (mode == DISP_MODE_CONVEX_CONTOURS)
	{
		cv::cvtColor(thresh, display, CV_GRAY2RGBA);
		for (auto &points : convex_contours)
		{
			cv::polylines(display, points, true, cv::Scalar(0, 112, 255), 3);
		}
	}
	else if (mode == DISP_MODE_POLY)
	{
		cv::cvtColor(thresh, display, CV_GRAY2RGBA);
		for (auto &points : polys)
		{
			cv::polylines(display, points, true, CV_RGB(0, 112, 255), 3);
		}
	}
	else
	{
		display = input;
		// Render the targets
		for (auto &target : targets)
		{
			// render accepted targets in blue/green
			cv::polylines(display, target.points, true, CV_RGB(0, 112, 255), 3);
			cv::circle(display, cv::Point(target.centroid_x, target.centroid_y), 5,
				cv::Scalar(0, 112, 255), 3);
		}
	}
	if (mode == DISP_MODE_TARGETS_PLUS)
	{
		for (auto &target : rejected_targets)
		{
			// render rejected targets in red
			cv::polylines(display, target.points, true, CV_RGB(255, 0, 0), 3);
		}
	}
	LOGD("Creating display costs %d ms", getTimeInterval(t));
}



#ifdef ANDROID

static bool sFieldsRegistered = false;

static jfieldID sNumTargetsField;
static jfieldID sTargetsField;

static jfieldID sCentroidXField;
static jfieldID sCentroidYField;
static jfieldID sWidthField;
static jfieldID sHeightField;

static void ensureJniRegistered(JNIEnv *env)
{
    if (sFieldsRegistered)
    {
        return;
    }

    // get class/fields once
    sFieldsRegistered = true;
    jclass targetsInfoClass = env->FindClass("org/team686/droidvision2017/NativePart$TargetsInfo");
    sNumTargetsField = env->GetFieldID(targetsInfoClass, "numTargets", "I");
    sTargetsField = env->GetFieldID(
        targetsInfoClass, "targets",  "[Lorg/team686/droidvision2017/NativePart$TargetsInfo$Target;");
    jclass targetClass = env->FindClass("org/team686/droidvision2017/NativePart$TargetsInfo$Target");

    sCentroidXField = env->GetFieldID(targetClass, "centroidX", "D");
    sCentroidYField = env->GetFieldID(targetClass, "centroidY", "D");
    sWidthField = env->GetFieldID(targetClass, "width", "D");
    sHeightField = env->GetFieldID(targetClass, "height", "D");
}

extern "C" void processFrame(JNIEnv *env, int tex1, int tex2, int w, int h,
                             int mode, int h_min, int h_max, int s_min,
                             int s_max, int v_min, int v_max,
                             jobject destTargetInfo)
{
    auto targets = processImplAndroid(w, h, tex2, static_cast<DisplayMode>(mode),
                               h_min, h_max, s_min, s_max, v_min, v_max);

    int numTargets = targets.size();
    ensureJniRegistered(env);

    // write results to Java data structure
    env->SetIntField(destTargetInfo, sNumTargetsField, numTargets);
    if (numTargets == 0)
    {
        return;
    }
    jobjectArray targetsArray = static_cast<jobjectArray>(env->GetObjectField(destTargetInfo, sTargetsField));
    for (int i = 0; i < min(numTargets, 3); ++i)
    {
        jobject targetObject = env->GetObjectArrayElement(targetsArray, i);
        const auto &target = targets[i];
        env->SetDoubleField(targetObject, sCentroidXField, target.centroid_x);
        env->SetDoubleField(targetObject, sCentroidYField, target.centroid_y);
        env->SetDoubleField(targetObject, sWidthField, target.width);
        env->SetDoubleField(targetObject, sHeightField, target.height);
    }
}

void processImplAndroid(int texOut, int w, int h, int mode,
	int h_min, int h_max, int s_min, int s_max, int v_min, int v_max,
	cv::Mat &display, vector<TargetInfo> &targets)
{
	LOGD("Image is %d x %d", w, h);
	LOGD("H %d-%d S %d-%d V %d-%d", h_min, h_max, s_min, s_max, v_min, v_max);
	int64_t t;

	static cv::Mat input;
	input.create(h, w, CV_8UC4);

	//-----------------------------------
	// Read image into array
	//-----------------------------------
	t = getTimeMs();
	glReadPixels(0, 0, w, h, GL_RGBA, GL_UNSIGNED_BYTE, input.data);
	LOGD("glReadPixels() costs %d ms", getTimeInterval(t));

	processImpl(input, static_cast<DisplayMode>(mode), h_min, h_max, s_min, s_max, v_min, v_max, display, targets);

	displayImpl(mode, input, thresh, targets, rejected_targets);

	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, texOut);
	t = getTimeMs();
	glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, w, h, GL_RGBA, GL_UNSIGNED_BYTE, display.data);
	LOGD("glTexSubImage2D() costs %d ms", getTimeInterval(t));
}



#endif



#ifdef _MSC_VER

void processFrame(cv::Mat input, int mode,
	int h_min, int h_max, int s_min, int s_max, int v_min, int v_max)
{
	cv::Mat display, thresh;
	vector<vector<cv::Point>> contours, convex_contours, polys;
	vector<TargetInfo> targets, rejected_targets;

	processImpl(input, thresh, contours, convex_contours, polys, targets, rejected_targets,
		h_min, h_max, s_min, s_max, v_min, v_max);

	displayImpl((DisplayMode)mode, input, thresh, contours, convex_contours, polys, targets, rejected_targets, display);

	switch ((DisplayMode)mode)
	{
	case DISP_MODE_RAW:					cv::putText(display, "Raw",				cv::Point(10, 20), CV_FONT_HERSHEY_PLAIN, 1.0, CV_RGB(250, 250, 0));	break;
	case DISP_MODE_THRESH:				cv::putText(display, "Thresh",			cv::Point(10, 20), CV_FONT_HERSHEY_PLAIN, 1.0, CV_RGB(250, 250, 0));	break;
	case DISP_MODE_CONTOURS:			cv::putText(display, "Contours",		cv::Point(10, 20), CV_FONT_HERSHEY_PLAIN, 1.0, CV_RGB(250, 250, 0));	break;
	case DISP_MODE_CONVEX_CONTOURS:		cv::putText(display, "Convex Contours", cv::Point(10, 20), CV_FONT_HERSHEY_PLAIN, 1.0, CV_RGB(250, 250, 0));	break;
	case DISP_MODE_POLY:				cv::putText(display, "Poly",			cv::Point(10, 20), CV_FONT_HERSHEY_PLAIN, 1.0, CV_RGB(250, 250, 0));	break;
	case DISP_MODE_TARGETS:				cv::putText(display, "Targets",			cv::Point(10, 20), CV_FONT_HERSHEY_PLAIN, 1.0, CV_RGB(250, 250, 0));	break;
	case DISP_MODE_TARGETS_PLUS:		cv::putText(display, "Targets Plus",	cv::Point(10, 20), CV_FONT_HERSHEY_PLAIN, 1.0, CV_RGB(250, 250, 0));	break;
	}
	cv::imshow("DroidVision", display);

	LOGD("---------------------------------------------");

}

#endif