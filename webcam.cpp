#include "webcam.h"

WebCam::WebCam()
{
	m_device = 0;
}

WebCam::~WebCam()
{
	close();
}

bool WebCam::open()
{
	if (m_vidcap.isOpened())
		return true;

    if (!m_vidcap.open(m_device))
		return false;

	return true;
}

void WebCam::close()
{
	if (m_vidcap.isOpened())
		m_vidcap.release();
}

bool WebCam::isOpen()
{
	return m_vidcap.isOpened();
}

bool WebCam::getNextFrame(MIL_ID buf_id)
{
	Mat frame;

	m_vidcap >> frame;	
			
	MbufPutColor(buf_id, M_PACKED + M_BGR24, M_ALL_BANDS, frame.data);

	return true;
}

// Always fixed using OpenCV interface to USB webcams
QSize WebCam::getImageSize()
{
	QSize sz(640, 480);

	return sz;
}