#ifndef WEBCAM_H
#define WEBCAM_H

#include "camera.h"

#include <opencv2/opencv.hpp>
using namespace cv;

class WebCam : public Camera
{
public:
	WebCam();
	virtual ~WebCam();

	virtual bool open();
	virtual void close();
	virtual bool isOpen();
	
	virtual bool getNextFrame(MIL_ID buf_id);

	virtual QSize getImageSize();

private:
	int m_device;
	VideoCapture m_vidcap;
};

#endif // WEBCAM_H
