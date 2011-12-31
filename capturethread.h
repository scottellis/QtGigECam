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

	bool startCapture(Camera *camera, MIL_ID buff_id);
    void stopCapture();

signals:
	void newImage(MIL_ID buff_id);

protected:
    void run();

private:
	Camera *m_camera;
	MIL_ID m_buff;
    volatile bool m_stop;
};

#endif // CAPTURETHREAD_H
