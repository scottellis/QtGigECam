#ifndef QTGIGECAM_H
#define QTGIGECAM_H

#include <mil.h>
#include <QtGui/QMainWindow>
#include "ui_qtgigecam.h"

#include "capturethread.h"


class QtGigECam : public QMainWindow
{
	Q_OBJECT

public:
	QtGigECam(QWidget *parent = 0, Qt::WFlags flags = 0);
	~QtGigECam();

	bool setGrabFrame(MIL_ID buf_id);
	MIL_ID getFreeBuffer();

public slots:
	void startVideo();
	void stopVideo();

protected:
	void timerEvent(QTimerEvent *event);

private:
	void showImage(MIL_ID buf_id);
	void prepareBuffers(int numBuffers);
	void freeBuffers();
	void prepareQueues();

	Ui::QtGigECamClass ui;

	CaptureThread *m_captureThread;
	
	QMutex m_frameQMutex;
	QQueue <MIL_ID> m_frameQueue;
	QMutex m_freeQMutex;
	QQueue <MIL_ID> m_freeQueue;

	int m_frameRateTimer;
	int m_frameRefreshTimer;
	QLabel *m_pStatus;
	int m_frameCount;
	int m_width;
	int m_height;
	int m_pitch;

	MIL_ID m_milApp;
};

#endif // QTGIGECAM_H
