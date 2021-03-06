/*
 * This is an example that demonstrates how to connect to the EyeX Engine and subscribe to the lightly filtered gaze data stream.
 *
 * Copyright 2013-2014 Tobii Technology AB. All rights reserved.
 */

#include <Windows.h>
#include <stdio.h>
#include <tchar.h> 
#include <strsafe.h>
#include <conio.h>
#include <assert.h>
#include "eyex\EyeX.h"
#include <chrono>
#include <thread>

#include "opencv2\core\core.hpp"
#include "opencv2\highgui\highgui.hpp"
#include "opencv2\imgproc\imgproc.hpp"
#include "opencv2\photo\photo.hpp"

#include <iostream>
#include <ctime>

using namespace std;

#pragma comment (lib, "Tobii.EyeX.Client.lib")

// ID of the global interactor that provides our data stream; must be unique within the application.
static const TX_STRING InteractorId = "Twilight Sparkle";

// global variables
static TX_HANDLE g_hGlobalInteractorSnapshot = TX_EMPTY_HANDLE;
cv::Mat image0;
cv::Mat imageMask;
cv::Mat imageProjector;
cv::Mat blurCircle;
int numImages = 1;
int curImage = 0;
bool doScreenSaver = false;

std::vector<cv::Point> lastTrail;


/**
* ANNIELAURIE CHANGE THE BLUR_AMOUNT TO BE AMOUNT OF BLUR ON SCREEN SAVER IMAGE. MAKE SURE ITS AN ODD NUMBER!!!
*/
int BLUR_AMOUNT = 51;

/**
* ANNIELAURIE CHANGE THE CHANGE_IMAGE_TIMER TO BE NUMBER OF SECONDS TO CHANGE TO NEXT IMAGE
*/
int SCREEN_SAVER_RESET_TIMER = 3;

/**
* ANNIELAURIE CHANGE THE CHANGE_IMAGE_TIMER TO BE NUMBER OF SECONDS TO CHANGE TO NEXT IMAGE
*/
int CHANGE_IMAGE_TIMER = 2;

/**
* ANNIELAURIE CHANGE THE CIRCLE_SIZE TO BE THE SIZE OF CIRCLE THAT IS VISIBLE
*/
int CIRCLE_SIZE = 100;

/**
* ANNIELAURIE CHANGE THE BOUNDING_BOX TO BE THE WIGGLE ROOM BEFORE IMAGE CHANGE
*/
int BOUNDING_BOX = 20;

// blacks out the imageMask Mat
void blackOutMask(){
	imageMask.setTo(cv::Scalar(0,0,0));
}

void resetProjector(){
	imageProjector.setTo(cv::Scalar(0,0,0));
}

void changeImage(){
	HANDLE hFind;
	WIN32_FIND_DATA search_data;
	memset(&search_data, 0, sizeof(WIN32_FIND_DATA));
	TCHAR imgDir[200];
	StringCchCopy(imgDir, MAX_PATH, TEXT("..\\images\\data*.jpg"));
	HANDLE handle = FindFirstFile(imgDir, &search_data);

	numImages = 0;
	while(handle != INVALID_HANDLE_VALUE)
	{
		//printf("Found file: %s\r\n", search_data.cFileName);
		numImages++;
		if(FindNextFile(handle, &search_data) == FALSE)
		break;
	}

	char path[200];
	curImage++;
	if( curImage > numImages )
		curImage = 1;
	sprintf_s(path, 200 ,"..\\images\\data (%d).jpg", curImage);
	image0 = cv::imread(path, 1);
	
	if( !image0.data )
	{
		cout <<  "Could not open or find the image" << std::endl ;
		return;
	}
	
	cv::resize(image0, image0, cvSize(1920,1080), 0, 0, 1);

	//image0.convertTo(image0, CV_8UC4, 1.0,0.0);

	lastTrail.clear();
	imageMask = image0.clone();
	imageProjector = image0.clone();

	resetProjector();
	blackOutMask();
}

/*
 * Initializes g_hGlobalInteractorSnapshot with an interactor that has the Gaze Point behavior.
 */
BOOL InitializeGlobalInteractorSnapshot(TX_CONTEXTHANDLE hContext)
{
	TX_HANDLE hInteractor = TX_EMPTY_HANDLE;
	TX_HANDLE hBehavior   = TX_EMPTY_HANDLE;
	TX_GAZEPOINTDATAPARAMS params = { TX_GAZEPOINTDATAMODE_LIGHTLYFILTERED };
	BOOL success;

	success = txCreateGlobalInteractorSnapshot(
		hContext,
		InteractorId,
		&g_hGlobalInteractorSnapshot,
		&hInteractor) == TX_RESULT_OK;
	success &= txCreateInteractorBehavior(hInteractor, &hBehavior, TX_INTERACTIONBEHAVIORTYPE_GAZEPOINTDATA) == TX_RESULT_OK;
	success &= txSetGazePointDataBehaviorParams(hBehavior, &params) == TX_RESULT_OK;

	txReleaseObject(&hBehavior);
	txReleaseObject(&hInteractor);

	return success;
}

/*
 * Callback function invoked when a snapshot has been committed.
 */
void TX_CALLCONVENTION OnSnapshotCommitted(TX_CONSTHANDLE hAsyncData, TX_USERPARAM param)
{
	// check the result code using an assertion.
	// this will catch validation errors and runtime errors in debug builds. in release builds it won't do anything.

	TX_RESULT result = TX_RESULT_UNKNOWN;
	txGetAsyncDataResultCode(hAsyncData, &result);
	assert(result == TX_RESULT_OK || result == TX_RESULT_CANCELLED);
}

/*
 * Callback function invoked when the status of the connection to the EyeX Engine has changed.
 */
void TX_CALLCONVENTION OnEngineConnectionStateChanged(TX_CONNECTIONSTATE connectionState, TX_USERPARAM userParam)
{
	switch (connectionState) {
	case TX_CONNECTIONSTATE_CONNECTED: {
			BOOL success;
			printf("The connection state is now CONNECTED (We are connected to the EyeX Engine)\n");
			// commit the snapshot with the global interactor as soon as the connection to the engine is established.
			// (it cannot be done earlier because committing means "send to the engine".)
			success = txCommitSnapshotAsync(g_hGlobalInteractorSnapshot, OnSnapshotCommitted, NULL) == TX_RESULT_OK;
			if (!success) {
				printf("Failed to initialize the data stream.\n");
			}
			else
			{
				printf("Waiting for gaze data to start streaming...\n");
			}
		}
		break;

	case TX_CONNECTIONSTATE_DISCONNECTED:
		printf("The connection state is now DISCONNECTED (We are disconnected from the EyeX Engine)\n");
		break;

	case TX_CONNECTIONSTATE_TRYINGTOCONNECT:
		printf("The connection state is now TRYINGTOCONNECT (We are trying to connect to the EyeX Engine)\n");
		break;

	case TX_CONNECTIONSTATE_SERVERVERSIONTOOLOW:
		printf("The connection state is now SERVER_VERSION_TOO_LOW: this application requires a more recent version of the EyeX Engine to run.\n");
		break;

	case TX_CONNECTIONSTATE_SERVERVERSIONTOOHIGH:
		printf("The connection state is now SERVER_VERSION_TOO_HIGH: this application requires an older version of the EyeX Engine to run.\n");
		break;
	}
}

void overlayImage(const cv::Mat &background, const cv::Mat &foreground,
    cv::Mat &output, cv::Point2i location)
{
  background.copyTo(output);


  // start at the row indicated by location, or at row 0 if location.y is negative.
  for(int y = std::max(location.y , 0); y < background.rows; ++y)
  {
    int fY = y - location.y; // because of the translation

    // we are done of we have processed all rows of the foreground image.
    if(fY >= foreground.rows)
      break;

    // start at the column indicated by location,

    // or at column 0 if location.x is negative.
    for(int x = std::max(location.x, 0); x < background.cols; ++x)
    {
      int fX = x - location.x; // because of the translation.

      // we are done with this row if the column is outside of the foreground image.
      if(fX >= foreground.cols)
        break;

      // determine the opacity of the foregrond pixel, using its fourth (alpha) channel.
      double opacity =
        ((double)foreground.data[fY * foreground.step + fX * foreground.channels() + 3])

        / 255.;


      // and now combine the background and foreground pixel, using the opacity,

      // but only if opacity > 0.
      for(int c = 0; opacity > 0 && c < output.channels(); ++c)
      {
        unsigned char foregroundPx =
          foreground.data[fY * foreground.step + fX * foreground.channels() + c];
        unsigned char backgroundPx =
          background.data[y * background.step + x * background.channels() + c];
        output.data[y*output.step + output.channels()*x + c] =
          backgroundPx * (1.-opacity) + foregroundPx * opacity;
      }
    }
  }
}

void drawCircle(cv::Point weightedAvgPoint, bool projectorOnly=false){
  blackOutMask();

  // display image
  cv::Mat img0;
  cv::Mat imgP;


  // draw circle for where eye is

  if( !projectorOnly)
	cv::circle(imageMask, weightedAvgPoint, CIRCLE_SIZE, cvScalar(255,255,255, 255), -1, 8, 0);
  cv::circle(imageProjector, weightedAvgPoint, CIRCLE_SIZE, cvScalar(255,255,255), -1, 8, 0);

  if( !projectorOnly)
	image0.copyTo(img0, imageMask);
  image0.copyTo(imgP, imageProjector);

  if( !projectorOnly)
	overlayImage(img0, blurCircle, img0, cv::Point(weightedAvgPoint.x-CIRCLE_SIZE, weightedAvgPoint.y-CIRCLE_SIZE));
  //overlayImage(imgP, blurCircle, imageProjector, cv::Point(weightedAvgPoint.x-CIRCLE_SIZE, weightedAvgPoint.y-CIRCLE_SIZE));

  if( !projectorOnly)
	cv::imshow("window", img0);
  else{
	cv::blur(image0,img0,cvSize(BLUR_AMOUNT,BLUR_AMOUNT),cvPoint(-1,-1),4);
	cv::imshow("window", img0);
  }
  cv::imshow("projector", imgP);

}


int shouldUpdateWindow = 0;
time_t window_timer = 0;
cv::Point lastPoint;
cv::Point weightedAvgPoint;
int shouldMove = 0;
time_t lastMove = 0;

int fps = 24;
int frames = 0;
time_t fps_timer = 0;
/*
 * Handles an event from the Gaze Point data stream.
 */
void OnGazeDataEvent(TX_HANDLE hGazeDataBehavior)
{
	TX_GAZEPOINTDATAEVENTPARAMS eventParams;
	if (txGetGazePointDataEventParams(hGazeDataBehavior, &eventParams) == TX_RESULT_OK) {
		time_t now;
		time(&now);
		frames++;
		if( difftime(now,fps_timer) >= 1 ){
			time(&fps_timer);
			fps = frames;
			frames = 0;
		}
		//printf("Gaze Data: (%.1f, %.1f) timestamp %.0f ms\n", eventParams.X, eventParams.Y, eventParams.Timestamp);
		if(!image0.data)
			return;
		if( difftime(now,window_timer) >= 2 ){
			time(&window_timer);
			doScreenSaver = false;
		}
		if(shouldUpdateWindow++ > 2) {
			//clear the things
			shouldUpdateWindow = 0;
			blackOutMask();

			// calculate weighted average for making the dot move smoother
			weightedAvgPoint.x *= 0.8;
			weightedAvgPoint.y *= 0.8;
			weightedAvgPoint.x += eventParams.X * 0.2;
			weightedAvgPoint.y += eventParams.Y * 0.2;

			drawCircle(weightedAvgPoint);
			lastTrail.push_back(weightedAvgPoint);
		}

		// checking for whether we should change the image
		if( abs(lastPoint.x - weightedAvgPoint.x) <= BOUNDING_BOX && abs(lastPoint.y - weightedAvgPoint.y) <= BOUNDING_BOX  ){
			shouldMove ++;
		} else {
			shouldMove = 0;
		}
		if ( shouldMove > CHANGE_IMAGE_TIMER * fps ) {
			shouldMove = 0;
			//if( difftime(now, lastMove) >  3) {
				time(&lastMove);
				changeImage();
			//}
		}
		lastPoint = weightedAvgPoint;
	} else {
		printf("Failed to interpret gaze data event packet.\n");
	}
}


/*
 * Callback function invoked when an event has been received from the EyeX Engine.
 */
void TX_CALLCONVENTION HandleEvent(TX_CONSTHANDLE hAsyncData, TX_USERPARAM userParam)
{
	TX_HANDLE hEvent = TX_EMPTY_HANDLE;
	TX_HANDLE hBehavior = TX_EMPTY_HANDLE;

    txGetAsyncDataContent(hAsyncData, &hEvent);

	// NOTE. Uncomment the following line of code to view the event object. The same function can be used with any interaction object.
	//OutputDebugStringA(txDebugObject(hEvent));

	if (txGetEventBehavior(hEvent, &hBehavior, TX_INTERACTIONBEHAVIORTYPE_GAZEPOINTDATA) == TX_RESULT_OK) {
		OnGazeDataEvent(hBehavior);
		txReleaseObject(&hBehavior);
	}

	// NOTE since this is a very simple application with a single interactor and a single data stream, 
	// our event handling code can be very simple too. A more complex application would typically have to 
	// check for multiple behaviors and route events based on interactor IDs.

	txReleaseObject(&hEvent);
}

int curPoint = 0;
void screenSave() {
  if(doScreenSaver) {
	if( curPoint < lastTrail.size() -1 && lastTrail.size() > 0){
		drawCircle((cv::Point)lastTrail[curPoint], true);
	}
    if( curPoint < lastTrail.size() -1 )
      curPoint++;
    else{
      curPoint = 0;
	  std::this_thread::sleep_for(std::chrono::milliseconds(SCREEN_SAVER_RESET_TIMER * 1000));
	  resetProjector();
	}
	
    std::this_thread::sleep_for(std::chrono::milliseconds(fps));
	screenSave();
  } 
  else {
	blackOutMask();
	resetProjector();
	if(lastTrail.size() > 10)
		lastTrail.clear();
	return;
  }
}

/*
 * Handles state changed notifications.
 */
void TX_CALLCONVENTION OnPresenceStateChanged(TX_CONSTHANDLE hAsyncData, TX_USERPARAM userParam)
{
	TX_RESULT result = TX_RESULT_UNKNOWN;
	TX_HANDLE hStateBag = TX_EMPTY_HANDLE;
	TX_BOOL success;
	TX_INTEGER presenceData;

	if (txGetAsyncDataResultCode(hAsyncData, &result) == TX_RESULT_OK && txGetAsyncDataContent(hAsyncData, &hStateBag) == TX_RESULT_OK)
	{		
		success = (txGetStateValueAsInteger(hStateBag, TX_STATEPATH_PRESENCEDATA, &presenceData) == TX_RESULT_OK);
		if (success)
		{
			printf("User is %s\n", presenceData == TX_PRESENCEDATA_PRESENT ? "present" : "NOT present");

			// if user is NOT present
			if( presenceData == TX_PRESENCEDATA_NOTPRESENT ) {
				resetProjector();
				doScreenSaver = true;
				screenSave();
			} else {
				// they ARE present
				doScreenSaver = false;
			}
		}
	}
}

/*
 * Application entry point.
 */
int main(int argc, char* argv[])
{
	time(&lastMove);
	
	cv::namedWindow( "projector", CV_WINDOW_FULLSCREEN | CV_GUI_NORMAL);
	cv::namedWindow( "window", CV_WINDOW_FULLSCREEN | CV_GUI_NORMAL);

	changeImage();

	blurCircle = cv::imread("..\\images\\blurCircle.png", -1);
	
	if( !blurCircle.data )
	{
		cout <<  "Could not open or find the image" << std::endl ;
		return -1;
	}
	
	cv::resize(blurCircle, blurCircle, cvSize(CIRCLE_SIZE*2, CIRCLE_SIZE*2), 0, 0, 1);


	cv::imshow( "window", imageMask );
	cv::moveWindow( "window", -10, 0 );

	cv::imshow( "projector", imageMask );
	cv::moveWindow( "projector", 1930, 0 );

	TX_CONTEXTHANDLE hContext = TX_EMPTY_HANDLE;
	TX_TICKET hConnectionStateChangedTicket = TX_INVALID_TICKET;
	TX_TICKET hEventHandlerTicket = TX_INVALID_TICKET;
	TX_TICKET hPresenceStateChangedTicket = TX_INVALID_TICKET;
	BOOL success;

	// initialize and enable the context that is our link to the EyeX Engine.
	success = txInitializeSystem(TX_SYSTEMCOMPONENTOVERRIDEFLAG_NONE, NULL, NULL, NULL) == TX_RESULT_OK;
	success &= txCreateContext(&hContext, TX_FALSE) == TX_RESULT_OK;
	success &= InitializeGlobalInteractorSnapshot(hContext);
	success &= txRegisterConnectionStateChangedHandler(hContext, &hConnectionStateChangedTicket, OnEngineConnectionStateChanged, NULL) == TX_RESULT_OK;
	success &= txRegisterEventHandler(hContext, &hEventHandlerTicket, HandleEvent, NULL) == TX_RESULT_OK;
	success &= txRegisterStateChangedHandler(hContext, &hPresenceStateChangedTicket, TX_STATEPATH_PRESENCEDATA, OnPresenceStateChanged, NULL) == TX_RESULT_OK;
	success &= txEnableConnection(hContext) == TX_RESULT_OK;

	// let the events flow until a key is pressed.
	if (success) {
		printf("Initialization was successful.\n");
	} else {
		printf("Initialization failed.\n");
	}
	printf("Press any key to exit...\n");
	cv::waitKey(0);
	//_getch();
	printf("Exiting.\n");

	// disable and delete the context.
	txDisableConnection(hContext);
	txReleaseObject(&g_hGlobalInteractorSnapshot);
	txShutdownContext(hContext, TX_CLEANUPTIMEOUT_DEFAULT, TX_FALSE);
	txReleaseContext(&hContext);

	cv::destroyWindow("window");
	cv::destroyWindow("projector");

	return 0;
}
