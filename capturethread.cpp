#include "capturethread.h"

//#include "qtgigecam.h"


bool CaptureThread::startCapture(Camera *camera)
{
	if (isRunning())
		return false;

	if (!camera || !camera->open())
		return false;

	m_camera = camera;
	m_stop = false;

	if (!m_camera->startCapture())
		return false;

	start(QThread::TimeCriticalPriority);

	return true;
}

void CaptureThread::stopCapture()
{    
	m_stop = true;
}

void CaptureThread::run()
{
	Mat frame;

    while (!m_stop) {
		if (!m_camera->getNextFrame(&frame)) {
			msleep(10);
			continue;
		}

		emit newImage(frame);
    }

	m_camera->stopCapture();
}
