#ifndef CAPTURETHREAD_H
#define CAPTURETHREAD_H

#include <QThread>

#include "camera.h"

class CaptureThread : public QThread
{
    Q_OBJECT

public:
    //CaptureThread();
	//virtual ~CaptureThread();

	bool startCapture(Camera *camera);
    void stopCapture();

signals:
	void newImage(Mat frame);

protected:
    void run();

private:
	Camera *m_camera;
	//MIL_ID m_buff;
    volatile bool m_stop;
};

#endif // CAPTURETHREAD_H
